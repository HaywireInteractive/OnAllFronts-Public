// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassSpawner.h"

#include "MilitaryUnitMassSpawner.generated.h"

inline bool AMilitaryUnitMassSpawner_SpawnSoldiersOnly = true;
inline FAutoConsoleVariableRef CVar_AMilitaryUnitMassSpawner_SpawnSoldiersOnly(TEXT("pm.AMilitaryUnitMassSpawner_SpawnSoldiersOnly"), AMilitaryUnitMassSpawner_SpawnSoldiersOnly, TEXT("AMilitaryUnitMassSpawner_SpawnSoldiersOnly"));

// Note that setting Count inherited from AMassSpawner gets ignored.
UCLASS()
class PROJECTM_API AMilitaryUnitMassSpawner : public AMassSpawner
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void BeginAssignEntitiesToMilitaryUnits();

	void AssignEntitiesToMilitaryUnits(TArray<UMilitaryUnit*>& Squads, TArray<UMilitaryUnit*>& HigherCommandSoldiers);
	void AssignEntitiesToSquad(int32& SoldierIndex, UMilitaryUnit* MilitaryUnit, UMilitaryUnit* SquadMilitaryUnit, int32& SquadMemberIndex);
	void SafeBindSoldier(UMilitaryUnit* SoldierMilitaryUnit, const TArray<FMassEntityHandle>& SpawnedEntities, int32& EntityIndex);
	void DoMilitaryUnitSpawning();
	void OnMilitaryUnitSpawnDataGenerationFinished(TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results, FMassSpawnDataGenerator* FinishedGenerator);

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem;
	
	// Indices into AMassSpawner's AllSpawnedEntities.
	int32 AllSpawnedEntitiesSoldierIndex;
	int32 AllSpawnedEntitiesVehicleIndex;

	bool bDidSpawnVehiclesOnly = false;
	bool bDidSpawnSoldiersOnly = false;
	FMilitaryUnitCounts UnitCounts;

public:
	AMilitaryUnitMassSpawner();

	UPROPERTY(EditAnywhere)
	uint8 MilitaryUnitIndex = 0; // Index into MilitaryUnits in MilitaryStructureSubsystem.cpp; TODO: make this an enum

	UPROPERTY(EditAnywhere)
	bool bIsTeam1;

	UPROPERTY(EditAnywhere)
	bool bSpawnVehiclesOnly;
};
