// Fill out your copyright notice in the Description page of Project Settings.


#include "MilitaryUnitMassSpawner.h"

#include "MassSpawner.h"
#include "MilitaryStructureSubsystem.h"
#include "MassEntityTraitBase.h"
#include "MassEntityConfigAsset.h"
#include "MassEnemyTargetFinderProcessor.h"
#include <Engine/AssetManager.h>
#include <MassSimulationSubsystem.h>
#include "VisualLogger/VisualLogger.h"
#include "Engine/StreamableManager.h"

AMilitaryUnitMassSpawner::AMilitaryUnitMassSpawner()
{
	bAutoSpawnOnBeginPlay = false;
	OnSpawningFinishedEvent.AddDynamic(this, &AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits);
}

void AMilitaryUnitMassSpawner::BeginPlay()
{
	Super::BeginPlay();

	const UMassSimulationSubsystem* MassSimulationSubsystem = UWorld::GetSubsystem<UMassSimulationSubsystem>(GetWorld());
	if (MassSimulationSubsystem == nullptr || MassSimulationSubsystem->IsSimulationStarted())
	{
		DoMilitaryUnitSpawning();
	}
	else
	{

		SimulationStartedHandle = UMassSimulationSubsystem::GetOnSimulationStarted().AddLambda([this](UWorld* InWorld)
		{
			UWorld* World = GetWorld();

			if (World == InWorld)
			{
				DoMilitaryUnitSpawning();
			}
		});
	}
}

void AMilitaryUnitMassSpawner::DoMilitaryUnitSpawning()
{
	// TODO: Get team from EntityTypes (UMassEntityConfigAsset) once figure out linker issue with using FMassSpawnedEntityType::GetEntityConfig(). Then replace bIsTeam1 below.
	// https://forums.unrealengine.com/t/how-to-resolve-unresolved-external-symbol-fmassspawnedentitytype-getentityconfig-error/636923
	// Error	LNK2019	unresolved external symbol "public: class UMassEntityConfigAsset * __cdecl FMassSpawnedEntityType::GetEntityConfig(void)" (? GetEntityConfig@FMassSpawnedEntityType@@QEAAPEAVUMassEntityConfigAsset@@XZ) referenced in function "protected: virtual void __cdecl AMilitaryUnitMassSpawner::BeginPlay(void)" (? BeginPlay@AMilitaryUnitMassSpawner@@MEAAXXZ)

	//bool bFoundTeamMemberTrait = false;
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

	MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);

	TPair<int32, int32> UnitCounts = MilitaryStructureSubsystem->CreateMilitaryUnit(MilitaryUnitIndex, bIsTeam1);

	// no spawn point generators configured. Let user know and fall back to the spawner's location
	if (SpawnDataGenerators.Num() == 0)
	{
		UE_VLOG_UELOG(this, LogTemp, Warning, TEXT("No Spawn Data Generators configured."));
		return;
	}

	AllGeneratedResults.Reset();

	for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
	{
		if (Generator.GeneratorInstance)
		{
			Generator.bDataGenerated = false;
		}
	}

	// Check if it needs loading
	if (StreamingHandle.IsValid() && StreamingHandle->IsActive())
	{
		// @todo, instead of blindly canceling, we should remember what was asked to load with that handle and compare if more is needed?
		StreamingHandle->CancelHandle();
	}
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FMassSpawnedEntityType& EntityType : EntityTypes)
	{
		if (!EntityType.IsLoaded())
		{
			AssetsToLoad.Add(EntityType.EntityConfig.ToSoftObjectPath());
		}
	}

	auto GenerateSpawningPoints = [this, UnitCounts]()
	{
		if (SpawnDataGenerators.Num() != 2 || EntityTypes.Num() != 2)
		{
			UE_VLOG_UELOG(this, LogTemp, Warning, TEXT("AMilitaryUnitMassSpawner needs exactly two EntityTypes and SpawnDataGenerators, first for soldiers, second for vehicles."));
			return;
		}

		uint8 Index = 0;
		for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
		{
			if (Generator.GeneratorInstance)
			{
				const int32 SpawnCount = Index == 0 ? UnitCounts.Key : UnitCounts.Value;
				const int32 OtherIndex = Index == 0 ? 1 : 0;
				EntityTypes[Index].Proportion = 1.f;
				EntityTypes[OtherIndex].Proportion = 0.f;

				FFinishedGeneratingSpawnDataSignature Delegate = FFinishedGeneratingSpawnDataSignature::CreateUObject(this, &AMassSpawner::OnSpawnDataGenerationFinished, &Generator);
				Generator.GeneratorInstance->Generate(*this, EntityTypes, SpawnCount, Delegate);
			}
			Index++;
		}
	};

	if (AssetsToLoad.Num())
	{
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamingHandle = StreamableManager.RequestAsyncLoad(AssetsToLoad, GenerateSpawningPoints);
	}
	else
	{
		GenerateSpawningPoints();
	}
}

void AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits()
{
	int32 Index = 0;
	int32 SubIndex = 0;
	AssignEntitiesToMilitaryUnits(MilitaryStructureSubsystem->GetRootUnitForTeam(bIsTeam1), Index, SubIndex);
	MilitaryStructureSubsystem->DidCompleteAssigningEntitiesToMilitaryUnits(bIsTeam1);
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
