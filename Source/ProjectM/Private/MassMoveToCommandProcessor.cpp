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

FVector GetSoldierOffsetFromSquadLeader(const int8 SquadMemberIndex, const FVector& SquadLeaderLocation, const FVector& SquadLeaderForward)
{
	const float ForwardToNextPointHeadingDegrees = FMath::RadiansToDegrees(UE::MassNavigation::GetYawFromDirection(SquadLeaderForward));
	FVector2D UnrotatedOffset = GSquadMemberOffsetsMeters[SquadMemberIndex] * 100.f * GSquadSpacingScalingFactor;
	UnrotatedOffset = FVector2D(UnrotatedOffset.Y, UnrotatedOffset.X); // For some reason we need to flip the X and Y coordinates to correctly rotate below.
	const FVector2D RotatedOffset = UnrotatedOffset.GetRotated(ForwardToNextPointHeadingDegrees);
	return SquadLeaderLocation + FVector(RotatedOffset, 0.f);
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
		const TArray<FNavigationActionGroup>& SquadLeaderActionGroups = SquadLeaderActionList.Get()->ActionGroups;
		const FVector& SoldierLocation = SoldierTransformFragment.GetTransform().GetLocation();
		TArray<FNavigationActionGroup> SoldierActionGroups;
		for (const FNavigationActionGroup& SquadLeaderActionGroup : SquadLeaderActionGroups)
		{
			const FVector& SoldierMoveTarget = GetSoldierOffsetFromSquadLeader(MilitaryUnit->SquadMemberIndex, SquadLeaderActionGroup.Actions[0].TargetLocation, SquadLeaderActionGroup.Actions[0].Forward);
			const FVector& SoldierForward = (SoldierMoveTarget - SoldierLocation).GetSafeNormal();
			TArray<FNavigationAction> SoldierActions;
			SoldierActions.Add(FNavigationAction(SoldierLocation, SoldierForward, EMassMovementAction::Stand));
			SoldierActions.Add(FNavigationAction(SoldierMoveTarget, SoldierForward, EMassMovementAction::Move, true));
			const FVector& SoldierOffset = GetSoldierOffsetFromSquadLeader(MilitaryUnit->SquadMemberIndex, SquadLeaderAction.TargetLocation, SquadLeaderAction.Forward);
			SoldierActions.Add(FNavigationAction(SoldierOffset, SquadLeaderAction.Forward, SquadLeaderAction.Action, SquadLeaderAction.Action == EMassMovementAction::Move));
		}

		check(SquadLeaderActions.Num() + 2 == SoldierActions.Num());
		SoldierNavMeshMoveFragment.ActionList = MakeShareable(new FNavigationActionList(SoldierActions));
		SoldierNavMeshMoveFragment.CurrentActionGroupIndex = 0;
		SoldierNavMeshMoveFragment.ReachedActionGroupIndex = 0;
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
	TArray<FNavigationActionGroup> ActionGroups;
	ActionGroups.Reserve(PathPoints.Num() - 1);

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

bool ProcessEntity(const UMassMoveToCommandProcessor* Processor, const FTeamMemberFragment& TeamMemberFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, const FTransform& EntityTransform, const FMassEntityHandle &Entity, UNavigationSystemV1* NavSys, FMassNavMeshMoveFragment& NavMeshMoveFragment, const FMassExecutionContext& Context, const UMilitaryUnit* LastMoveToCommandMilitaryUnit, const UWorld* World, const float& NavMeshRadius, const UMassEntitySubsystem& EntitySubsystem)
{
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	check(MilitaryStructureSubsystem);
	UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);

	bool bSetSquadMemberPaths = false;
	const bool bIsSquadLeader = EntityUnit->IsSquadLeader();
	if (bIsSquadLeader)
	{
		NavMeshMoveFragment.Reset();
		NavMeshMoveFragment.SquadMemberIndex = 0;
		bSetSquadMemberPaths = true;
	}
	else if (IsSquadMember(EntityUnit))
	{
		return false; // return since squad leader will have set NavMeshMoveFragment on squad members
	}

	static constexpr float AgentHeight = 200.f; // TODO: Don't hard-code
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties(NavMeshRadius, AgentHeight), EntityLocation);

	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not get navigation data. Likely there is no NavMesh in level."));
		return false;
	}

	FNavLocation ClosestValidLocation;
	const FVector Extent(8000.f, 8000.f, 1000.f); // TODO: don't hard-code
	const bool bProjectResult = NavSys->ProjectPointToNavigation(LastMoveToCommandTarget, ClosestValidLocation, Extent, NavData);

	if (!bProjectResult)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find closest valid location to move to command location from %s to %s"), *EntityLocation.ToString(), *LastMoveToCommandTarget.ToString());
		return false;
	}

	const FPathFindingQuery Query(Processor, *NavData, EntityLocation, ClosestValidLocation.Location);
	const FPathFindingResult Result = NavSys->FindPathSync(Query);

	if (!Result.IsSuccessful())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find path to target. NavMeshRadius = %.0f, Start = %s, End = %s"), NavMeshRadius, *EntityLocation.ToString(), *ClosestValidLocation.Location.ToString());
		return false;
	}

	NavMeshMoveFragment.ActionList = CreateNavActionList(Result.Path);
	NavMeshMoveFragment.ActionsRemaining = NavMeshMoveFragment.ActionList.Get()->Actions.Num();

	if (bSetSquadMemberPaths)
	{
		const UMilitaryUnit* SquadMilitaryUnit = EntityUnit->Parent;
		SetSquadMemberPaths(SquadMilitaryUnit, EntitySubsystem, NavMeshMoveFragment.ActionList, Context);
	}

	Context.Defer().AddTag<FMassNeedsNavMeshMoveTag>(Entity);

	return true;
}

void UMassMoveToCommandProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassMoveToCommandProcessor_Execute");

	if (!MoveToCommandSubsystem)
	{
		return;
	}

	const FVector* LastMoveToCommandTarget = MoveToCommandSubsystem->GetLastMoveToCommandTarget();
	if (!LastMoveToCommandTarget)
	{
		return;
	}

	int32 NumEntitiesSetMoveTarget = 0;
	int32 NumEntitiesAttemptedSetMoveTarget = 0;

	const bool& IsLastMoveToCommandForTeam1 = MoveToCommandSubsystem->IsLastMoveToCommandForTeam1();
	const UMilitaryUnit* LastMoveToCommandMilitaryUnit = MoveToCommandSubsystem->GetLastMoveToCommandMilitaryUnit();

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
			FVector MoveToCommandTarget = *LastMoveToCommandTarget;
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

	UE_LOG(LogTemp, Log, TEXT("UMassMoveToCommandProcessor: Set move target to %d/%d entities."), NumEntitiesSetMoveTarget, NumEntitiesAttemptedSetMoveTarget);

	MoveToCommandSubsystem->ResetLastMoveToCommand();
}
