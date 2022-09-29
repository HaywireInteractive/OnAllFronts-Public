// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassNavMeshMoveProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include <MassMoveToCommandProcessor.h>
#include <MassTrackedVehicleOrientationProcessor.h>

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
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassNeedsNavMeshMoveTag>(EMassFragmentPresence::All);
}

bool UMassNavMeshMoveProcessor_DrawPathState = false;
FAutoConsoleVariableRef CVarUMassNavMeshMoveProcessor_DrawPathState(TEXT("pm.UMassNavMeshMoveProcessor_DrawPathState"), UMassNavMeshMoveProcessor_DrawPathState, TEXT("UMassNavMeshMoveProcessor: Draw Path State"));

void ProcessEntity(FMassMoveTargetFragment& MoveTargetFragment, UWorld *World, const FTransform& EntityTransform, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle& Entity, FMassNavMeshMoveFragment& NavMeshMoveFragment, const float& MovementSpeed, const float& AgentRadius)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const bool& bIsTrackedVehicle = Context.DoesArchetypeHaveTag<FMassTrackedVehicleOrientationTag>();

	// TODO: would it be faster to have two separate entity queries instead of checking tag here?
	const bool& bUseStashedMoveTarget = Context.DoesArchetypeHaveTag<FMassTrackTargetTag>() || Context.DoesArchetypeHaveTag<FMassTrackSoundTag>();
	FMassMoveTargetFragment& MoveTargetFragmentToModify = bUseStashedMoveTarget ? StashedMoveTargetFragment : MoveTargetFragment;

	const auto& Points = NavMeshMoveFragment.Path.Get()->GetPathPoints();
	FVector NextMovePoint = Points[NavMeshMoveFragment.CurrentPathPointIndex].Location;

	// For tracked vehicles set initial move target forward to direction so they first turn if needed.
	if (NavMeshMoveFragment.CurrentPathPointIndex == 0 && bIsTrackedVehicle)
	{
		MoveTargetFragmentToModify.Forward = (Points[1].Location - EntityLocation).GetSafeNormal();
	}

	const auto DistanceFromNextMovePoint = (EntityLocation - NextMovePoint).Size();
	MoveTargetFragmentToModify.DistanceToGoal = DistanceFromNextMovePoint;
	const bool bAtNextMovePoint = DistanceFromNextMovePoint < AgentRadius;
	const bool bFinishedNextMovePoint = bIsTrackedVehicle ? bAtNextMovePoint && IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward) : bAtNextMovePoint;
	if (!bFinishedNextMovePoint)
	{
		if (UMassNavMeshMoveProcessor_DrawPathState || UE::Mass::Debug::IsDebuggingEntity(Entity))
		{
			DrawDebugPoint(World, NextMovePoint, 10.f, FColor::Red, false, 1.f); // Red = Not at next point yet
		}

		// Wait for UMassSteerToMoveTargetProcessor to move entity to next point.
		return;
	}

	if (UMassNavMeshMoveProcessor_DrawPathState || UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		DrawDebugPoint(World, NextMovePoint, 10.f, FColor::Green, false, 1.f); // Green = Reached next point
	}

	NavMeshMoveFragment.CurrentPathPointIndex++;

	bool bFinishedNavMeshMove = NavMeshMoveFragment.CurrentPathPointIndex >= Points.Num();
	if (bFinishedNavMeshMove)
	{
		MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Stand, *World);
		MoveTargetFragmentToModify.DistanceToGoal = 0.f;
		MoveTargetFragmentToModify.DesiredSpeed.Set(0.f);

		if (UMassNavMeshMoveProcessor_DrawPathState || UE::Mass::Debug::IsDebuggingEntity(Entity))
		{
			DrawDebugPoint(World, NextMovePoint, 10.f, FColor::Blue, false, 1.f); // Blue = Reached last point
		}

		Context.Defer().RemoveTag<FMassNeedsNavMeshMoveTag>(Entity);
		return;
	}

	// We are at the next move point but we haven't reached the final move point, so update FMassMoveTargetFragment.
	NextMovePoint = Points[NavMeshMoveFragment.CurrentPathPointIndex].Location;

	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Move, *World);
	MoveTargetFragmentToModify.Center = NextMovePoint;
	MoveTargetFragmentToModify.Forward = (NextMovePoint - EntityLocation).GetSafeNormal();
	const float Distance = (EntityLocation - NextMovePoint).Size();
	MoveTargetFragmentToModify.DistanceToGoal = Distance;
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(MovementSpeed);
	const bool& bIsLastMoveTarget = NavMeshMoveFragment.CurrentPathPointIndex + 1 == Points.Num();
	MoveTargetFragmentToModify.IntentAtGoal = (bIsLastMoveTarget || bIsTrackedVehicle) ? EMassMovementAction::Stand : EMassMovementAction::Move;

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
			ProcessEntity(MoveTargetList[i], GetWorld(), TransformList[i].GetTransform(), Context, StashedMoveTargetList[i], Context.GetEntity(i), NavMeshMoveList[i], MovementSpeedList[i].MovementSpeed, AgentRadiusList[i].Radius);
		}
	});
}
