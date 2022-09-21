// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassEntityConfigAsset.h"
#include "MassRifle.generated.h"

UCLASS()
class PROJECTM_API AMassRifle : public AActor
{
	GENERATED_BODY()

public:
	AMassRifle();

protected:
	UFUNCTION(BlueprintCallable)
	void SpawnProjectile(const FTransform SpawnTransform, const bool bIsPlayerTeam1) const;

	UPROPERTY(EditAnywhere, Category = "Mass")
	FMassEntityConfig ProjectileEntityConfig;
};
