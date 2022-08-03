// Fill out your copyright notice in the Description page of Project Settings.


#include "DestroyedTargetFinderProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"

UDestroyedTargetFinderProcessor::UDestroyedTargetFinderProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UDestroyedTargetFinderProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassWillNeedEnemyTargetTag>(EMassFragmentPresence::All);
}

void ProcessEntity(const FMassExecutionContext& Context, const FMassEntityHandle Entity, const UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment)
{
	bool bTargetEntityWasDestroyed = !EntitySubsystem.IsEntityValid(TargetEntityFragment.Entity);
	if (!bTargetEntityWasDestroyed)
	{
		return;
	}
	
	TargetEntityFragment.Entity.Reset();
	Context.Defer().AddTag<FMassNeedsEnemyTargetTag>(Entity);
	Context.Defer().RemoveTag<FMassWillNeedEnemyTargetTag>(Entity);
}

void UDestroyedTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UDestroyedTargetFinderProcessor);

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				ProcessEntity(Context, Context.GetEntity(EntityIndex), EntitySubsystem, TargetEntityList[EntityIndex]);
			}
		});
}
