// Fill out your copyright notice in the Description page of Project Settings.

#include "MassFireProjectileTask.h"

#include "StateTreeExecutionContext.h"
#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnLocationProcessor.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassSignalSubsystem.h"
#include "Async/Async.h"
#include "StateTreeLinker.h"

bool FMassFireProjectileTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(EntityTransformHandle);

	Linker.LinkInstanceDataProperty(EntityConfigHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, EntityConfig));
	Linker.LinkInstanceDataProperty(InitialVelocityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, InitialVelocity));
	Linker.LinkInstanceDataProperty(ForwardVectorMagnitudeHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, ForwardVectorMagnitude));
	Linker.LinkInstanceDataProperty(ProjectileLocationOffsetHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, ProjectileLocationOffset));

	return true;
}

EStateTreeRunStatus FMassFireProjectileTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	const UWorld* World = Context.GetWorld();
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const UMassEntitySubsystem& EntitySubsystem = MassContext.GetEntitySubsystem();

	const FTransformFragment& StateTreeEntityTransformFragment = Context.GetExternalData(EntityTransformHandle);
	const FVector StateTreeEntityLocation = StateTreeEntityTransformFragment.GetTransform().GetLocation();
	const FVector StateTreeEntityCurrentForward = StateTreeEntityTransformFragment.GetTransform().GetRotation().GetForwardVector();

	const FMassEntityConfig& EntityConfig = Context.GetInstanceData(EntityConfigHandle);
	const float InitialVelocityMagnitude = Context.GetInstanceData(InitialVelocityHandle);
	const FVector InitialVelocity = StateTreeEntityCurrentForward * InitialVelocityMagnitude;
	const float ForwardVectorMagnitude = Context.GetInstanceData(ForwardVectorMagnitudeHandle);
	const FVector& ProjectileLocationOffset = Context.GetInstanceData(ProjectileLocationOffsetHandle);

	const FVector SpawnLocation = StateTreeEntityLocation + StateTreeEntityCurrentForward * ForwardVectorMagnitude + ProjectileLocationOffset;

	AsyncTask(ENamedThreads::GameThread, [EntityConfig, InitialVelocity, SpawnLocation, &EntitySubsystem, World]()
	{
		UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
		if (SpawnerSystem == nullptr)
		{
			return;
		}

		const FMassEntityTemplate& EntityTemplate = EntityConfig.GetOrCreateEntityTemplate(*World, *SpawnerSystem); // TODO: passing SpawnerSystem is a hack
		if (EntityTemplate.IsValid())
		{
			FMassEntitySpawnDataGeneratorResult Result;
			Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
			Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
			Result.NumEntities = 1;
			FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

			Transforms.Transforms.Reserve(1);
			FTransform& SpawnDataTransform = Transforms.Transforms.AddDefaulted_GetRef();
			SpawnDataTransform.SetLocation(SpawnLocation);

			TArray<FMassEntityHandle> SpawnedEntities;
			SpawnerSystem->SpawnEntities(EntityTemplate.GetTemplateID(), Result.NumEntities, Result.SpawnData, Result.SpawnDataProcessor, SpawnedEntities);

			FMassVelocityFragment* SpawnedEntityVelocityFragment = EntitySubsystem.GetFragmentDataPtr<FMassVelocityFragment>(SpawnedEntities[0]);
			if (SpawnedEntityVelocityFragment)
			{
				SpawnedEntityVelocityFragment->Value = InitialVelocity;
			} else {
				UE_LOG(LogTemp, Warning, TEXT("FMassFireProjectileTask::EnterState: Spawned entity has no FMassVelocityFragment"));
			}
		}
	});

	UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity(), 0.1f); // TODO: needed?

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassFireProjectileTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Succeeded;
}
