// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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
};

UCLASS(BlueprintType)
class PROJECTM_API UMilitaryStructureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
    // USubsystem BEGIN
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // USubsystem END

    void CreateArmyDivision();
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UMilitaryUnit *RootUnit;
};
