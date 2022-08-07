// Fill out your copyright notice in the Description page of Project Settings.


#include "DestroyedTargetFinderProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"

UDestroyedTargetFinderProcessor::UDestroyedTargetFinderProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UDestroyedTargetFinderProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassWillNeedEnemyTargetTag>(EMassFragmentPresence::All);
}

bool IsTargetEntityOutOfRange(const FMassEntityHandle& TargetEntity, const FVector &EntityLocation, const UMassEntitySubsystem& EntitySubsystem)
{
	const FTransformFragment* TargetTransformFragment = EntitySubsystem.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
	check(TargetTransformFragment);

	const FVector& TargetEntityLocation = TargetTransformFragment->GetTransform().GetLocation();
	const double DistanceBetweenEntities = (TargetEntityLocation - EntityLocation).Size();

	static const double MaxRange = 7000.f; // TODO: make configurable in data asset
	return DistanceBetweenEntities > MaxRange;
}

void ProcessEntity(const FMassExecutionContext& Context, const FMassEntityHandle Entity, const UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment, const FVector &EntityLocation)
{
	FMassEntityHandle& TargetEntity = TargetEntityFragment.Entity;
	bool bTargetEntityWasDestroyed = !EntitySubsystem.IsEntityValid(TargetEntity);
	bool bTargetEntityOutOfRange = !bTargetEntityWasDestroyed && IsTargetEntityOutOfRange(TargetEntity, EntityLocation, EntitySubsystem);
	if (bTargetEntityWasDestroyed || bTargetEntityOutOfRange)
	{
		TargetEntity.Reset();
		Context.Defer().AddTag<FMassNeedsEnemyTargetTag>(Entity);
		Context.Defer().RemoveTag<FMassWillNeedEnemyTargetTag>(Entity);
		Context.Defer().RemoveTag<FMassTrackTargetTag>(Entity);
	}
}

void UDestroyedTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UDestroyedTargetFinderProcessor);

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
			const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				ProcessEntity(Context, Context.GetEntity(EntityIndex), EntitySubsystem, TargetEntityList[EntityIndex], TransformList[EntityIndex].GetTransform().GetLocation());
			}
		});
}
