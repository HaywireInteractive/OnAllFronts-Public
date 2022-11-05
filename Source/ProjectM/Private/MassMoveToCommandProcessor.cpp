// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassMoveToCommandProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include <MassNavMeshMoveProcessor.h>
#include "MassEntityView.h"
#include <MassNavigationUtils.h>

//----------------------------------------------------------------------//
//  UMassCommandableTrait
//----------------------------------------------------------------------//
void UMassCommandableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FMassStashedMoveTargetFragment>();
	BuildContext.AddFragment<FMassNavMeshMoveFragment>();
	BuildContext.AddTag<FMassCommandableTag>();

	FMassCommandableMovementSpeedFragment& CommandableMovementSpeedTemplate = BuildContext.AddFragment_GetRef<FMassCommandableMovementSpeedFragment>();
	CommandableMovementSpeedTemplate.MovementSpeed = MovementSpeed;

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntitySubsystem);
	const FConstSharedStruct NavMeshParamsFragment = EntitySubsystem->GetOrCreateConstSharedFragment(UE::StructUtils::GetStructCrc32(FConstStructView::Make(NavMeshParams)), NavMeshParams);
	BuildContext.AddConstSharedFragment(NavMeshParamsFragment);
}

//----------------------------------------------------------------------//
//  UMassMoveToCommandProcessor
//----------------------------------------------------------------------//
UMassMoveToCommandProcessor::UMassMoveToCommandProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UMassMoveToCommandProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassNavMeshMoveFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassCommandableTag>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FNavMeshParamsFragment>(EMassFragmentPresence::All);
}

void UMassMoveToCommandProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	MoveToCommandSubsystem = UWorld::GetSubsystem<UMassMoveToCommandSubsystem>(Owner.GetWorld());
}

bool IsEntityCommandableByUnit(const FMassEntityHandle& Entity, const UMilitaryUnit* ParentUnit, const UWorld* World)
{
	// This is more for debugging in levels without a military unit spawner, to allow setting move to command to all soldiers on team.
	if (!ParentUnit)
	{
		return true;
	}

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	check(MilitaryStructureSubsystem);
	UMilitaryUnit* EntityUnit =	MilitaryStructureSubsystem->GetUnitForEntity(Entity);
	return EntityUnit->IsChildOfUnit(ParentUnit);
}

bool IsSquadMember(const UMilitaryUnit* MilitaryUnit)
{
	return MilitaryUnit->Depth > GSquadUnitDepth;
}

/*static*/ FVector2D UMassMoveToCommandProcessor::GetSoldierOffsetFromSquadLeaderUnscaledMeters(const int8 SquadMemberIndex, const FVector& SquadLeaderForward)
{
	const float YawRadians = UE::MassNavigation::GetYawFromDirection(SquadLeaderForward);
	float YawDegrees = FMath::RadiansToDegrees(YawRadians);
	// We subtract 90 because:
	// GetYawFromDirection((1,0)) = 0, should be 90 deg
	// GetYawFromDirection((0,1)) = 90 deg, should be 0 deg
	// Then because GetRotated rotates counter clockwise, we can leave as-is.
	YawDegrees -= 90.f;
	const FVector2D& UnrotatedOffset = GSquadMemberOffsetsMeters[SquadMemberIndex];
	const FVector2D& RotatedOffset = UnrotatedOffset.GetRotated(YawDegrees);
	return RotatedOffset;
}

/*static*/ FVector UMassMoveToCommandProcessor::GetSoldierOffsetFromSquadLeader(const int8 SquadMemberIndex, const FVector& SquadLeaderLocation, const FVector& SquadLeaderForward)
{
	const FVector2D& Offset = GetSoldierOffsetFromSquadLeaderUnscaledMeters(SquadMemberIndex, SquadLeaderForward) * 100.f * GSquadSpacingScalingFactor;
	return SquadLeaderLocation + FVector(Offset, 0.f);
}

void SetSquadMemberPaths(const UMilitaryUnit* MilitaryUnit, const UMassEntitySubsystem& EntitySubsystem, FNavActionListSharedPtr SquadLeaderActionList, const FMassExecutionContext& Context)
{
	const bool bIsSquadLeader = MilitaryUnit->SquadMemberIndex == 0;
	const bool bIsValidSquadMember = MilitaryUnit->SquadMemberIndex >= 0;
	if (MilitaryUnit->bIsSoldier && !bIsSquadLeader && bIsValidSquadMember)
	{
		FMassEntityView SoldierEntityView(EntitySubsystem, MilitaryUnit->GetMassEntityHandle());
		FMassNavMeshMoveFragment& SoldierNavMeshMoveFragment = SoldierEntityView.GetFragmentData<FMassNavMeshMoveFragment>();
		SoldierNavMeshMoveFragment.Reset();
		FTransformFragment& SoldierTransformFragment = SoldierEntityView.GetFragmentData<FTransformFragment>();
		const TArray<FNavigationAction>& SquadLeaderActions = SquadLeaderActionList.Get()->Actions;
		TArray<FNavigationAction> SquadMemberActions;
		const FVector& SoldierLocation = SoldierTransformFragment.GetTransform().GetLocation();
		const FVector& SoldierFirstMoveTarget = UMassMoveToCommandProcessor::GetSoldierOffsetFromSquadLeader(MilitaryUnit->SquadMemberIndex, SquadLeaderActions[0].TargetLocation, SquadLeaderActions[0].Forward);
		const FVector& SquadMemberFirstForward = (SoldierFirstMoveTarget - SoldierLocation).GetSafeNormal();
		SquadMemberActions.Add(FNavigationAction(SoldierLocation, SquadMemberFirstForward, EMassMovementAction::Stand));
		SquadMemberActions.Add(FNavigationAction(SoldierFirstMoveTarget, SquadMemberFirstForward, EMassMovementAction::Move));
		for (int32 i = 0; i < SquadLeaderActions.Num(); i++)
		{
			const FNavigationAction& SquadLeaderAction = SquadLeaderActions[i];
			const FNavigationAction& NextSquadLeaderAction = i + 1 < SquadLeaderActions.Num() ? SquadLeaderActions[i + 1] : SquadLeaderActions.Last();
			const FVector& Forward = SquadLeaderAction.Action == EMassMovementAction::Move ? NextSquadLeaderAction.Forward : SquadLeaderAction.Forward;
			const FVector& SoldierOffset = UMassMoveToCommandProcessor::GetSoldierOffsetFromSquadLeader(MilitaryUnit->SquadMemberIndex, SquadLeaderAction.TargetLocation, Forward);
			const FVector& SoldierForward = SquadLeaderAction.Action == EMassMovementAction::Move ? (SoldierOffset - SquadMemberActions.Last().TargetLocation).GetSafeNormal() : SquadLeaderAction.Forward;
			SquadMemberActions.Add(FNavigationAction(SoldierOffset, SoldierForward, SquadLeaderAction.Action));
		}

		check(SquadLeaderActions.Num() + 2 == SquadMemberActions.Num());
		SoldierNavMeshMoveFragment.ActionList = MakeShareable(new FNavigationActionList(SquadMemberActions));
		SoldierNavMeshMoveFragment.CurrentActionIndex = 0;
		SoldierNavMeshMoveFragment.ActionsRemaining = SquadMemberActions.Num();
		SoldierNavMeshMoveFragment.SquadMemberIndex = MilitaryUnit->SquadMemberIndex;

		Context.Defer().AddTag<FMassNeedsNavMeshMoveTag>(MilitaryUnit->GetMassEntityHandle());
	}
	for (const UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
	{
		SetSquadMemberPaths(SubUnit, EntitySubsystem, SquadLeaderActionList, Context);
	}
}

FNavActionListSharedPtr CreateNavActionList(FNavPathSharedPtr NavPath)
{
	const TArray<FNavPathPoint>& PathPoints = NavPath.Get()->GetPathPoints();
	TArray<FNavigationAction> Actions;
	Actions.Reserve(PathPoints.Num() * 2);

	// We skip the first point since it's where entity is currently located.
	for (int32 Index = 1; Index < PathPoints.Num(); Index++)
	{
		const FNavPathPoint& PathPoint = PathPoints[Index];
		const FNavPathPoint& PreviousPathPoint = PathPoints[Index - 1];
		FVector Forward = (PathPoint.Location - PreviousPathPoint.Location).GetSafeNormal();
		Actions.Add(FNavigationAction(PreviousPathPoint.Location, Forward, EMassMovementAction::Stand));
		Actions.Add(FNavigationAction(PathPoint.Location, Forward));
	}

	return MakeShareable(new FNavigationActionList(Actions));
}

bool UMassMoveToCommandProcessor_ProjectMoveToCommandTarget = false;
FAutoConsoleVariableRef CVar_UMassMoveToCommandProcessor_ProjectMoveToCommandTarget(TEXT("pm.UMassMoveToCommandProcessor_ProjectMoveToCommandTarget"), UMassMoveToCommandProcessor_ProjectMoveToCommandTarget, TEXT("UMassMoveToCommandProcessor_ProjectMoveToCommandTarget"));

bool ProcessEntity(const UMassMoveToCommandProcessor* Processor, const FTeamMemberFragment& TeamMemberFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, const FTransform& EntityTransform, const FMassEntityHandle &Entity, UNavigationSystemV1* NavSys, FMassNavMeshMoveFragment& NavMeshMoveFragment, const FMassExecutionContext& Context, const UMilitaryUnit* LastMoveToCommandMilitaryUnit, const UWorld* World, const float& NavMeshRadius, const UMassEntitySubsystem& EntitySubsystem)
{
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	check(MilitaryStructureSubsystem);
	UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);

	const bool bIsSquadLeader = EntityUnit->IsSquadLeader();
	if (bIsSquadLeader)
	{
		NavMeshMoveFragment.Reset();
		NavMeshMoveFragment.SquadMemberIndex = 0;
	}
	else if (IsSquadMember(EntityUnit))
	{
		return false; // return since squad leader will have set NavMeshMoveFragment on squad members
	}
	else {
		NavMeshMoveFragment.Reset();
		NavMeshMoveFragment.SquadMemberIndex = -1;
	}

	static constexpr float AgentHeight = 200.f; // TODO: Don't hard-code
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties(NavMeshRadius, AgentHeight), EntityLocation);

	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not get navigation data. Likely there is no NavMesh in level."));
		return false;
	}

	FVector CommandTarget = LastMoveToCommandTarget;

	if (UMassMoveToCommandProcessor_ProjectMoveToCommandTarget)
	{
		FNavLocation ClosestValidLocation;
		const FVector Extent(8000.f, 8000.f, 1000.f); // TODO: don't hard-code
		const bool bProjectResult = NavSys->ProjectPointToNavigation(LastMoveToCommandTarget, ClosestValidLocation, Extent, NavData);

		if (!bProjectResult)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find closest valid location to move to command location from %s to %s"), *EntityLocation.ToString(), *LastMoveToCommandTarget.ToString());
			return false;
		}

		const float ProjectedDeltaSquared = (ClosestValidLocation.Location - LastMoveToCommandTarget).SizeSquared();

		constexpr float MaxGoodProjectedDelta = 1000.f;
		constexpr float MaxGoodProjectedDeltaSquared = MaxGoodProjectedDelta * MaxGoodProjectedDelta;
		if (ProjectedDeltaSquared > MaxGoodProjectedDeltaSquared)
		{
			UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Projected point is far from original. Original = %s, Projected = %s, DeltaSquared = %.0f"), *LastMoveToCommandTarget.ToString(), *ClosestValidLocation.Location.ToString(), MaxGoodProjectedDeltaSquared);
		}

		CommandTarget = ClosestValidLocation.Location;
	}

	const FPathFindingQuery Query(Processor, *NavData, EntityLocation, CommandTarget);
	const FPathFindingResult Result = NavSys->FindPathSync(Query);

	if (!Result.IsSuccessful())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find path to target. NavMeshRadius = %.0f, Start = %s, End = %s"), NavMeshRadius, *EntityLocation.ToString(), *CommandTarget.ToString());
		return false;
	}

	NavMeshMoveFragment.ActionList = CreateNavActionList(Result.Path);
	NavMeshMoveFragment.ActionsRemaining = NavMeshMoveFragment.ActionList.Get()->Actions.Num();
	NavMeshMoveFragment.CurrentActionIndex = 0;

	if (bIsSquadLeader)
	{
		const UMilitaryUnit* SquadMilitaryUnit = EntityUnit->Parent;
		SetSquadMemberPaths(SquadMilitaryUnit, EntitySubsystem, NavMeshMoveFragment.ActionList, Context);
	}

	Context.Defer().AddTag<FMassNeedsNavMeshMoveTag>(Entity);

	return true;
}

void UMassMoveToCommandProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassMoveToCommandProcessor.Execute);

	if (!MoveToCommandSubsystem)
	{
		return;
	}

	FMoveToCommand MoveToCommand;
	if (!MoveToCommandSubsystem->DequeueMoveToCommand(MoveToCommand))
	{
		return;
	}

	int32 NumEntitiesSetMoveTarget = 0;
	int32 NumEntitiesAttemptedSetMoveTarget = 0;

	const bool IsLastMoveToCommandForTeam1 = MoveToCommand.bIsOnTeam1;
	const UMilitaryUnit* LastMoveToCommandMilitaryUnit = MoveToCommand.MilitaryUnit;
	const FVector LastMoveToCommandTarget = MoveToCommand.Target;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &IsLastMoveToCommandForTeam1, LastMoveToCommandTarget, NavSys, LastMoveToCommandMilitaryUnit, &NumEntitiesSetMoveTarget, &NumEntitiesAttemptedSetMoveTarget , &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetMutableFragmentView<FMassNavMeshMoveFragment>();
		const FNavMeshParamsFragment& NavMeshParams = Context.GetConstSharedFragment<FNavMeshParamsFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			// Change move to command target Z to make current entity's Z value since we don't know ground height.
			const FVector& EntityLocation = TransformList[i].GetTransform().GetLocation();
			FVector MoveToCommandTarget = LastMoveToCommandTarget;
			MoveToCommandTarget.Z = EntityLocation.Z;

			const FTeamMemberFragment& TeamMemberFragment = TeamMemberList[i];
			if (TeamMemberFragment.IsOnTeam1 != IsLastMoveToCommandForTeam1)
			{
				continue;
			}

			const FMassEntityHandle& Entity = Context.GetEntity(i);
			const UWorld* World = GetWorld();
			if (!IsEntityCommandableByUnit(Entity, LastMoveToCommandMilitaryUnit, World))
			{
				continue;
			}

			FMassNavMeshMoveFragment& NavMeshMoveFragment = NavMeshMoveList[i];
			const bool& bDidSetMoveTarget = ProcessEntity(this, TeamMemberList[i], IsLastMoveToCommandForTeam1, MoveToCommandTarget, TransformList[i].GetTransform(), Entity, NavSys, NavMeshMoveFragment, Context, LastMoveToCommandMilitaryUnit, World, NavMeshParams.NavMeshRadius, EntitySubsystem);

			// Skip squad members that aren't squad leaders.
			const bool bIsNonSquadLeaderSquadMember = NavMeshMoveFragment.SquadMemberIndex > 0;
			if (!bIsNonSquadLeaderSquadMember)
			{
				NumEntitiesAttemptedSetMoveTarget++;
			}
			if (bDidSetMoveTarget)
			{
				NumEntitiesSetMoveTarget++;
			}
		}
	});

	UE_LOG(LogTemp, Log, TEXT("UMassMoveToCommandProcessor: Set move target to %d/%d entities to %s."), NumEntitiesSetMoveTarget, NumEntitiesAttemptedSetMoveTarget, *LastMoveToCommandTarget.ToCompactString());
}
