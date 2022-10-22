// Adapted from MassSpawnLocationProcessor.cpp, just without randomness.

#include "MassOrderedSpawnLocationProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassSpawnerTypes.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"

//----------------------------------------------------------------------//
// UMassOrderedSpawnLocationProcessor 
//----------------------------------------------------------------------//
UMassOrderedSpawnLocationProcessor::UMassOrderedSpawnLocationProcessor()
{
	bAutoRegisterWithProcessingPhases = false;
}

void UMassOrderedSpawnLocationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassOrderedSpawnLocationProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	if (!ensure(Context.ValidateAuxDataType<FMassTransformsSpawnData>()))
	{
		UE_VLOG_UELOG(this, LogMass, Log, TEXT("Execution context has invalid AuxData or it's not FMassSpawnAuxData. Entity transforms won't be initialized."));
		return;
	}

	const UWorld* World = EntitySubsystem.GetWorld();

	check(World);

	const ENetMode NetMode = World->GetNetMode();

	if (NetMode != NM_Client)
	{
		FMassTransformsSpawnData& AuxData = Context.GetMutableAuxData().GetMutable<FMassTransformsSpawnData>();
		TArray<FTransform>& Transforms = AuxData.Transforms;

		const int32 NumSpawnTransforms = Transforms.Num();
		if (NumSpawnTransforms == 0)
		{
			UE_VLOG_UELOG(this, LogMass, Error, TEXT("No spawn transforms provided. Entity transforms won't be initialized."));
			return;
		}

		int32 NumRequiredSpawnTransforms = 0;
		EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&NumRequiredSpawnTransforms](const FMassExecutionContext& Context)
			{
				NumRequiredSpawnTransforms += Context.GetNumEntities();
			});

		const int32 NumToAdd = NumRequiredSpawnTransforms - NumSpawnTransforms;
		if (NumToAdd > 0)
		{
			UE_VLOG_UELOG(this, LogMass, Warning,
				TEXT("Not enough spawn locations provided (%d) for all entities (%d). Existing locations will be reused randomly to fill the %d missing positions."),
				NumSpawnTransforms, NumRequiredSpawnTransforms, NumToAdd);

			Transforms.AddUninitialized(NumToAdd);
			for (int i = 0; i < NumToAdd; ++i)
			{
				Transforms[NumSpawnTransforms + i] = Transforms[FMath::RandRange(0, NumSpawnTransforms - 1)];
			}
		}

		int32 TransformIndex = 0;
		EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&Transforms, this, &TransformIndex](FMassExecutionContext& Context)
		{
			const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
			const int32 NumEntities = Context.GetNumEntities();
			for (int32 i = 0; i < NumEntities; ++i)
			{
				LocationList[i].GetMutableTransform() = Transforms[TransformIndex++];
			}
		});
	}
}
