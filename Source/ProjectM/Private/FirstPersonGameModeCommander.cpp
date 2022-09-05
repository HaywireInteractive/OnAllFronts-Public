// Fill out your copyright notice in the Description page of Project Settings.


#include "FirstPersonGameModeCommander.h"

void AFirstPersonGameModeCommander::StartPlay()
{
  Super::StartPlay();

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);
	MilitaryStructureSubsystem->OnCompletedAssigningEntitiesToMilitaryUnitsEvent.AddDynamic(this, &AFirstPersonGameModeCommander::BP_OnCompletedAssigningEntitiesToMilitaryUnits);
}
