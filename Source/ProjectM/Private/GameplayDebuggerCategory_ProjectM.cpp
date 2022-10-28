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

	CollectDataForNavMeshMoveProcessor(OwnerPC, ViewLocation, ViewDirection);

	const FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (!DebugEntityData.IsEntitySearching)
	{
		return;
	}

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

void FGameplayDebuggerCategory_ProjectM::DrawTargetEntityLocations(const TArray<FVector>& TargetEntityLocations, const FColor& Color, const FVector& EntityLocation)
{
	const FVector ZOffset(0.f, 0.f, 200.f);
	for (const FVector& TargetEntityLocation : TargetEntityLocations)
	{
		AddShape(FGameplayDebuggerShape::MakeArrow(EntityLocation + ZOffset, TargetEntityLocation + ZOffset, 10.0f, 2.0f, Color));
		AddShape(FGameplayDebuggerShape::MakeCylinder(TargetEntityLocation + ZOffset / 2.f, 50.f, 100.0f, Color));
	}
}

void FGameplayDebuggerCategory_ProjectM::CollectDataForNavMeshMoveProcessor(const APlayerController* OwnerPC, const FVector& ViewLocation, const FVector& ViewDirection)
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
	EntityQuery.AddTagRequirement<FMassNeedsNavMeshMoveTag>(EMassFragmentPresence::All);

	FMassExecutionContext Context(0.0f);

	EntityQuery.ForEachEntityChunk(*EntitySystem, Context, [this, &ViewLocation, &ViewDirection](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetFragmentView<FMassNavMeshMoveFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

		const UGameplayDebuggerUserSettings* Settings = GetDefault<UGameplayDebuggerUserSettings>();
		const float MaxViewDistance = Settings->MaxViewDistance;
		const float MinViewDirDot = FMath::Cos(FMath::DegreesToRadians(Settings->MaxViewAngle));

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			DrawEntityInfo(NavMeshMoveList[EntityIndex], TransformList[EntityIndex].GetTransform(), MinViewDirDot, ViewLocation, ViewDirection, MaxViewDistance);
		}
	});
}

void FGameplayDebuggerCategory_ProjectM::DrawEntityInfo(const FMassNavMeshMoveFragment& NavMeshMoveFragment, const FTransform& Transform, const float MinViewDirDot, const FVector& ViewLocation, const FVector& ViewDirection, const float MaxViewDistance)
{
	const FVector& EntityLocation = Transform.GetLocation();

	// Cull entity if needed
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

	const TArray<FNavPathPoint>& Points = NavMeshMoveFragment.Path.Get()->GetPathPoints();
	int32 LineEndIndex = 1;
	int32 PointIndex = 0;
	int32 MaxIndex = 1;
	for (const FNavPathPoint& NavPathPoint : Points)
	{
		// Red = point not yet reached. Green = point already reached.
		// TODO:
		const FColor& Color = PointIndex < NavMeshMoveFragment.CurrentPathPointIndex ? FColor::Green : FColor::Red;
		//const FColor& Color = PointIndex == 0 ? FColor::Green : FColor::Red;
		const FVector StringLocation = NavPathPoint.Location + FVector(0.f, 0.f, 50.f);
		AddShape(FGameplayDebuggerShape::MakePoint(NavPathPoint.Location, 3.f, Color, FString::Printf(TEXT("PI %d, SI %d"), PointIndex, NavMeshMoveFragment.SquadMemberIndex)));
		if (LineEndIndex >= Points.Num())
		{
			break;
		}
		const FNavPathPoint& LineEndNavPathPoint = Points[LineEndIndex++];
		const FColor& LineColor = LineEndIndex < NavMeshMoveFragment.CurrentPathPointIndex ? FColor::Green : FColor::Red;

		//if (PointIndex >= 1 && NavMeshMoveFragment.SquadMemberIndex != 0)
		//{
		//	break; // TODO: temp
		//}
		AddShape(FGameplayDebuggerShape::MakeSegment(NavPathPoint.Location, LineEndNavPathPoint.Location, Color));
		PointIndex++;
	}

	if (DistanceToEntitySq < FMath::Square(MaxViewDistance * 0.5f))
	{
		FString Status;
		Status += FString::Printf(TEXT("{orange}Squad Member Index: %d {white}\nReached Point: %d\n"), NavMeshMoveFragment.SquadMemberIndex, NavMeshMoveFragment.ReachedPathPointIndex);
		Status += FString::Printf(TEXT("{pink}Progress Distance:  %.1f"), NavMeshMoveFragment.ProgressDistance);

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
	const FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (DebugEntityData.IsEntitySearching)
	{
		const FVector2D EntityScreenLocation = CanvasContext.ProjectLocation(DebugEntityData.EntityLocation);
		CanvasContext.PrintAt(EntityScreenLocation.X, EntityScreenLocation.Y, FColor::Purple, 1.f, FString::Printf(TEXT("{Purple}Number of close entities: %d"), DebugEntityData.NumCloseEntities));
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