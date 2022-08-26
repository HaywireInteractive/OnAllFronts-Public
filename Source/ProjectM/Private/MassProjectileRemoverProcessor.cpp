// Fill out your copyright notice in the Description page of Project Settings.


#include "MassProjectileRemoverProcessor.h"
#include "MassProjectileDamageProcessor.h"
#include "MassCommonFragments.h"

UMassProjectileRemoverProcessor::UMassProjectileRemoverProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void UMassProjectileRemoverProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassProjectileWithDamageTag>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMinZParameters>(EMassFragmentPresence::All);
}

void UMassProjectileRemoverProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const FMinZParameters& MinZParams = Context.GetConstSharedFragment<FMinZParameters>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FTransformFragment& Location = LocationList[EntityIndex];
			if (Location.GetTransform().GetTranslation().Z <= MinZParams.Value)
			{
				Context.Defer().DestroyEntity(Context.GetEntity(EntityIndex));
			}
		}
	});
}
