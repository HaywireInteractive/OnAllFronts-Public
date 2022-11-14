#include "MassSwapEntityOnDestructionTrait.h"

#include <MassVisualEffectsSubsystem.h>
#include <MassCommonFragments.h>

//----------------------------------------------------------------------//
//  UMassSwapEntityOnDestructionTrait
//----------------------------------------------------------------------//
void UMassSwapEntityOnDestructionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	if (SwappedEntityConfig)
	{
		FMassSwapEntityOnDestructionFragment& SwapEntityOnDestructionFragment = BuildContext.AddFragment_GetRef<FMassSwapEntityOnDestructionFragment>();
		UMassVisualEffectsSubsystem* MassVisualEffectsSubsystem = UWorld::GetSubsystem<UMassVisualEffectsSubsystem>(&World);
		SwapEntityOnDestructionFragment.SwappedEntityConfigIndex = MassVisualEffectsSubsystem->FindOrAddEntityConfig(SwappedEntityConfig);
	}
}

//----------------------------------------------------------------------//
//  UMassSwapEntityOnDestructionProcessor
//----------------------------------------------------------------------//
UMassSwapEntityOnDestructionProcessor::UMassSwapEntityOnDestructionProcessor()
{
	ObservedType = FMassSwapEntityOnDestructionFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
}

void UMassSwapEntityOnDestructionProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassSwapEntityOnDestructionFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UMassSwapEntityOnDestructionProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TConstArrayView<FMassSwapEntityOnDestructionFragment> SwapEntityOnDestructionList = Context.GetFragmentView<FMassSwapEntityOnDestructionFragment>();
		TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const int16 SwappedEntityConfigIndex = SwapEntityOnDestructionList[i].SwappedEntityConfigIndex;
			if (SwappedEntityConfigIndex >= 0)
			{
				UMassVisualEffectsSubsystem* MassVisualEffectsSubsystem = UWorld::GetSubsystem<UMassVisualEffectsSubsystem>(EntitySubsystem.GetWorld());
				check(MassVisualEffectsSubsystem);

				const FVector& EntityLocation = TransformList[i].GetTransform().GetLocation();

				// Must be done async because we can't spawn Mass entities in the middle of a Mass processor's Execute method.
				AsyncTask(ENamedThreads::GameThread, [MassVisualEffectsSubsystem, SwappedEntityConfigIndex = SwappedEntityConfigIndex, EntityLocation]()
				{
					MassVisualEffectsSubsystem->SpawnEntity(SwappedEntityConfigIndex, EntityLocation);
				});
			}
		}
	});
}
