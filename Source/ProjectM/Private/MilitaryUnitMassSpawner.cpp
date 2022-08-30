// Fill out your copyright notice in the Description page of Project Settings.


#include "MilitaryUnitMassSpawner.h"
#include "MassSpawner.h"
#include "MilitaryStructureSubsystem.h"

AMilitaryUnitMassSpawner::AMilitaryUnitMassSpawner()
{
#if WITH_EDITOR
	GetClass()->FindPropertyByName(FName("Count"))->ClearPropertyFlags(CPF_Edit);
#endif

	OnSpawningFinishedEvent.AddDynamic(this, &AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits);
}

void AMilitaryUnitMassSpawner::BeginPlay()
{
	MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);

	const UMassEntityConfigAsset* EntityConfig = EntityTypes[0].GetEntityConfig();
	const FMassEntityTemplate* EntityTemplate = EntityConfig->GetConfig().GetOrCreateEntityTemplate(*this, *EntityConfig)
		if (const UMassEntityConfigAsset* EntityConfig = EntityType.GetEntityConfig())
		{
			const FMassEntityTemplate* EntityTemplate = EntityConfig->GetConfig().GetOrCreateEntityTemplate(*this, *EntityConfig);

		Count = MilitaryStructureSubsystem->CreateMilitaryUnit(MilitaryUnitIndex);

	Super::BeginPlay();
}

void AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits()
{
	int32 Index = 0;
	int32 SubIndex = 0;
	AssignEntitiesToMilitaryUnits(MilitaryStructureSubsystem->RootUnit, Index, SubIndex);
}

void AMilitaryUnitMassSpawner::AssignEntitiesToMilitaryUnits(UMilitaryUnit* MilitaryUnit, int32& Index, int32& SubIndex)
{
	if (MilitaryUnit->bIsSoldier)
	{
		MilitaryStructureSubsystem->BindUnitToMassEntity(MilitaryUnit, AllSpawnedEntities[Index].Entities[SubIndex]);

		// Increment indices.
		if (SubIndex + 1 < AllSpawnedEntities[Index].Entities.Num())
		{
			SubIndex++;
		}
		else
		{
			Index++;
			SubIndex = 0;
		}
	}
	else
	{
		for (UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
		{
			AssignEntitiesToMilitaryUnits(SubUnit, Index, SubIndex);
		}
	}
}
