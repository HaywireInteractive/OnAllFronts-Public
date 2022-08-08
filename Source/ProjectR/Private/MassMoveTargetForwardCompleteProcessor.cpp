// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveTargetForwardCompleteProcessor.h"

#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"

UMassMoveTargetForwardCompleteProcessor::UMassMoveTargetForwardCompleteProcessor()
	: EntityQuery(*this)
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
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassNeedsMoveTargetForwardCompleteSignalTag>(EMassFragmentPresence::All);
}

void UMassMoveTargetForwardCompleteProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TransientEntitiesToSignal.Reset();

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FTransform& CurrentTransform = LocationList[EntityIndex].GetTransform();
			const FMassMoveTargetFragment& MoveTargetFragment = MoveTargetList[EntityIndex];

			const FQuat& CurrentRotation = CurrentTransform.GetRotation();

			const float MoveTargetForwardHeading = UE::MassNavigation::GetYawFromDirection(MoveTargetFragment.Forward);
			FQuat MoveTargetForwardRotation(FVector::UpVector, MoveTargetForwardHeading);

			const bool bAtMoveTargetForward = CurrentRotation.Equals(MoveTargetForwardRotation, 1.0f);

			if (bAtMoveTargetForward) {
				const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
				TransientEntitiesToSignal.Add(Entity);
				Context.Defer().RemoveTag<FMassNeedsMoveTargetForwardCompleteSignalTag>(Entity);
			}
		}
	});

	if (TransientEntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, TransientEntitiesToSignal);
	}
}
