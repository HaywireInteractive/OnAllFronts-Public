// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassNavMeshMoveProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include <MassMoveToCommandProcessor.h>

UMassNavMeshMoveProcessor::UMassNavMeshMoveProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassNavMeshMoveProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshMoveFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCommandableMovementSpeedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassNeedsNavMeshMoveTag>(EMassFragmentPresence::All);
}

void ProcessEntity(FMassMoveTargetFragment& MoveTargetFragment, UWorld *World, const FVector& EntityLocation, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle& Entity, FMassNavMeshMoveFragment& NavMeshMoveFragment, const float& MovementSpeed)
{
	FVector& NextMovePoint = NavMeshMoveFragment.Path.Get()->GetPathPoints()[NavMeshMoveFragment.CurrentPathPointIndex].Location;

	static const float FudgeFactor = 50.f; // TODO: don't hardcode
	bool bAtNextMovePoint = (EntityLocation - NextMovePoint).Size() < FudgeFactor;
	if (!bAtNextMovePoint)
	{
		// Wait for UMassSteerToMoveTargetProcessor to move entity to next point.
		return;
	}

	NavMeshMoveFragment.CurrentPathPointIndex++;

	bool bFinishedNavMeshMove = NavMeshMoveFragment.CurrentPathPointIndex >= NavMeshMoveFragment.Path.Get()->GetPathPoints().Num();
	if (bFinishedNavMeshMove)
	{
		Context.Defer().RemoveTag<FMassNeedsNavMeshMoveTag>(Entity);
		return;
	}

	// We are at the next move point but we haven't reached the final move point, so update FMassMoveTargetFragment.

	NextMovePoint = NavMeshMoveFragment.Path.Get()->GetPathPoints()[NavMeshMoveFragment.CurrentPathPointIndex].Location;

	// TODO: would it be faster to have two separate entity queries instead of checking tag here?
	const bool& bUseStashedMoveTarget = Context.DoesArchetypeHaveTag<FMassTrackTargetTag>();
	FMassMoveTargetFragment& MoveTargetFragmentToModify = bUseStashedMoveTarget ? StashedMoveTargetFragment : MoveTargetFragment;

	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Move, *World);
	MoveTargetFragmentToModify.Center = NextMovePoint;
	MoveTargetFragmentToModify.Forward = (NextMovePoint - EntityLocation).GetSafeNormal();
	float Distance = (EntityLocation - NextMovePoint).Size();
	MoveTargetFragmentToModify.DistanceToGoal = Distance;
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(MovementSpeed);
	MoveTargetFragmentToModify.IntentAtGoal = EMassMovementAction::Stand;

	if (bUseStashedMoveTarget) {
		Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
	}
}

void UMassNavMeshMoveProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassNavMeshMoveProcessor_Execute);

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassCommandableMovementSpeedFragment> MovementSpeedList = Context.GetFragmentView<FMassCommandableMovementSpeedFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetMutableFragmentView<FMassNavMeshMoveFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			ProcessEntity(MoveTargetList[i], GetWorld(), TransformList[i].GetTransform().GetLocation(), Context, StashedMoveTargetList[i], Context.GetEntity(i), NavMeshMoveList[i], MovementSpeedList[i].MovementSpeed);
		}
	});
}
