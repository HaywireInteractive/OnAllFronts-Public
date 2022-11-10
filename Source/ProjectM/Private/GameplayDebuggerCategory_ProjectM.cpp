// Adapted from GameplayDebuggerCategory_Mass.cpp.

#include "GameplayDebuggerCategory_ProjectM.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "MassAudioPerceptionProcessor.h"
#include <MassEnemyTargetFinderProcessor.h>
#include <MassNavMeshMoveProcessor.h>
#include <MassMoveToCommandProcessor.h>
#include "NavigationSystem.h"
#include <MassCommonFragments.h>
#include <GameplayDebuggerConfig.h>
#include "CanvasItem.h"

FGameplayDebuggerCategory_ProjectM::FGameplayDebuggerCategory_ProjectM()
{
  bShowOnlyWithDebugActor = false;

  BindKeyPress(EKeys::T.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_ProjectM::OnToggleEnemyTargetFinderDetails, EGameplayDebuggerInputMode::Replicated);
}

// Warning: This gets called on every tick before UMassProcessor::Execute.
void FGameplayDebuggerCategory_ProjectM::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	if (!OwnerPC)
	{
		return;
	}
	
	NearEntityDescriptions.Reset();

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	ensureMsgf(GetViewPoint(OwnerPC, ViewLocation, ViewDirection), TEXT("GetViewPoint is expected to always succeed when passing a valid controller."));

	CollectDataForEntities(OwnerPC, ViewLocation, ViewDirection);

	FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (DebugEntityData.IsEntitySearching && bShowEnemyTargetFinderDetails)
	{
		AddTextLine(FString::Printf(TEXT("Enemy Target Finder Legend:")));
		AddTextLine(FString::Printf(TEXT("{red}Red: Same team\n{yellow}Yellow: Blocked by another entity\n{orange}Orange: Impenetrable\n{blue}Blue: Out of range\n{black}Black: No line of sight")));

		AddShape(FGameplayDebuggerShape::MakeBox(DebugEntityData.SearchCenter, DebugEntityData.SearchExtent, FColor::Purple));

		DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToSameTeam, FColor::Red, DebugEntityData.EntityLocation);
		DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToOtherEntityBlocking, FColor::Yellow, DebugEntityData.EntityLocation);
		DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToImpenetrable, FColor::Orange, DebugEntityData.EntityLocation);
		DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToOutOfRange, FColor::Blue, DebugEntityData.EntityLocation);
		DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToNoLineOfSight, FColor::Black, DebugEntityData.EntityLocation);

		if (DebugEntityData.HasTargetEntity)
		{
			TArray<FVector> TargetEntityLocationArray;
			TargetEntityLocationArray.Add(DebugEntityData.TargetEntityLocation);
			DrawTargetEntityLocations(TargetEntityLocationArray, FColor::Green, DebugEntityData.EntityLocation);
		}
	}
	DebugEntityData.Reset();
}

void FGameplayDebuggerCategory_ProjectM::DrawTargetEntityLocations(const TArray<FVector>& TargetEntityLocations, const FColor& Color, const FVector& EntityLocation)
{
	const FVector ZOffset(0.f, 0.f, 200.f);
	for (const FVector& TargetEntityLocation : TargetEntityLocations)
	{
		AddShape(FGameplayDebuggerShape::MakeArrow(EntityLocation + ZOffset, TargetEntityLocation + ZOffset, 10.0f, 2.0f, Color));
		AddShape(FGameplayDebuggerShape::MakeCylinder(TargetEntityLocation + ZOffset / 2.f, 50.f, 100.0f, Color));
	}
}

void FGameplayDebuggerCategory_ProjectM::CollectDataForEntities(const APlayerController* OwnerPC, const FVector& ViewLocation, const FVector& ViewDirection)
{
	UWorld* World = GetDataWorld(OwnerPC, nullptr);

	UMassEntitySubsystem* EntitySystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySystem)
	{
		return;
	}

	FMassEntityQuery EntityQuery;
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassNavMeshMoveFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);

	FMassExecutionContext Context(0.0f);

	EntityQuery.ForEachEntityChunk(*EntitySystem, Context, [this, &ViewLocation, &ViewDirection, World, &EntitySystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetFragmentView<FMassNavMeshMoveFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();

		const UGameplayDebuggerUserSettings* Settings = GetDefault<UGameplayDebuggerUserSettings>();
		const float MaxViewDistance = Settings->MaxViewDistance;
		const float MinViewDirDot = FMath::Cos(FMath::DegreesToRadians(Settings->MaxViewAngle));

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			CollectDataForEntity(NavMeshMoveList[EntityIndex], TransformList[EntityIndex].GetTransform(), MinViewDirDot, ViewLocation, ViewDirection, MaxViewDistance, MoveTargetList[EntityIndex], RadiusList[EntityIndex].Radius, Context.GetEntity(EntityIndex), World, Context);
		}
	});
}

void FGameplayDebuggerCategory_ProjectM::CollectDataForEntity(const FMassNavMeshMoveFragment& NavMeshMoveFragment, const FTransform& Transform, const float MinViewDirDot, const FVector& ViewLocation, const FVector& ViewDirection, const float MaxViewDistance, const FMassMoveTargetFragment& MoveTargetFragment, const float AgentRadius, const FMassEntityHandle& Entity, const UWorld* World, FMassExecutionContext& Context)
{
	const bool bNeedsNavMeshMove = Context.DoesArchetypeHaveTag<FMassNeedsNavMeshMoveTag>();
	if (bNeedsNavMeshMove && UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		const TArray<FNavigationAction>& Actions = NavMeshMoveFragment.ActionList.Get()->Actions;
		int32 LineEndIndex = 1;
		int32 ActionIndex = 0;
		// Draw lines.
		for (const FNavigationAction& Action : Actions)
		{
			if (Action.Action == EMassMovementAction::Move)
			{
				const FNavigationAction& LineStartAction = Actions[ActionIndex - 1];
				const FColor& LineColor = ActionIndex < NavMeshMoveFragment.CurrentActionIndex ? FColor::Green : FColor::Red;
				AddShape(FGameplayDebuggerShape::MakeSegment(LineStartAction.TargetLocation, Action.TargetLocation, LineColor));
			}
			ActionIndex++;
		}
		ActionIndex = 0;
		// Draw points and forwards.
		for (const FNavigationAction& Action : Actions)
		{
			if (Action.Action == EMassMovementAction::Stand)
			{
				// Red = point not yet reached. Green = point already reached.
				const FColor& Color = ActionIndex < NavMeshMoveFragment.CurrentActionIndex ? FColor::Green : FColor::Red;
				const FVector StringLocation = Action.TargetLocation + FVector(0.f, 0.f, 50.f);
				AddShape(FGameplayDebuggerShape::MakePoint(Action.TargetLocation, 3.f, Color, FString::Printf(TEXT("{white}SMI %d, AI %d"), NavMeshMoveFragment.SquadMemberIndex, ActionIndex)));
				AddShape(FGameplayDebuggerShape::MakeArrow(Action.TargetLocation, Action.TargetLocation + Action.Forward * AgentRadius, 10.f, 2.f, FColor::Purple));
			}
			ActionIndex++;
		}
	}

	// Cull entity if needed
	const FVector& EntityLocation = Transform.GetLocation();
	const FVector DirToEntity = EntityLocation - ViewLocation;
	const float DistanceToEntitySq = DirToEntity.SquaredLength();
	if (DistanceToEntitySq > FMath::Square(MaxViewDistance))
	{
		return;
	}
	const float ViewDot = FVector::DotProduct(DirToEntity.GetSafeNormal(), ViewDirection);
	if (ViewDot < MinViewDirDot)
	{
		return;
	}

	if (DistanceToEntitySq < FMath::Square(MaxViewDistance * 0.5f))
	{
		AddShape(FGameplayDebuggerShape::MakeArrow(EntityLocation, MoveTargetFragment.Center, 10.f, 2.f, FColor::Black));

		UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
		check(MilitaryStructureSubsystem);
		UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);

		FString Status;
		const bool bIsSearching = Context.DoesArchetypeHaveTag<FMassNeedsEnemyTargetTag>();
		const bool bIsEngaging = Context.DoesArchetypeHaveTag<FMassWillNeedEnemyTargetTag>();
		FString ContactState = FString(bIsSearching ? TEXT("Searching") : bIsEngaging ? TEXT("Engaging") : TEXT(""));
		Status += FString::Printf(TEXT("{pink}ContactState: %s\n"), *ContactState);
		Status += FString::Printf(TEXT("{red}SquadIndex: %d\n{orange}SquadMemberIndex: %d\n"), EntityUnit->SquadIndex, NavMeshMoveFragment.SquadMemberIndex);
		if (bNeedsNavMeshMove)
		{
			Status += FString::Printf(TEXT("{white}CurrentActionIndex: %d\n{yellow}ActionsRemaining: %d\n{turquoise}ActionsNum: %d\n{cyan}IsWaitingOnSquadMates: %d"), NavMeshMoveFragment.CurrentActionIndex, NavMeshMoveFragment.ActionsRemaining, NavMeshMoveFragment.ActionList.Get()->Actions.Num(), NavMeshMoveFragment.bIsWaitingOnSquadMates);
		}

		FVector BasePos = EntityLocation + FVector(0.0f, 0.0f, 25.0f);
		constexpr float ViewWeight = 0.6f; // Higher the number the more the view angle affects the score.
		const float ViewScale = 1.f - (ViewDot / MinViewDirDot); // Zero at center of screen
		NearEntityDescriptions.Emplace(DistanceToEntitySq * ((1.0f - ViewWeight) + ViewScale * ViewWeight), BasePos, Status);
	}

	// Cap labels to closest ones.
	NearEntityDescriptions.Sort([](const FEntityDescription& LHS, const FEntityDescription& RHS) { return LHS.Score < RHS.Score; });
	constexpr int32 MaxLabels = 15;
	if (NearEntityDescriptions.Num() > MaxLabels)
	{
		NearEntityDescriptions.RemoveAt(MaxLabels, NearEntityDescriptions.Num() - MaxLabels);
	}
}

void FGameplayDebuggerCategory_ProjectM::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("\n[{yellow}%s{white}] %s Enemy Target Finder details"), *GetInputHandlerDescription(0), bShowEnemyTargetFinderDetails ? TEXT("Hide") : TEXT("Show"));


	const FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (DebugEntityData.IsEntitySearching)
	{
		const FVector2D EntityScreenLocation = CanvasContext.ProjectLocation(DebugEntityData.EntityLocation + FVector(0.f, 0.f, 300.f));
		CanvasContext.PrintAt(EntityScreenLocation.X, EntityScreenLocation.Y, FColor::Purple, 1.f, FString::Printf(TEXT("{purple}Number of close entities: %d"), DebugEntityData.NumCloseEntities));
	}

	struct FEntityLayoutRect
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		int32 Index = INDEX_NONE;
		float Alpha = 1.0f;
	};

	TArray<FEntityLayoutRect> Layout;

	// The loop below is O(N^2), make sure to keep the N small.
	constexpr int32 MaxDesc = 20;
	const int32 NumDescs = FMath::Min(NearEntityDescriptions.Num(), MaxDesc);

	// The labels are assumed to have been ordered in order of importance (i.e. front to back).
	for (int32 Index = 0; Index < NumDescs; Index++)
	{
		const FEntityDescription& Desc = NearEntityDescriptions[Index];
		if (Desc.Description.Len() && CanvasContext.IsLocationVisible(Desc.Location))
		{
			float SizeX = 0, SizeY = 0;
			const FVector2D ScreenLocation = CanvasContext.ProjectLocation(Desc.Location);
			CanvasContext.MeasureString(Desc.Description, SizeX, SizeY);

			FEntityLayoutRect Rect;
			Rect.Min = ScreenLocation + FVector2D(0, -SizeY * 0.5f);
			Rect.Max = Rect.Min + FVector2D(SizeX, SizeY);
			Rect.Index = Index;
			Rect.Alpha = 0.0f;

			// Calculate transparency based on how much more important rects are overlapping the new rect.
			const float Area = FMath::Max(0.0f, Rect.Max.X - Rect.Min.X) * FMath::Max(0.0f, Rect.Max.Y - Rect.Min.Y);
			const float InvArea = Area > KINDA_SMALL_NUMBER ? 1.0f / Area : 0.0f;
			float Coverage = 0.0;

			for (const FEntityLayoutRect& Other : Layout)
			{
				// Calculate rect intersection
				const float MinX = FMath::Max(Rect.Min.X, Other.Min.X);
				const float MinY = FMath::Max(Rect.Min.Y, Other.Min.Y);
				const float MaxX = FMath::Min(Rect.Max.X, Other.Max.X);
				const float MaxY = FMath::Min(Rect.Max.Y, Other.Max.Y);

				// return zero area if not overlapping
				const float IntersectingArea = FMath::Max(0.0f, MaxX - MinX) * FMath::Max(0.0f, MaxY - MinY);
				Coverage += (IntersectingArea * InvArea) * Other.Alpha;
			}

			Rect.Alpha = FMath::Square(1.0f - FMath::Min(Coverage, 1.0f));

			if (Rect.Alpha > KINDA_SMALL_NUMBER)
			{
				Layout.Add(Rect);
			}
		}
	}

	// Render back to front so that the most important item renders at top.
	const FVector2D Padding(5, 5);
	for (int32 Index = Layout.Num() - 1; Index >= 0; Index--)
	{
		const FEntityLayoutRect& Rect = Layout[Index];
		const FEntityDescription& Desc = NearEntityDescriptions[Rect.Index];

		const FVector2D BackgroundPosition(Rect.Min - Padding);
		FCanvasTileItem Background(Rect.Min - Padding, Rect.Max - Rect.Min + Padding * 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Rect.Alpha));
		Background.BlendMode = SE_BLEND_TranslucentAlphaOnly;
		CanvasContext.DrawItem(Background, BackgroundPosition.X, BackgroundPosition.Y);

		CanvasContext.PrintAt(Rect.Min.X, Rect.Min.Y, FColor::White, Rect.Alpha, Desc.Description);
	}

	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ProjectM::MakeInstance()
{
  return MakeShareable(new FGameplayDebuggerCategory_ProjectM());
}

#endif // WITH_GAMEPLAY_DEBUGGER