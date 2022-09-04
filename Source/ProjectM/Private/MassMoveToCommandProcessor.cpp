// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassMoveToCommandProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassCommonFragments.h"
#include "NavigationSystem.h"
#include <MassNavMeshMoveProcessor.h>

//----------------------------------------------------------------------//
//  UMassCommandableTrait
//----------------------------------------------------------------------//
void UMassCommandableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FMassStashedMoveTargetFragment>();
	BuildContext.AddFragment<FMassNavMeshMoveFragment>();
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
}

void UMassMoveToCommandProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	MoveToCommandSubsystem = UWorld::GetSubsystem<UMassMoveToCommandSubsystem>(Owner.GetWorld());
}

void ProcessEntity(const UMassMoveToCommandProcessor* Processor, const FTeamMemberFragment& TeamMemberFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, const FVector& EntityLocation, const FMassEntityHandle &Entity, UNavigationSystemV1* NavSys, FMassNavMeshMoveFragment& NavMeshMoveFragment, FMassExecutionContext& Context)
{
	if (TeamMemberFragment.IsOnTeam1 != IsLastMoveToCommandForTeam1)
	{
		return;
	}

	const ANavigationData* NavData = NavSys->GetNavDataForProps(FNavAgentProperties::DefaultProperties, EntityLocation);

	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not get navigation data. Likely there is no NavMesh in level."));
		return;
	}

	FPathFindingQuery Query(Processor, *NavData, EntityLocation, LastMoveToCommandTarget);
	FPathFindingResult Result = NavSys->FindPathSync(Query);

	if (!Result.IsSuccessful())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassMoveToCommandProcessor: Could not find path to target."));
		return;
	}

	NavMeshMoveFragment.Path = Result.Path;
	NavMeshMoveFragment.CurrentPathPointIndex = 0;

	Context.Defer().AddTag<FMassNeedsNavMeshMoveTag>(Entity);
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

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &IsLastMoveToCommandForTeam1, LastMoveToCommandTarget = *LastMoveToCommandTarget, NavSys](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetMutableFragmentView<FMassNavMeshMoveFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			// Change move to command target Z to make current entity's Z value since we don't know ground height.
			const FVector& EntityLocation = TransformList[i].GetTransform().GetLocation();
			FVector MoveToCommandTarget = LastMoveToCommandTarget;
			MoveToCommandTarget.Z = EntityLocation.Z;

			ProcessEntity(this, TeamMemberList[i], IsLastMoveToCommandForTeam1, MoveToCommandTarget, EntityLocation, Context.GetEntity(i), NavSys, NavMeshMoveList[i], Context);
		}
	});

	MoveToCommandSubsystem->ResetLastMoveToCommand();
}
