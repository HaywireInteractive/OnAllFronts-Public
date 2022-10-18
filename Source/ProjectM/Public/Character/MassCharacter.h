// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AnimToTextureDataAsset.h"
#include "MassCharacter.generated.h"

UCLASS()
class PROJECTM_API AMassCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Mass")
	TObjectPtr<UAnimToTextureDataAsset> AnimToTextureDataAsset;
};
