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
#include "MassProjectileDamageProcessor.h"

void SpawnProjectile(const UWorld* World, const FVector& SpawnLocation, const FQuat& SpawnRotation, const FVector& InitialVelocity, const FMassEntityConfig& EntityConfig)
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
		SpawnDataTransform.SetRotation(SpawnRotation);

		TArray<FMassEntityHandle> SpawnedEntities;
		SpawnerSystem->SpawnEntities(EntityTemplate.GetTemplateID(), Result.NumEntities, Result.SpawnData, Result.SpawnDataProcessor, SpawnedEntities);

		const UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
		check(EntitySubsystem);
		FMassVelocityFragment* SpawnedEntityVelocityFragment = EntitySubsystem->GetFragmentDataPtr<FMassVelocityFragment>(SpawnedEntities[0]);
		if (SpawnedEntityVelocityFragment)
		{
			SpawnedEntityVelocityFragment->Value = InitialVelocity;
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("SpawnProjectile: Spawned entity has no FMassVelocityFragment"));
		}

		FMassPreviousLocationFragment* SpawnedEntityPreviousLocationFragment = EntitySubsystem->GetFragmentDataPtr<FMassPreviousLocationFragment>(SpawnedEntities[0]);
		if (SpawnedEntityPreviousLocationFragment)
		{
			SpawnedEntityPreviousLocationFragment->Location = SpawnLocation;
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("SpawnProjectile: Spawned entity has no SpawnedEntityPreviousLocationFragment"));
		}
	}
}

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

	const FTransformFragment& StateTreeEntityTransformFragment = Context.GetExternalData(EntityTransformHandle);
	const FTransform& StateTreeEntityTransform = StateTreeEntityTransformFragment.GetTransform();
	const FVector StateTreeEntityLocation = StateTreeEntityTransform.GetLocation();
	const FVector StateTreeEntityCurrentForward = StateTreeEntityTransform.GetRotation().GetForwardVector();

	const FMassEntityConfig& EntityConfig = Context.GetInstanceData(EntityConfigHandle);
	const float InitialVelocityMagnitude = Context.GetInstanceData(InitialVelocityHandle);
	const FVector InitialVelocity = StateTreeEntityCurrentForward * InitialVelocityMagnitude;
	const float ForwardVectorMagnitude = Context.GetInstanceData(ForwardVectorMagnitudeHandle);
	const FVector& ProjectileLocationOffset = Context.GetInstanceData(ProjectileLocationOffsetHandle);

	const FVector SpawnLocation = StateTreeEntityLocation + StateTreeEntityCurrentForward * ForwardVectorMagnitude + ProjectileLocationOffset;
	const FQuat SpawnRotation = StateTreeEntityTransform.GetRotation();

	AsyncTask(ENamedThreads::GameThread, [EntityConfig, InitialVelocity, SpawnLocation, World, SpawnRotation]()
	{
			SpawnProjectile(World, SpawnLocation, SpawnRotation, InitialVelocity, EntityConfig);
	});

	UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity(), 1.0f); // TODO: needed?

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassFireProjectileTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Succeeded;
}
