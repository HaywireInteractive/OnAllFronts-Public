// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassMoveToCommandProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassCommonFragments.h"
#include "ZoneGraphSubsystem.h"
#include "MassZoneGraphNavigationTypes.h"
#include <MassZoneGraphNavigationUtils.h>
#include "MassZoneGraphNavigationFragments.h"
#include <Tasks/MassZoneGraphPathFollowTask.h>

//----------------------------------------------------------------------//
//  UMassCommandableTrait
//----------------------------------------------------------------------//
void UMassCommandableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FMassStashedMoveTargetFragment>();
}

//----------------------------------------------------------------------//
//  UMassMoveToCommandProcessor
//----------------------------------------------------------------------//
UMassMoveToCommandProcessor::UMassMoveToCommandProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassMoveToCommandProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassZoneGraphPathRequestFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassZoneGraphShortPathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassZoneGraphCachedLaneFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
}

void UMassMoveToCommandProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	MoveToCommandSubsystem = UWorld::GetSubsystem<UMassMoveToCommandSubsystem>(Owner.GetWorld());
}

bool RequestPath(const FMassZoneGraphTargetLocation& RequestedTargetLocation, FMassMoveTargetFragment& MoveTarget, FMassZoneGraphPathRequestFragment& RequestFragment, const FAgentRadiusFragment& AgentRadius, const UWorld* World, const FMassEntityHandle& Entity, const UZoneGraphSubsystem* ZoneGraphSubsystem, const FMassZoneGraphLaneLocationFragment& LaneLocation, FMassZoneGraphShortPathFragment& ShortPath, FMassZoneGraphCachedLaneFragment& CachedLane)
{
	FZoneGraphShortPathRequest& PathRequest = RequestFragment.PathRequest;
	PathRequest.StartPosition = MoveTarget.Center;
	PathRequest.bMoveReverse = RequestedTargetLocation.bMoveReverse;
	PathRequest.TargetDistance = RequestedTargetLocation.TargetDistance;
	PathRequest.NextLaneHandle = RequestedTargetLocation.NextLaneHandle;
	PathRequest.NextExitLinkType = RequestedTargetLocation.NextExitLinkType;
	PathRequest.EndOfPathIntent = RequestedTargetLocation.EndOfPathIntent;
	PathRequest.bIsEndOfPathPositionSet = RequestedTargetLocation.EndOfPathPosition.IsSet();
	PathRequest.EndOfPathPosition = RequestedTargetLocation.EndOfPathPosition.Get(FVector::ZeroVector);
	PathRequest.bIsEndOfPathDirectionSet = RequestedTargetLocation.EndOfPathDirection.IsSet();
	PathRequest.EndOfPathDirection.Set(RequestedTargetLocation.EndOfPathDirection.Get(FVector::ForwardVector));
	PathRequest.AnticipationDistance = RequestedTargetLocation.AnticipationDistance;
	PathRequest.EndOfPathOffset.Set(FMath::RandRange(-AgentRadius.Radius, AgentRadius.Radius));

	const float DesiredSpeed = 200.f; // TODO

	MoveTarget.CreateNewAction(EMassMovementAction::Move, *World);
	return UE::MassNavigation::ActivateActionMove(*World, World, Entity, *ZoneGraphSubsystem, LaneLocation, PathRequest, AgentRadius.Radius, DesiredSpeed, MoveTarget, ShortPath, CachedLane);
}

void ProcessEntity(const FTeamMemberFragment& TeamMemberFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, FMassMoveTargetFragment& MoveTarget, FMassZoneGraphPathRequestFragment& RequestFragment, const FAgentRadiusFragment& AgentRadius, const UWorld* World, const FMassEntityHandle& Entity, const UZoneGraphSubsystem* ZoneGraphSubsystem, const FMassZoneGraphLaneLocationFragment& LaneLocation, FMassZoneGraphShortPathFragment& ShortPath, FMassZoneGraphCachedLaneFragment& CachedLane)
{
	// TODO: Would it be faster to have team member be a tag so can use EntityQuery to filter?
	if (TeamMemberFragment.IsOnTeam1 != IsLastMoveToCommandForTeam1)
	{
		return;
	}

	// Find lane at destination.
	static const FVector SearchExtent = FVector(1000.f, 1000.f, 1000.f); // TODO: don't hardcode
	FBox QueryBounds = FBox(LastMoveToCommandTarget - SearchExtent, LastMoveToCommandTarget + SearchExtent);
	FZoneGraphLaneLocation OutGoalLaneLocation;
	float OutDistanceSqr = 0.0f;
	ZoneGraphSubsystem->FindNearestLane(QueryBounds, FZoneGraphTagFilter(), OutGoalLaneLocation, OutDistanceSqr);

	if (!OutGoalLaneLocation.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to find valid lane near move to command target."));
		return;
	}

	FMassZoneGraphTargetLocation TargetLocation;
	TargetLocation.TargetDistance = OutGoalLaneLocation.DistanceAlongLane;
	TargetLocation.EndOfPathPosition = LastMoveToCommandTarget;
	TargetLocation.AnticipationDistance.Set(50.0f); // TODO

	if (!RequestPath(TargetLocation, MoveTarget, RequestFragment, AgentRadius, World, Entity, ZoneGraphSubsystem, LaneLocation, ShortPath, CachedLane))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to find valid path to move to command target."));
	}
}

void UMassMoveToCommandProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassMoveToCommandProcessor_Execute);

	if (!MoveToCommandSubsystem)
	{
		return;
	}

	const FVector* LastMoveToCommandTarget = MoveToCommandSubsystem->GetLastMoveToCommandTarget();
	if (!LastMoveToCommandTarget)
	{
		return;
	}

	const bool& IsLastMoveToCommandForTeam1 = MoveToCommandSubsystem->IsLastMoveToCommandForTeam1();

	const UZoneGraphSubsystem* ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(GetWorld());

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem, &IsLastMoveToCommandForTeam1, LastMoveToCommandTarget = *LastMoveToCommandTarget, ZoneGraphSubsystem, World = GetWorld()](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> LaneLocationList = Context.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassZoneGraphPathRequestFragment> RequestList = Context.GetMutableFragmentView<FMassZoneGraphPathRequestFragment>();
		const TArrayView<FMassZoneGraphShortPathFragment> ShortPathList = Context.GetMutableFragmentView<FMassZoneGraphShortPathFragment>();
		const TArrayView<FMassZoneGraphCachedLaneFragment> CachedLaneList = Context.GetMutableFragmentView<FMassZoneGraphCachedLaneFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			// Change move to command target Z to make current entity's Z value since we don't know ground height.
			const FVector& EntityLocation = TransformList[i].GetTransform().GetLocation();
			FVector MoveToCommandTarget = LastMoveToCommandTarget;
			MoveToCommandTarget.Z = EntityLocation.Z;

			ProcessEntity(TeamMemberList[i], IsLastMoveToCommandForTeam1, MoveToCommandTarget, MoveTargetList[i], RequestList[i], RadiusList[i], World, Context.GetEntity(i), ZoneGraphSubsystem, LaneLocationList[i], ShortPathList[i], CachedLaneList[i]);
		}
	});

	MoveToCommandSubsystem->ResetLastMoveToCommand();
}
