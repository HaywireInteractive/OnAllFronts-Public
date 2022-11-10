// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveTargetForwardCompleteProcessor.h"

#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include <MassMoveToCommandProcessor.h>
#include "MassTrackedVehicleOrientationProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "InvalidTargetFinderProcessor.h"

UMassMoveTargetForwardCompleteProcessor::UMassMoveTargetForwardCompleteProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Behavior);
}

void UMassMoveTargetForwardCompleteProcessor::Initialize(UObject& Owner)
{
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UMassMoveTargetForwardCompleteProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveForwardCompleteSignalFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassNavMeshMoveFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassNeedsMoveTargetForwardCompleteSignalTag>(EMassFragmentPresence::All);
}

void UMassMoveTargetForwardCompleteProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassMoveTargetForwardCompleteProcessor");

	TransientEntitiesToSignal.Reset();

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveForwardCompleteSignalFragment> MoveForwardCompleteSignalList = Context.GetFragmentView<FMassMoveForwardCompleteSignalFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassNavMeshMoveFragment> NavMeshMoveList = Context.GetMutableFragmentView<FMassNavMeshMoveFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const bool& bAtMoveTargetForward = IsTransformFacingDirection(LocationList[EntityIndex].GetTransform(), MoveTargetList[EntityIndex].Forward);

			if (bAtMoveTargetForward) {
				const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);

				const auto SignalType = MoveForwardCompleteSignalList[EntityIndex].SignalType;
				if (SignalType == EMassMoveForwardCompleteSignalType::NewStateTreeTask)
				{
					TransientEntitiesToSignal.Add(Entity);
				}
				else if (SignalType == EMassMoveForwardCompleteSignalType::TrackSoundComplete && StashedMoveTargetList.Num() > 0 && MoveTargetList.Num() > 0)
				{
					UnstashMoveTarget(StashedMoveTargetList[EntityIndex], MoveTargetList[EntityIndex], *EntitySubsystem.GetWorld(), Context, NavMeshMoveList[EntityIndex], LocationList[EntityIndex].GetTransform());
					Context.Defer().RemoveTag<FMassHasStashedMoveTargetTag>(Entity);
				}
				Context.Defer().RemoveTag<FMassTrackSoundTag>(Entity);
				Context.Defer().RemoveTag<FMassNeedsMoveTargetForwardCompleteSignalTag>(Entity);
			}
		}
	});

	if (TransientEntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, TransientEntitiesToSignal);
	}
}
