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
#include "MassProjectileDamageProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassSoundPerceptionSubsystem.h"

void SpawnProjectile(const UWorld* World, const FVector& SpawnLocation, const FQuat& SpawnRotation, const FVector& InitialVelocity, const FMassEntityConfig& EntityConfig, const bool& bIsProjectileFromTeam1)
{
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	if (SpawnerSystem == nullptr)
	{
		return;
	}

	// TODO: A bit hacky to get first actor here.
	const FMassEntityTemplate* EntityTemplate = EntityConfig.GetOrCreateEntityTemplate(*World->GetLevel(0)->Actors[0], *SpawnerSystem); // TODO: passing SpawnerSystem is a hack
	if (!EntityTemplate->IsValid())
	{
		return;
	}
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
	SpawnerSystem->SpawnEntities(EntityTemplate->GetTemplateID(), Result.NumEntities, Result.SpawnData, Result.SpawnDataProcessor, SpawnedEntities);

	const UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	check(EntitySubsystem);
	if (FMassVelocityFragment* SpawnedEntityVelocityFragment = EntitySubsystem->GetFragmentDataPtr<FMassVelocityFragment>(SpawnedEntities[0]))
	{
		SpawnedEntityVelocityFragment->Value = InitialVelocity;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnProjectile: Spawned entity has no FMassVelocityFragment"));
	}

	if (FMassPreviousLocationFragment* SpawnedEntityPreviousLocationFragment = EntitySubsystem->GetFragmentDataPtr<FMassPreviousLocationFragment>(SpawnedEntities[0]))
	{
		SpawnedEntityPreviousLocationFragment->Location = SpawnLocation;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnProjectile: Spawned entity has no FMassPreviousLocationFragment"));
	}

	UMassSoundPerceptionSubsystem* SoundPerceptionSubsystem = UWorld::GetSubsystem<UMassSoundPerceptionSubsystem>(World);
	check(SoundPerceptionSubsystem);
	SoundPerceptionSubsystem->AddSoundPerception(SpawnLocation, bIsProjectileFromTeam1);
}

bool FMassFireProjectileTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(EntityTransformHandle);
	Linker.LinkExternalData(TeamMemberHandle);

	Linker.LinkInstanceDataProperty(EntityConfigHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, EntityConfig));
	Linker.LinkInstanceDataProperty(InitialVelocityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, InitialVelocity));
	Linker.LinkInstanceDataProperty(ForwardVectorMagnitudeHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, ForwardVectorMagnitude));
	Linker.LinkInstanceDataProperty(IsFromSoldierHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, IsFromSoldier));
	Linker.LinkInstanceDataProperty(WeaponCoolDownSecondsHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, WeaponCoolDownSeconds));
	Linker.LinkInstanceDataProperty(LastWeaponFireTimeSecondsHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassFireProjectileTaskInstanceData, LastWeaponFireTimeSeconds));

	return true;
}

EStateTreeRunStatus FMassFireProjectileTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	const UWorld* World = Context.GetWorld();

	const float& WorldRealTimeSeconds = World->GetRealTimeSeconds();
	const float& WeaponCoolDownSeconds = Context.GetInstanceData(WeaponCoolDownSecondsHandle);
	float& LastWeaponFireTimeSeconds = Context.GetInstanceData(LastWeaponFireTimeSecondsHandle);

	if (LastWeaponFireTimeSeconds > 0.f && WorldRealTimeSeconds - LastWeaponFireTimeSeconds < WeaponCoolDownSeconds)
	{
		MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity(), 1.0f); // TODO: needed?
		return EStateTreeRunStatus::Running;
	}


	const FTransformFragment& StateTreeEntityTransformFragment = Context.GetExternalData(EntityTransformHandle);
	const FTransform& StateTreeEntityTransform = StateTreeEntityTransformFragment.GetTransform();
	const FVector StateTreeEntityLocation = StateTreeEntityTransform.GetLocation();
	const FVector StateTreeEntityCurrentForward = StateTreeEntityTransform.GetRotation().GetForwardVector();

	const FMassEntityConfig& EntityConfig = Context.GetInstanceData(EntityConfigHandle);
	const float InitialVelocityMagnitude = Context.GetInstanceData(InitialVelocityHandle);
	const FVector InitialVelocity = StateTreeEntityCurrentForward * InitialVelocityMagnitude;
	const float ForwardVectorMagnitude = Context.GetInstanceData(ForwardVectorMagnitudeHandle);
	const bool IsFromSoldier = Context.GetInstanceData(IsFromSoldierHandle);
	const FVector& ProjectileLocationOffset = FVector(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(IsFromSoldier));

	const FVector SpawnLocation = StateTreeEntityLocation + StateTreeEntityCurrentForward * ForwardVectorMagnitude + ProjectileLocationOffset;
	const FQuat SpawnRotation = StateTreeEntityTransform.GetRotation();

	const FTeamMemberFragment& StateTreeEntityTeamMemberFragment = Context.GetExternalData(TeamMemberHandle);
	const bool& bIsProjectileSourceTeam1 = StateTreeEntityTeamMemberFragment.IsOnTeam1;

	AsyncTask(ENamedThreads::GameThread, [EntityConfig, InitialVelocity, SpawnLocation, World, SpawnRotation, bIsProjectileSourceTeam1]()
	{
		SpawnProjectile(World, SpawnLocation, SpawnRotation, InitialVelocity, EntityConfig, bIsProjectileSourceTeam1);
	});

	MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity(), 1.0f); // TODO: needed?

	LastWeaponFireTimeSeconds = WorldRealTimeSeconds;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassFireProjectileTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Succeeded;
}
