// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassMoveToCommandProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include <MassNavMeshMoveProcessor.h>
#include <MilitaryStructureSubsystem.h>
#include "DrawDebugHelpers.h"

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
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
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

bool UMassMoveToCommandProcessor_DrawPathResults = false;
FAutoConsoleVariableRef CVarUMassMoveToCommandProcessor_DrawPathResults(TEXT("pm.UMassMoveToCommandProcessor_DrawPathResults"), UMassMoveToCommandProcessor_DrawPathResults, TEXT("UMassMoveToCommandProcessor: Draw NavMesh Path Results"));

bool ProcessEntity(const UMassMoveToCommandProcessor* Processor, const FTeamMemberFragment& TeamMemberFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, const FVector& EntityLocation, const FMassEntityHandle &Entity, UNavigationSystemV1* NavSys, FMassNavMeshMoveFragment& NavMeshMoveFragment, FMassExecutionContext& Context, const UMilitaryUnit* LastMoveToCommandMilitaryUnit, const UWorld* World, const float& NavMeshRadius)
{
	if (TeamMemberFragment.IsOnTeam1 != IsLastMoveToCommandForTeam1)
	{
		return false;
	}

	if (!IsEntityCommandableByUnit(Entity, LastMoveToCommandMilitaryUnit, World))
	{
		return false;
	}

	static const float AgentHeight = 200.f; // TODO: Don't hardcode
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

	FPathFindingQuery Query(Processor, *NavData, EntityLocation, ClosestValidLocation.Location);
	FPathFindingResult Result = NavSys->FindPathSync(Query);

	if (!Result.IsSuccessful())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find path to target. NavMeshRadius = %.0f, Start = %s, End = %s"), NavMeshRadius, *EntityLocation.ToString(), *ClosestValidLocation.Location.ToString());
		return false;
	}

	if (UMassMoveToCommandProcessor_DrawPathResults || UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		int32 LineEndIndex = 1;
		for (const FNavPathPoint& NavPathPoint : Result.Path.Get()->GetPathPoints())
		{
			DrawDebugPoint(NavSys->GetWorld(), NavPathPoint.Location, 10.f, FColor::Red, true);
			if (LineEndIndex >= Result.Path.Get()->GetPathPoints().Num())
			{
				break;
			}
			const auto LineEndNavPathPoint = Result.Path.Get()->GetPathPoints()[LineEndIndex++];
			DrawDebugLine(NavSys->GetWorld(), NavPathPoint.Location, LineEndNavPathPoint.Location, FColor::Red, true);
		}
	}

	NavMeshMoveFragment.Path = Result.Path;
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

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &IsLastMoveToCommandForTeam1, LastMoveToCommandTarget, NavSys, LastMoveToCommandMilitaryUnit, &NumEntitiesSetMoveTarget](FMassExecutionContext& Context)
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

			const bool& bDidSetMoveTarget = ProcessEntity(this, TeamMemberList[i], IsLastMoveToCommandForTeam1, MoveToCommandTarget, EntityLocation, Context.GetEntity(i), NavSys, NavMeshMoveList[i], Context, LastMoveToCommandMilitaryUnit, GetWorld(), NavMeshParams.NavMeshRadius);

			if (bDidSetMoveTarget)
			{
				NumEntitiesSetMoveTarget++;
			}
		}
	});

	UE_LOG(LogTemp, Log, TEXT("UMassMoveToCommandProcessor: Set move target to %d entities."), NumEntitiesSetMoveTarget);

	MoveToCommandSubsystem->ResetLastMoveToCommand();
}
