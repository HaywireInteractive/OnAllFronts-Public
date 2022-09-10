// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassSpawner.h"

#include "MilitaryUnitMassSpawner.generated.h"

// Note that setting Count inherited from AMassSpawner gets ignored.
UCLASS()
class PROJECTM_API AMilitaryUnitMassSpawner : public AMassSpawner
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void BeginAssignEntitiesToMilitaryUnits();

	void AssignEntitiesToMilitaryUnits(UMilitaryUnit* MilitaryUnit, int32& SoldierIndex, int32& VehicleIndex);
	void DoMilitaryUnitSpawning();

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem;
	
	// Indices into AMassSpawner's AllSpawnedEntities.
	int32 AllSpawnedEntitiesSoldierIndex;
	int32 AllSpawnedEntitiesVehicleIndex;

public:
	AMilitaryUnitMassSpawner();

	UPROPERTY(EditAnywhere)
	uint8 MilitaryUnitIndex = 0; // Index into MilitaryUnits in MilitaryStructureSubsystem.cpp; TODO: make this an enum

	UPROPERTY(EditAnywhere)
	bool bIsTeam1;
};
