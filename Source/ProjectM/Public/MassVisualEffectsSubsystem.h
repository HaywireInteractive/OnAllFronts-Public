// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassVisualEffectsSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTM_API UMassVisualEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<UMassEntityConfigAsset*> MassEntityConfigAssets;

public:
	int16 FindOrAddEntityConfig(UMassEntityConfigAsset* ExplosionEntityConfig);

	void SpawnEntity(const int16 EntityConfigIndex, const FVector& Location);
	void SpawnEntity(const int16 EntityConfigIndex, const FTransform& Transform);
};
