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

// TODO: Move the data needed to compute this into Mass fragment so we don't need inefficient RAM hits.
bool IsSquadLeader(const UMilitaryUnit* MilitaryUnit)
{
	return MilitaryUnit->bIsCommander && MilitaryUnit->Depth == GSquadUnitDepth + 1;
}

bool IsSquadMember(const UMilitaryUnit* MilitaryUnit)
{
	return MilitaryUnit->Depth > GSquadUnitDepth;
}

void SetSquadMemberPaths(const UMilitaryUnit* MilitaryUnit, const UMassEntitySubsystem& EntitySubsystem, FNavPathSharedPtr SquadLeaderPath, const FMassExecutionContext& Context)
{
	const bool bIsSquadLeader = MilitaryUnit->SquadMemberIndex == 0;
	const bool bIsValidSquadMember = MilitaryUnit->SquadMemberIndex >= 0;
	if (MilitaryUnit->bIsSoldier && !bIsSquadLeader && bIsValidSquadMember)
	{
		FMassEntityView SoldierEntityView(EntitySubsystem, MilitaryUnit->GetMassEntityHandle());
		FMassNavMeshMoveFragment& SoldierNavMeshMoveFragment = SoldierEntityView.GetFragmentData<FMassNavMeshMoveFragment>();
		FTransformFragment& SoldierTransformFragment = SoldierEntityView.GetFragmentData<FTransformFragment>();
		const TArray<FNavPathPoint>& SquadLeaderPathPoints = SquadLeaderPath.Get()->GetPathPoints();
		TArray<FVector> SquadMemberPathPoints;
		SquadMemberPathPoints.Add(SoldierTransformFragment.GetTransform().GetLocation());
		int32 Index = 0;
		for (const FNavPathPoint& SquadLeaderPathPoint : SquadLeaderPathPoints)
		{
			const FVector NextPoint = Index + 1 < SquadLeaderPathPoints.Num() ? SquadLeaderPathPoints[Index + 1] : SquadLeaderPathPoints.Last();
			const FVector& SquadLeaderPathPointLocation = SquadLeaderPathPoint.Location;
			const FVector& ForwardToNextPoint = (NextPoint - SquadLeaderPathPointLocation).GetSafeNormal();
			const float ForwardToNextPointHeadingDegrees = FMath::RadiansToDegrees(UE::MassNavigation::GetYawFromDirection(ForwardToNextPoint));
			FVector2D UnrotatedOffset = GSquadMemberOffsetsMeters[MilitaryUnit->SquadMemberIndex] * 100.f * GSquadSpacingScalingFactor;
			UnrotatedOffset = FVector2D(UnrotatedOffset.Y, UnrotatedOffset.X);
			const FVector2D RotatedOffset = UnrotatedOffset.GetRotated(ForwardToNextPointHeadingDegrees);
			const FVector SquadMemberPathPointLocation = SquadLeaderPathPointLocation + FVector(RotatedOffset, 0.f);
			SquadMemberPathPoints.Add(SquadMemberPathPointLocation);
			Index++;
		}
		FNavPathSharedPtr SquadMemberPathShared = MakeShareable(new FNavigationPath(SquadMemberPathPoints));
		SoldierNavMeshMoveFragment.Path = SquadMemberPathShared;
		SoldierNavMeshMoveFragment.CurrentPathPointIndex = 0;
		SoldierNavMeshMoveFragment.SquadMemberIndex = MilitaryUnit->SquadMemberIndex;

		Context.Defer().AddTag<FMassNeedsNavMeshMoveTag>(MilitaryUnit->GetMassEntityHandle());
	}
	for (const UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
	{
		SetSquadMemberPaths(SubUnit, EntitySubsystem, SquadLeaderPath, Context);
	}
}

FNavPathSharedPtr PrependExtraPointIfNeeded(FNavPathSharedPtr NavPath, const bool bIsSquadLeader)
{
	if (!bIsSquadLeader)
	{
		return NavPath;
	}

	const TArray<FNavPathPoint>& PathPoints = NavPath.Get()->GetPathPoints();
	TArray<FVector> NewPathPoints;
	NewPathPoints.Reserve(PathPoints.Num() + 1);
	NewPathPoints.Add(PathPoints[0].Location);
	for (const FNavPathPoint& PathPoint : PathPoints)
	{
		NewPathPoints.Add(PathPoint.Location);
	}

	return MakeShareable(new FNavigationPath(NewPathPoints));
}

bool ProcessEntity(const UMassMoveToCommandProcessor* Processor, const FTeamMemberFragment& TeamMemberFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, const FVector& EntityLocation, const FMassEntityHandle &Entity, UNavigationSystemV1* NavSys, FMassNavMeshMoveFragment& NavMeshMoveFragment, const FMassExecutionContext& Context, const UMilitaryUnit* LastMoveToCommandMilitaryUnit, const UWorld* World, const float& NavMeshRadius, const UMassEntitySubsystem& EntitySubsystem)
{
	if (TeamMemberFragment.IsOnTeam1 != IsLastMoveToCommandForTeam1)
	{
		return false;
	}

	if (!IsEntityCommandableByUnit(Entity, LastMoveToCommandMilitaryUnit, World))
	{
		return false;
	}

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	check(MilitaryStructureSubsystem);
	UMilitaryUnit* EntityUnit = MilitaryStructureSubsystem->GetUnitForEntity(Entity);

	bool bSetSquadMemberPaths = false;
	const bool bIsSquadLeader = IsSquadLeader(EntityUnit);
	if (bIsSquadLeader)
	{
		NavMeshMoveFragment.SquadMemberIndex = 0;
		bSetSquadMemberPaths = true;
	}
	else if (IsSquadMember(EntityUnit))
	{
		return true; // return since squad leader will have set NavMeshMoveFragment on squad members
	}

	static constexpr float AgentHeight = 200.f; // TODO: Don't hard-code
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
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find closest valid location to move to command location to %s"), *LastMoveToCommandTarget.ToString());
		return false;
	}

	const FPathFindingQuery Query(Processor, *NavData, EntityLocation, ClosestValidLocation.Location);
	const FPathFindingResult Result = NavSys->FindPathSync(Query);

	if (!Result.IsSuccessful())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find path to target. NavMeshRadius = %.0f, Start = %s, End = %s"), NavMeshRadius, *EntityLocation.ToString(), *ClosestValidLocation.Location.ToString());
		return false;
	}

	if (bSetSquadMemberPaths)
	{
		const UMilitaryUnit* SquadMilitaryUnit = EntityUnit->Parent;
		SetSquadMemberPaths(SquadMilitaryUnit, EntitySubsystem, Result.Path, Context);
	}

	NavMeshMoveFragment.Path = PrependExtraPointIfNeeded(Result.Path, bIsSquadLeader);
	NavMeshMoveFragment.CurrentPathPointIndex = 0;

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

	const bool& IsLastMoveToCommandForTeam1 = MoveToCommandSubsystem->IsLastMoveToCommandForTeam1();
	const UMilitaryUnit* LastMoveToCommandMilitaryUnit = MoveToCommandSubsystem->GetLastMoveToCommandMilitaryUnit();

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &IsLastMoveToCommandForTeam1, LastMoveToCommandTarget, NavSys, LastMoveToCommandMilitaryUnit, &NumEntitiesSetMoveTarget, &EntitySubsystem](FMassExecutionContext& Context)
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

			const bool& bDidSetMoveTarget = ProcessEntity(this, TeamMemberList[i], IsLastMoveToCommandForTeam1, MoveToCommandTarget, EntityLocation, Context.GetEntity(i), NavSys, NavMeshMoveList[i], Context, LastMoveToCommandMilitaryUnit, GetWorld(), NavMeshParams.NavMeshRadius, EntitySubsystem);

			if (bDidSetMoveTarget)
			{
				NumEntitiesSetMoveTarget++;
			}
		}
	});

	UE_LOG(LogTemp, Log, TEXT("UMassMoveToCommandProcessor: Set move target to %d entities."), NumEntitiesSetMoveTarget);

	MoveToCommandSubsystem->ResetLastMoveToCommand();
}
