// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassNavMeshMoveProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include <MassMoveToCommandProcessor.h>
#include <MassTrackedVehicleOrientationProcessor.h>
#include "MassEntityView.h"

UMassNavMeshMoveProcessor::UMassNavMeshMoveProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UMassNavMeshMoveProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshMoveFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCommandableMovementSpeedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassNeedsNavMeshMoveTag>(EMassFragmentPresence::All);
}

const FMassNavMeshMoveFragment& GetNavMeshMoveFragmentForSoldier(const UMilitaryUnit* SoldierMilitaryUnit, const UMassEntitySubsystem& EntitySubsystem)
{
	FMassEntityView SoldierEntityView(EntitySubsystem, SoldierMilitaryUnit->GetMassEntityHandle());
	return SoldierEntityView.GetFragmentData<FMassNavMeshMoveFragment>();
}

bool DoesSoldierHaveSameActionsRemaining(int32 ActionsRemaining, const UMilitaryUnit* SoldierMilitaryUnit, const UMassEntitySubsystem& EntitySubsystem)
{
	const FMassNavMeshMoveFragment& NavMeshMoveFragment = GetNavMeshMoveFragmentForSoldier(SoldierMilitaryUnit, EntitySubsystem);
	return NavMeshMoveFragment.ActionsRemaining <= ActionsRemaining;
}

bool HaveAllSquadMembersReachedSameAction(int32 ActionsRemaining, const UMilitaryUnit* MilitaryUnit, const UMassEntitySubsystem& EntitySubsystem, UMilitaryUnit* UnitToIgnore)
{
	if (MilitaryUnit == UnitToIgnore)
	{
		return true;
	}

	if (MilitaryUnit->bIsSoldier)
	{
		if (!DoesSoldierHaveSameActionsRemaining(ActionsRemaining, MilitaryUnit, EntitySubsystem))
		{
			return false;
		}
	}
	
	for (UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
	{
		if (!HaveAllSquadMembersReachedSameAction(ActionsRemaining, SubUnit, EntitySubsystem, UnitToIgnore))
		{
			return false;
		}
	}

	return true;
}

void ProcessEntity(FMassMoveTargetFragment& MoveTargetFragment, UWorld* World, const FTransform& EntityTransform, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle& Entity, FMassNavMeshMoveFragment& NavMeshMoveFragment, const float MovementSpeed, const float AgentRadius, const UMassEntitySubsystem& EntitySubsystem)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();

	// TODO: would it be faster to have two separate entity queries instead of checking tag here?
	const bool& bUseStashedMoveTarget = Context.DoesArchetypeHaveTag<FMassTrackTargetTag>() || Context.DoesArchetypeHaveTag<FMassTrackSoundTag>();
	FMassMoveTargetFragment& MoveTargetFragmentToModify = bUseStashedMoveTarget ? StashedMoveTargetFragment : MoveTargetFragment;

	const TArray<FNavigationAction>& Actions = NavMeshMoveFragment.ActionList.Get()->Actions;
	const FNavigationAction& CurrentAction = Actions[NavMeshMoveFragment.CurrentActionIndex];

	if (!NavMeshMoveFragment.bIsWaitingOnSquadMates)
	{
		bool bFinishedCurrentAction = false;

		const float DistanceFromTarget = (EntityLocation - CurrentAction.TargetLocation).Size();
		if (CurrentAction.Action == EMassMovementAction::Stand)
		{
			bFinishedCurrentAction = IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward);
		}
		else
		{
			bFinishedCurrentAction = DistanceFromTarget < AgentRadius;
		}

		if (!bFinishedCurrentAction)
		{
			// Wait for UMassSteerToMoveTargetProcessor to move entity to next point.
			MoveTargetFragmentToModify.DistanceToGoal = DistanceFromTarget;
			return;
		}

		NavMeshMoveFragment.ActionsRemaining = NavMeshMoveFragment.ActionList.Get()->Actions.Num() - (NavMeshMoveFragment.CurrentActionIndex + 1);
	}

	if (NavMeshMoveFragment.IsSquadMember())
	{
		UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
		check(MilitaryStructureSubsystem);
		UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);
		if (!HaveAllSquadMembersReachedSameAction(NavMeshMoveFragment.ActionsRemaining, EntityUnit->SquadMilitaryUnit, EntitySubsystem, EntityUnit))
		{
			NavMeshMoveFragment.bIsWaitingOnSquadMates = true;
			return; // Wait for squad mates.
		}
		NavMeshMoveFragment.bIsWaitingOnSquadMates = false;
	}

	NavMeshMoveFragment.CurrentActionIndex++;

	const bool bFinishedNavMeshMove = NavMeshMoveFragment.CurrentActionIndex >= Actions.Num();
	if (bFinishedNavMeshMove)
	{
		MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Stand, *World);
		MoveTargetFragmentToModify.DistanceToGoal = 0.f;
		MoveTargetFragmentToModify.DesiredSpeed.Set(0.f);

		Context.Defer().RemoveTag<FMassNeedsNavMeshMoveTag>(Entity);

		if (bUseStashedMoveTarget) {
			Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
		}
		return;
	}

	// Update move target to next action.
	
	const FNavigationAction& NextAction = Actions[NavMeshMoveFragment.CurrentActionIndex];

	MoveTargetFragmentToModify.CreateNewAction(NextAction.Action, *World);
	MoveTargetFragmentToModify.Center = NextAction.TargetLocation;
	MoveTargetFragmentToModify.Forward = NextAction.Forward;
	const float Distance = (NextAction.TargetLocation - EntityLocation).Size();
	MoveTargetFragmentToModify.DistanceToGoal = Distance;
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(NextAction.Action == EMassMovementAction::Stand ? 0.f : MovementSpeed);
	MoveTargetFragmentToModify.IntentAtGoal = EMassMovementAction::Stand;

	if (bUseStashedMoveTarget) {
		Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
	}
}

void UMassNavMeshMoveProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassNavMeshMoveProcessor_Execute");

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassCommandableMovementSpeedFragment> MovementSpeedList = Context.GetFragmentView<FMassCommandableMovementSpeedFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetMutableFragmentView<FMassNavMeshMoveFragment>();
		const TConstArrayView<FAgentRadiusFragment> AgentRadiusList = Context.GetFragmentView<FAgentRadiusFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			ProcessEntity(MoveTargetList[i], GetWorld(), TransformList[i].GetTransform(), Context, StashedMoveTargetList[i], Context.GetEntity(i), NavMeshMoveList[i], MovementSpeedList[i].MovementSpeed, AgentRadiusList[i].Radius, EntitySubsystem);
		}
	});
}
