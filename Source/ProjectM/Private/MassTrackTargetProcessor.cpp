// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassRepresentationTypes.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassNavigationFragments.h"

UMassTrackTargetProcessor::UMassTrackTargetProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void UMassTrackTargetProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassTrackTargetTag>(EMassFragmentPresence::All);
}

void UMassTrackTargetProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassTrackTargetProcessor_Execute);

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTargetEntityFragment> TargetEntityList = Context.GetFragmentView<FTargetEntityFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			UpdateLookAtTrackedEntity(EntitySubsystem, TransformList[i], TargetEntityList[i], MoveTargetList[i]);
		}
	});
}

void UMassTrackTargetProcessor::UpdateLookAtTrackedEntity(const UMassEntitySubsystem& EntitySubsystem, const FTransformFragment& TransformFragment, const FTargetEntityFragment& TargetEntityFragment, FMassMoveTargetFragment& MoveTargetFragment) const
{
	const FMassEntityHandle& TargetEntity = TargetEntityFragment.Entity;
	if (!TargetEntity.IsSet()) {
		return;
	}

	if (!EntitySubsystem.IsEntityValid(TargetEntity)) {
		return;
	}
	const FTransformFragment* TargetTransformFragment = EntitySubsystem.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
	check(TargetTransformFragment);

	const FTransform& EntityTransform = TransformFragment.GetTransform();
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const FVector NewGlobalDirection = (TargetTransformFragment->GetTransform().GetLocation() - EntityLocation).GetSafeNormal();

	UWorld* World = EntitySubsystem.GetWorld();
	MoveTargetFragment.Forward = NewGlobalDirection;
}
