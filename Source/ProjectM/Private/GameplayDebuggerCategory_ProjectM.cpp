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
	
	CollectDataForNavMeshMoveProcessor(OwnerPC);

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

void FGameplayDebuggerCategory_ProjectM::CollectDataForNavMeshMoveProcessor(const APlayerController* OwnerPC)
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

	EntityQuery.ForEachEntityChunk(*EntitySystem, Context, [this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetFragmentView<FMassNavMeshMoveFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			DrawEntityInfo(NavMeshMoveList[EntityIndex], TransformList[EntityIndex].GetTransform());
		}
	});
}

void FGameplayDebuggerCategory_ProjectM::DrawEntityInfo(const FMassNavMeshMoveFragment& NavMeshMoveFragment, const FTransform& Transform)
{
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

	const FVector EntityDrawPoint = Transform.GetLocation() + FVector(0.f, 0.f, 200.f);
	AddShape(FGameplayDebuggerShape::MakePoint(EntityDrawPoint, 3.f, FColor::Blue, FString::Printf(TEXT("SI %d"), NavMeshMoveFragment.SquadMemberIndex)));
}

void FGameplayDebuggerCategory_ProjectM::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	const FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (!DebugEntityData.IsEntitySearching)
	{
		return;
	}

	const FVector2D EntityScreenLocation = CanvasContext.ProjectLocation(DebugEntityData.EntityLocation);
	CanvasContext.PrintAt(EntityScreenLocation.X, EntityScreenLocation.Y, FColor::Purple, 1.f, FString::Printf(TEXT("{Purple}Number of close entities: %d"), DebugEntityData.NumCloseEntities));

	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ProjectM::MakeInstance()
{
  return MakeShareable(new FGameplayDebuggerCategory_ProjectM());
}

#endif // WITH_GAMEPLAY_DEBUGGER