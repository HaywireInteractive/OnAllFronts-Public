// Fill out your copyright notice in the Description page of Project Settings.


#include "FirstPersonGameModeCommander.h"

#include "Kismet/GameplayStatics.h"
#include "MilitaryUnitMassSpawner.h"

void AFirstPersonGameModeCommander::StartPlay()
{
  Super::StartPlay();

	TArray<AActor*> MilitaryUnitMassSpawners;
	UGameplayStatics::GetAllActorsOfClass(this, AMilitaryUnitMassSpawner::StaticClass(), MilitaryUnitMassSpawners);

	if (MilitaryUnitMassSpawners.Num() == 0)
	{
		AFirstPersonGameModeCommander::BP_OnCompletedAssigningEntitiesToMilitaryUnits();
		return;
	}

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);
	MilitaryStructureSubsystem->OnCompletedAssigningEntitiesToMilitaryUnitsEvent.AddDynamic(this, &AFirstPersonGameModeCommander::BP_OnCompletedAssigningEntitiesToMilitaryUnits);
}
