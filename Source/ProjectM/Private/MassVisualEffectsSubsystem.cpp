// Fill out your copyright notice in the Description page of Project Settings.


#include "MassVisualEffectsSubsystem.h"

int16 UMassVisualEffectsSubsystem::FindOrAddEntityConfig(UMassEntityConfigAsset* EntityConfigAsset)
{
	int32 Index = MassEntityConfigAssets.IndexOfByPredicate([EntityConfigAsset](UMassEntityConfigAsset* EntityConfigAssetInArray) { return EntityConfigAssetInArray == EntityConfigAsset; });
	if (Index == INDEX_NONE)
	{
		Index = MassEntityConfigAssets.Emplace(EntityConfigAsset);
	}
	check(Index < INT16_MAX);
	return (int16)Index;
}

void UMassVisualEffectsSubsystem::SpawnEntity(const int16 EntityConfigIndex, const FVector& Location)
{
	UWorld* World = GetWorld();
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	if (SpawnerSystem == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassVisualEffectsSubsystem: Invalid SpawnerSystem"));
		return;
	}

	if (EntityConfigIndex < 0 || EntityConfigIndex >= MassEntityConfigAssets.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassVisualEffectsSubsystem: Invalid EntityConfigIndex"));
		return;
	}

	// TODO: A bit hacky to get first actor here.
	const FMassEntityTemplate* EntityTemplate = MassEntityConfigAssets[EntityConfigIndex]->GetConfig().GetOrCreateEntityTemplate(*World->GetLevel(0)->Actors[0], *SpawnerSystem); // TODO: passing SpawnerSystem is a hack

	if (!EntityTemplate->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UMassVisualEffectsSubsystem: Invalid entity template"));
		return;
	}

	FMassEntitySpawnDataGeneratorResult Result;
	Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
	Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
	Result.NumEntities = 1;
	FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

	Transforms.Transforms.Reserve(1);
	FTransform& SpawnDataTransform = Transforms.Transforms.AddDefaulted_GetRef();
	SpawnDataTransform.SetLocation(Location);

	TArray<FMassEntityHandle> SpawnedEntities;
	SpawnerSystem->SpawnEntities(EntityTemplate->GetTemplateID(), Result.NumEntities, Result.SpawnData, Result.SpawnDataProcessor, SpawnedEntities);
}
