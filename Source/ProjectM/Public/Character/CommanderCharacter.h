// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "AnimToTextureDataAsset.h"

#include "CommanderCharacter.generated.h"

class UMassMoveToCommandSubsystem;

UCLASS(meta = (DisplayName = "PlayerControllableCharacter"))
class PROJECTM_API UMassPlayerControllableCharacterTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

USTRUCT()
struct FMassPlayerControllableCharacterTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class PROJECTM_API ACommanderCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	void InitializeFromMassSoldierInternal();
	FMassEntityHandle GetMassEntityHandle() const;

	FDelegateHandle OnMassAgentComponentEntityAssociatedHandle;
	FMassEntityHandle MassSoldierEntityToInitializeWith;

protected:
	UPROPERTY()
	UMassMoveToCommandSubsystem* MoveToCommandSystem;

	UPROPERTY(EditAnywhere, Category = "Mass")
	FMassEntityConfig ProjectileEntityConfig;

	UPROPERTY(EditAnywhere, Category = "Mass")
	FMassEntityConfig SoldierEntityConfig;

	UFUNCTION(BlueprintCallable)
	void SpawnProjectile(const FTransform SpawnTransform) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable)
	void SetMoveToCommand(FVector2D CommandLocation) const;

	UFUNCTION(BlueprintCallable)
	class UMilitaryUnit* GetMyMilitaryUnit() const;

public:
	ACommanderCharacter();

	UFUNCTION(BlueprintCallable)
	void ChangePlayerToAISoldier();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void DidDie();

	UFUNCTION(BlueprintCallable)
	bool InitializeFromMassSoldier(const int32 MassEntityIndex, const int32 MassEntitySerialNumber);

	UFUNCTION(BlueprintCallable)
	bool IsPlayerOnTeam1() const;

	UFUNCTION(BlueprintCallable)
	bool IsCommander() const;

	UPROPERTY(EditAnywhere, Category = "Mass")
	TObjectPtr<UAnimToTextureDataAsset> AnimToTextureDataAsset;
};
