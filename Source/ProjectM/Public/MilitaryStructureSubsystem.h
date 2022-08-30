// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MilitaryStructureSubsystem.generated.h"

UCLASS(BlueprintType)
class PROJECTM_API UTreeViewItem : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Text;
};

USTRUCT(BlueprintType)
struct PROJECTM_API FSoldier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Name;
};

UCLASS(BlueprintType)
class PROJECTM_API UMilitaryUnit : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<UMilitaryUnit*> SubUnits;

    UPROPERTY(BlueprintReadOnly)
    uint8 Depth;

    UPROPERTY(BlueprintReadOnly)
    int32 MassEntityIndex;

    UPROPERTY(BlueprintReadOnly)
    int32 MassEntitySerialNumber;

    UPROPERTY(BlueprintReadOnly)
    UMilitaryUnit* Parent;

    UPROPERTY(BlueprintReadOnly)
    bool bIsSoldier = false;

    void RemoveFromParent();
};

UCLASS(BlueprintType)
class PROJECTM_API UMilitaryStructureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

protected:
  UPROPERTY()
  TMap<FMassEntityHandle, UMilitaryUnit*> EntityToUnitMap;

public:
    // USubsystem BEGIN
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // USubsystem END

    // returns number of soldiers
    int32 CreateMilitaryUnit(uint8 MilitaryUnitIndex, bool bIsTeam1);

    void BindUnitToMassEntity(UMilitaryUnit* MilitaryUnit, FMassEntityHandle Entity);
    void DestroyEntity(FMassEntityHandle Entity);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMilitaryUnit* Team1RootUnit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMilitaryUnit* Team2RootUnit;
};
