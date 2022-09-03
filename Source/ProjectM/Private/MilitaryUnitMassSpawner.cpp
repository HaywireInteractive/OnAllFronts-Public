// Fill out your copyright notice in the Description page of Project Settings.


#include "MilitaryUnitMassSpawner.h"

#include "MassSpawner.h"
#include "MilitaryStructureSubsystem.h"
#include "MassEntityTraitBase.h"
#include "MassEntityConfigAsset.h"
#include "MassEnemyTargetFinderProcessor.h"

AMilitaryUnitMassSpawner::AMilitaryUnitMassSpawner()
{
	OnSpawningFinishedEvent.AddDynamic(this, &AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits);
}

void AMilitaryUnitMassSpawner::BeginPlay()
{
	MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);

	bool bFoundTeamMemberTrait = false;

	// TODO: Get team from EntityTypes (UMassEntityConfigAsset) once figure out linker issue with using FMassSpawnedEntityType::GetEntityConfig():
	// https://forums.unrealengine.com/t/how-to-resolve-unresolved-external-symbol-fmassspawnedentitytype-getentityconfig-error/636923
	// Error	LNK2019	unresolved external symbol "public: class UMassEntityConfigAsset * __cdecl FMassSpawnedEntityType::GetEntityConfig(void)" (? GetEntityConfig@FMassSpawnedEntityType@@QEAAPEAVUMassEntityConfigAsset@@XZ) referenced in function "protected: virtual void __cdecl AMilitaryUnitMassSpawner::BeginPlay(void)" (? BeginPlay@AMilitaryUnitMassSpawner@@MEAAXXZ)
	
	//const UMassEntityConfigAsset* EntityConfig = EntityTypes[0].GetEntityConfig();
	//TConstArrayView<UMassEntityTraitBase*> Traits = EntityConfig->GetConfig().GetTraits();
	//for (UMassEntityTraitBase* Trait : Traits)
	//{
	//	if (UMassTeamMemberTrait* TeamMemberTrait = Cast<UMassTeamMemberTrait>(Trait))
	//	{
	//		bIsTeam1 = TeamMemberTrait->IsOnTeam1;
	//		bFoundTeamMemberTrait = true;
	//		break;
	//	}
	//}

	//if (!bFoundTeamMemberTrait)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("Could not find UMassTeamMemberTrait, defaulting to team 1"));
	//}

	Count = MilitaryStructureSubsystem->CreateMilitaryUnit(MilitaryUnitIndex, bIsTeam1);

	Super::BeginPlay();
}

void AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits()
{
	int32 Index = 0;
	int32 SubIndex = 0;
	AssignEntitiesToMilitaryUnits(MilitaryStructureSubsystem->GetRootUnitForTeam(bIsTeam1), Index, SubIndex);
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
