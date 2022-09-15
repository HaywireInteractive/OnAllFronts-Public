// Fill out your copyright notice in the Description page of Project Settings.


#include "MassDelayedDestructionProcessor.h"

//----------------------------------------------------------------------//
//  UMassDelayedDestructionTrait
//----------------------------------------------------------------------//
void UMassDelayedDestructionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	FMassDelayedDestructionFragment& DelayedDestructionFragment = BuildContext.AddFragment_GetRef<FMassDelayedDestructionFragment>();
	DelayedDestructionFragment.SecondsLeftTilDestruction = SecondsDelay;
}

//----------------------------------------------------------------------//
//  UMassDelayedDestructionProcessor
//----------------------------------------------------------------------//
UMassDelayedDestructionProcessor::UMassDelayedDestructionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void UMassDelayedDestructionProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassDelayedDestructionFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassDelayedDestructionProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	UWorld* World = EntitySubsystem.GetWorld();
	float DeltaTimeSeconds = World->DeltaTimeSeconds;

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [DeltaTimeSeconds](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TArrayView<FMassDelayedDestructionFragment> DelayedDestructionList = Context.GetMutableFragmentView<FMassDelayedDestructionFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassDelayedDestructionFragment& DelayedDestructionFragment = DelayedDestructionList[EntityIndex];
			DelayedDestructionFragment.SecondsLeftTilDestruction -= DeltaTimeSeconds;
			if (DelayedDestructionFragment.SecondsLeftTilDestruction <= 0.f)
			{
				Context.Defer().DestroyEntity(Context.GetEntity(EntityIndex));
			}
		}
	});
}
