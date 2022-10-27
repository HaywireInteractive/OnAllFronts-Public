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

void ProcessEntity(FMassMoveTargetFragment& MoveTargetFragment, UWorld *World, const FTransform& EntityTransform, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle& Entity, FMassNavMeshMoveFragment& NavMeshMoveFragment, const float& MovementSpeed, const float& AgentRadius, const UMassEntitySubsystem& EntitySubsystem)
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

	//const float DistanceFromNextMovePoint = (EntityLocation - CurrentMovePoint).Size();
	//const bool bAtNextMovePoint = DistanceFromNextMovePoint < AgentRadius;
	//bool bAllSquadMembersAtNextMovePoint = true;
	//if (bAtNextMovePoint)
	//{
	//	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Stand, *World);
	//	MoveTargetFragmentToModify.DistanceToGoal = 0.f;
	//	MoveTargetFragmentToModify.DesiredSpeed.Set(0.f);

	//	NavMeshMoveFragment.ReachedPathPointIndex = NavMeshMoveFragment.CurrentPathPointIndex;
	//	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	//	check(MilitaryStructureSubsystem);
	//	UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);
	//	if (IsSquadMember(EntityUnit))
	//	{
	//		bAllSquadMembersAtNextMovePoint = HaveAllSquadMembersReachedNextNavPoint(NavMeshMoveFragment.ReachedPathPointIndex, EntityUnit->SquadMilitaryUnit, EntitySubsystem, EntityUnit);
	//	}
	//}
	//const bool bFinishedNextMovePoint = bIsTrackedVehicle ? bAtNextMovePoint && IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward) : bAtNextMovePoint && bAllSquadMembersAtNextMovePoint;

	if (!MoveTargetFragmentToModify.bSteeringFallingBehind && !bUseStashedMoveTarget)
	{
		NavMeshMoveFragment.ProgressDistance += MoveTargetFragmentToModify.DesiredSpeed.Get() * Context.GetDeltaTimeSeconds();
	}

	bool bAtNextMovePoint = false;
	if (NavMeshMoveFragment.CurrentPathPointIndex == 0)
	{
		bAtNextMovePoint = true;
		/*const FVector& NextMovePoint = Points[NavMeshMoveFragment.CurrentPathPointIndex + 1].Location;
		MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Stand, *World);
		MoveTargetFragmentToModify.Center = CurrentMovePoint;
		MoveTargetFragmentToModify.Forward = (NextMovePoint - CurrentMovePoint).GetSafeNormal();
		MoveTargetFragmentToModify.DistanceToGoal = 0.f;
		MoveTargetFragmentToModify.bOffBoundaries = true;*/
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
			MoveTargetFragmentToModify.DistanceToGoal = LengthOfCurrentLine - FMath::Lerp(0.f, LengthOfCurrentLine, PercentMoveTargetCompleted);
		}
		else
		{
			bAtNextMovePoint = true;
		}
	}

	if (!bAtNextMovePoint)
	{
		return;
	}

	NavMeshMoveFragment.CurrentPathPointIndex++;

	const bool bFinishedNavMeshMove = NavMeshMoveFragment.CurrentPathPointIndex >= Points.Num();
	if (bFinishedNavMeshMove)
	{
		MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Stand, *World);
		MoveTargetFragmentToModify.DistanceToGoal = 0.f;
		MoveTargetFragmentToModify.DesiredSpeed.Set(0.f);

		Context.Defer().RemoveTag<FMassNeedsNavMeshMoveTag>(Entity);
		return;
	}

	// We are at the next move point but we haven't reached the final move point, so update FMassMoveTargetFragment.

	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Move, *World);
	MoveTargetFragmentToModify.Center = CurrentMovePoint;
	MoveTargetFragmentToModify.DistanceToGoal = 0.f;
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(MovementSpeed);
	const bool& bIsLastMoveTarget = NavMeshMoveFragment.CurrentPathPointIndex + 1 >= Points.Num();
	MoveTargetFragmentToModify.IntentAtGoal = (bIsLastMoveTarget || bIsTrackedVehicle) ? EMassMovementAction::Stand : EMassMovementAction::Move;
	NavMeshMoveFragment.ProgressDistance = 0.f;

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
