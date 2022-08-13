// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MassEntityConfigAsset.h"
#include "CommanderCharacter.generated.h"

class UMassMoveToCommandSubsystem;

UCLASS()
class PROJECTR_API ACommanderCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACommanderCharacter();

protected:
	UPROPERTY()
	UMassMoveToCommandSubsystem* MoveToCommandSystem;

	UPROPERTY(EditAnywhere, Category = "Mass")
	FMassEntityConfig ProjectileEntityConfig;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void SetMoveToCommand() const;

	UFUNCTION(BlueprintCallable)
	void SpawnProjectile() const;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
