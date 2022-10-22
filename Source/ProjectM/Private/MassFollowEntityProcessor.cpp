// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassFollowEntityProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassNavMeshMoveProcessor.h"

UMassFollowEntityProcessor::UMassFollowEntityProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UMassFollowEntityProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshMoveFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassCommandableMovementSpeedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassFollowLeaderTag>(EMassFragmentPresence::All);
}

void ProcessEntity(FMassMoveTargetFragment& MoveTargetFragment, UWorld *World, const FTransform& EntityTransform, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle& Entity, const FMassNavMeshMoveFragment& NavMeshMoveFragment, const float& MovementSpeed, const UMassEntitySubsystem& EntitySubsystem)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();

	const bool bUseStashedMoveTarget = Context.DoesArchetypeHaveTag<FMassTrackTargetTag>() || Context.DoesArchetypeHaveTag<FMassTrackSoundTag>();
	FMassMoveTargetFragment& MoveTargetFragmentToModify = bUseStashedMoveTarget ? StashedMoveTargetFragment : MoveTargetFragment;

	if (!EntitySubsystem.IsEntityValid(NavMeshMoveFragment.EntityToFollow))
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassFollowEntityProcessor: Invalid EntityToFollow."));
		Context.Defer().RemoveTag<FMassFollowLeaderTag>(Entity);
		return;
	}

	FMassEntityView FollowEntityView(EntitySubsystem, NavMeshMoveFragment.EntityToFollow);
	const FTransformFragment& FollowEntityTransformFragment = FollowEntityView.GetFragmentData<FTransformFragment>();

	const FVector NextMovePoint = FollowEntityTransformFragment.GetTransform().GetLocation() + NavMeshMoveFragment.LeaderFollowDelta;

	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Move, *World);
	MoveTargetFragmentToModify.Center = NextMovePoint;
	MoveTargetFragmentToModify.Forward = (NextMovePoint - EntityLocation).GetSafeNormal();
	const float Distance = (EntityLocation - NextMovePoint).Size();
	MoveTargetFragmentToModify.DistanceToGoal = Distance;
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(MovementSpeed);
	const bool bIsLeaderMovingInNavMesh = FollowEntityView.HasTag<FMassNeedsNavMeshMoveTag>();
	MoveTargetFragmentToModify.IntentAtGoal = bIsLeaderMovingInNavMesh ? EMassMovementAction::Move : EMassMovementAction::Stand;

	if (!bIsLeaderMovingInNavMesh)
	{
		Context.Defer().RemoveTag<FMassFollowLeaderTag>(Entity);
	}
}

void UMassFollowEntityProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassFollowEntityProcessor.Execute);

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassCommandableMovementSpeedFragment> MovementSpeedList = Context.GetFragmentView<FMassCommandableMovementSpeedFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
		const TConstArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetMutableFragmentView<FMassNavMeshMoveFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			ProcessEntity(MoveTargetList[i], GetWorld(), TransformList[i].GetTransform(), Context, StashedMoveTargetList[i], Context.GetEntity(i), NavMeshMoveList[i], MovementSpeedList[i].MovementSpeed, EntitySubsystem);
		}
	});
}
