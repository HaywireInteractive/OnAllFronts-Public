// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FirstPersonGameModeCommander.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTM_API AFirstPersonGameModeCommander : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void StartPlay() override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCompletedAssigningEntitiesToMilitaryUnits();
};
