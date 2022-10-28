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

bool UMassNavMeshMoveProcessor_DrawPathState = false;
FAutoConsoleVariableRef CVarUMassNavMeshMoveProcessor_DrawPathState(TEXT("pm.UMassNavMeshMoveProcessor_DrawPathState"), UMassNavMeshMoveProcessor_DrawPathState, TEXT("UMassNavMeshMoveProcessor: Draw Path State"));

bool HasSoldierReachedNextNavPoint(int32 NextNavPointIndex, const UMilitaryUnit* SoldierMilitaryUnit, const UMassEntitySubsystem& EntitySubsystem)
{
	FMassEntityView SoldierEntityView(EntitySubsystem, SoldierMilitaryUnit->GetMassEntityHandle());
	const FMassNavMeshMoveFragment& NavMeshMoveFragment = SoldierEntityView.GetFragmentData<FMassNavMeshMoveFragment>();
	return NavMeshMoveFragment.ReachedPathPointIndex >= NextNavPointIndex;
}

bool HaveAllSquadMembersReachedNextNavPoint(int32 NextNavPointIndex, const UMilitaryUnit* MilitaryUnit, const UMassEntitySubsystem& EntitySubsystem, UMilitaryUnit* UnitToIgnore)
{
	if (MilitaryUnit == UnitToIgnore)
	{
		return true;
	}

	if (MilitaryUnit->bIsSoldier)
	{
		const bool bHasSoldierReachedNextNavPoint = HasSoldierReachedNextNavPoint(NextNavPointIndex, MilitaryUnit, EntitySubsystem);
		if (!bHasSoldierReachedNextNavPoint)
		{
			return false;
		}
	}
	
	for (UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
	{
		if (!HaveAllSquadMembersReachedNextNavPoint(NextNavPointIndex, SubUnit, EntitySubsystem, UnitToIgnore))
		{
			return false;
		}
	}

	return true;
}

void SetUpMoveTargetToPoint(FMassMoveTargetFragment& MoveTargetFragmentToModify, const FVector& CurrentMovePoint, UWorld* World, const float MovementSpeed, FMassNavMeshMoveFragment& NavMeshMoveFragment, const EMassMovementAction IntentAtGoal)
{
	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Move, *World);
	MoveTargetFragmentToModify.Center = CurrentMovePoint;
	MoveTargetFragmentToModify.DistanceToGoal = 0.f; // We can set to 0 since it'll get set in next tick.
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(MovementSpeed);
	NavMeshMoveFragment.ProgressDistance = 0.f;
}

void ProcessEntity(FMassMoveTargetFragment& MoveTargetFragment, UWorld *World, const FTransform& EntityTransform, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle& Entity, FMassNavMeshMoveFragment& NavMeshMoveFragment, const float MovementSpeed, const float AgentRadius, const UMassEntitySubsystem& EntitySubsystem)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const bool& bIsTrackedVehicle = Context.DoesArchetypeHaveTag<FMassTrackedVehicleOrientationTag>();

	// TODO: would it be faster to have two separate entity queries instead of checking tag here?
	const bool& bUseStashedMoveTarget = Context.DoesArchetypeHaveTag<FMassTrackTargetTag>() || Context.DoesArchetypeHaveTag<FMassTrackSoundTag>();
	FMassMoveTargetFragment& MoveTargetFragmentToModify = bUseStashedMoveTarget ? StashedMoveTargetFragment : MoveTargetFragment;

	const TArray<FNavPathPoint>& Points = NavMeshMoveFragment.Path.Get()->GetPathPoints();
	FVector CurrentMovePoint = Points[NavMeshMoveFragment.CurrentPathPointIndex].Location;

	// For tracked vehicles set initial move target forward to direction so they first turn if needed.
	if (NavMeshMoveFragment.CurrentPathPointIndex == 0 && bIsTrackedVehicle)
	{
		MoveTargetFragmentToModify.Forward = (Points[1].Location - EntityLocation).GetSafeNormal();
	}

	if (!MoveTargetFragmentToModify.bSteeringFallingBehind && !bUseStashedMoveTarget && MoveTargetFragmentToModify.GetCurrentAction() == EMassMovementAction::Move)
	{
		NavMeshMoveFragment.ProgressDistance += MoveTargetFragmentToModify.DesiredSpeed.Get() * Context.GetDeltaTimeSeconds();
	}

	const bool bIsStanding = MoveTargetFragmentToModify.GetCurrentAction() == EMassMovementAction::Stand;
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	check(MilitaryStructureSubsystem);
	UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);
	const bool bIsSquadMember = IsSquadMember(EntityUnit);
	if (bIsSquadMember && bIsStanding) // If waiting for squad mates to reach next move point.
	{
		const bool bAllSquadMembersAtNextMovePoint = HaveAllSquadMembersReachedNextNavPoint(NavMeshMoveFragment.ReachedPathPointIndex, EntityUnit->SquadMilitaryUnit, EntitySubsystem, EntityUnit);

		if (bAllSquadMembersAtNextMovePoint)
		{
			const bool bIsLastMoveTarget = NavMeshMoveFragment.CurrentPathPointIndex + 1 >= Points.Num();
			const EMassMovementAction IntentAtGoal = (bIsLastMoveTarget || bIsTrackedVehicle) ? EMassMovementAction::Stand : EMassMovementAction::Move;
			SetUpMoveTargetToPoint(MoveTargetFragmentToModify, CurrentMovePoint, World, MovementSpeed, NavMeshMoveFragment, IntentAtGoal);
			if (bUseStashedMoveTarget) {
				Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
			}
		}

		return;
	}

	bool bNeedsNextMovePoint = false;
	if (NavMeshMoveFragment.CurrentPathPointIndex == 0)
	{
		bNeedsNextMovePoint = true;
	}
	else
	{
		const FVector& PreviousMovePoint = Points[NavMeshMoveFragment.CurrentPathPointIndex - 1].Location;
		const float LengthOfCurrentLine = (CurrentMovePoint - PreviousMovePoint).Size();
		const FVector& CurrentTangent = (CurrentMovePoint - PreviousMovePoint).GetSafeNormal();
		if (NavMeshMoveFragment.ProgressDistance <= 0.0f)
		{
			MoveTargetFragmentToModify.Center = CurrentMovePoint;
			MoveTargetFragmentToModify.Forward = CurrentTangent;
			MoveTargetFragmentToModify.DistanceToGoal = LengthOfCurrentLine;
			MoveTargetFragmentToModify.bOffBoundaries = true;
		}
		else if (NavMeshMoveFragment.ProgressDistance <= LengthOfCurrentLine)
		{
			const FVector& NextMovePoint = NavMeshMoveFragment.CurrentPathPointIndex + 1 < Points.Num() ? Points[NavMeshMoveFragment.CurrentPathPointIndex + 1].Location : Points.Last().Location;
			const float PercentMoveTargetCompleted = LengthOfCurrentLine > 0.f ? NavMeshMoveFragment.ProgressDistance / LengthOfCurrentLine : 1.f;
			MoveTargetFragmentToModify.Center = FMath::Lerp(PreviousMovePoint, CurrentMovePoint, PercentMoveTargetCompleted);
			const FVector& NextTangent = (NextMovePoint - CurrentMovePoint).GetSafeNormal();
			MoveTargetFragmentToModify.Forward = FMath::Lerp(CurrentTangent, NextTangent, PercentMoveTargetCompleted).GetSafeNormal();
			MoveTargetFragmentToModify.DistanceToGoal = LengthOfCurrentLine * (1 - PercentMoveTargetCompleted);
		}
		else // progress > 100%
		{
			bNeedsNextMovePoint = true;
		}
	}

	if (bNeedsNextMovePoint)
	{
		NavMeshMoveFragment.ReachedPathPointIndex = NavMeshMoveFragment.CurrentPathPointIndex;
		NavMeshMoveFragment.CurrentPathPointIndex++;

		const bool bFinishedNavMeshMove = NavMeshMoveFragment.CurrentPathPointIndex >= Points.Num();
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

		// We are at the next move point but we haven't reached the final move point, so update FMassMoveTargetFragment.

		bool bAllSquadMembersAtNextMovePoint = true;

		if (bIsSquadMember)
		{
			bAllSquadMembersAtNextMovePoint = HaveAllSquadMembersReachedNextNavPoint(NavMeshMoveFragment.ReachedPathPointIndex, EntityUnit->SquadMilitaryUnit, EntitySubsystem, EntityUnit);
		}

		if (bAllSquadMembersAtNextMovePoint)
		{
			const bool bIsLastMoveTarget = NavMeshMoveFragment.CurrentPathPointIndex + 1 >= Points.Num();
			const EMassMovementAction IntentAtGoal = (bIsLastMoveTarget || bIsTrackedVehicle) ? EMassMovementAction::Stand : EMassMovementAction::Move;
			SetUpMoveTargetToPoint(MoveTargetFragmentToModify, CurrentMovePoint, World, MovementSpeed, NavMeshMoveFragment, IntentAtGoal);
		}
		else // Stand and wait for squad mates.
		{
			MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Stand, *World);
			MoveTargetFragmentToModify.DistanceToGoal = 0.f;
			MoveTargetFragmentToModify.DesiredSpeed.Set(0.f);
		}
	}

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
