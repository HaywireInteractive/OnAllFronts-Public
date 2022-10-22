// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MilitaryStructureSubsystem.generated.h"

struct FMilitaryUnitCounts
{
	int32 SoldierCount = 0;
	int32 VehicleCount = 0;
	int32 SquadCount = 0;
};

constexpr int32 GSquadUnitDepth = 6; // TODO: calculate dynamically?

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
	UMilitaryUnit* Commander;

	// TODO: make private and expose getter
	UPROPERTY(BlueprintReadOnly)
	bool bIsSoldier = false;

	// TODO: make private and expose getter
	UPROPERTY(BlueprintReadOnly)
	bool bIsVehicle = false;

	// TODO: make private and expose getter
	UPROPERTY(BlueprintReadOnly)
	bool bIsCommander = false;

	// TODO: make private and expose getter
	UPROPERTY(BlueprintReadOnly)
	bool bIsPlayer = false;

	void RemoveFromParent();
	FMassEntityHandle GetMassEntityHandle();
	bool IsChildOfUnit(const UMilitaryUnit* ParentUnit);

	UFUNCTION(BlueprintCallable)
	bool IsLeafUnit() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCompletedAssigningEntitiesToMilitaryUnitsEvent);

UCLASS(BlueprintType)
class PROJECTM_API UMilitaryStructureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	bool bDidCompleteAssigningEntitiesToMilitaryUnitsForTeam1;
	bool bDidCompleteAssigningEntitiesToMilitaryUnitsForTeam2;

protected:
	UPROPERTY()
	TMap<FMassEntityHandle, UMilitaryUnit*> EntityToUnitMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UMilitaryUnit* Team1RootUnit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UMilitaryUnit* Team2RootUnit;

public:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem END

	FMilitaryUnitCounts CreateMilitaryUnit(uint8 MilitaryUnitIndex, bool bIsTeam1);

	void BindUnitToMassEntity(UMilitaryUnit* MilitaryUnit, FMassEntityHandle Entity);
	void DestroyEntity(FMassEntityHandle Entity);
	
	UMilitaryUnit* GetUnitForEntity(const FMassEntityHandle Entity);
	UMilitaryUnit* GetRootUnitForTeam(const bool bIsTeam1);

	void DidCompleteAssigningEntitiesToMilitaryUnits(const bool bIsTeam1);

	FCompletedAssigningEntitiesToMilitaryUnitsEvent OnCompletedAssigningEntitiesToMilitaryUnitsEvent;
};
