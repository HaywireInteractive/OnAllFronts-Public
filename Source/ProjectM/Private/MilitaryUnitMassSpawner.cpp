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
#include "MassSpawnLocationProcessor.h"
#include "MassOrderedSpawnLocationProcessor.h"

AMilitaryUnitMassSpawner::AMilitaryUnitMassSpawner()
{
	bAutoSpawnOnBeginPlay = false;
}

void AMilitaryUnitMassSpawner::BeginPlay()
{
	Super::BeginPlay();

	// We need this check because it seems that this binding may have been serialized to disk on older versions of this actor in certain levels.
	if (!OnSpawningFinishedEvent.IsAlreadyBound(this, &AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits))
	{
		OnSpawningFinishedEvent.AddDynamic(this, &AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits);
	}

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

bool AMilitaryUnitMassSpawner_SpawnVehiclesOnly = false;
FAutoConsoleVariableRef CVar_AMilitaryUnitMassSpawner_SpawnVehiclesOnly(TEXT("pm.AMilitaryUnitMassSpawner_SpawnVehiclesOnly"), AMilitaryUnitMassSpawner_SpawnVehiclesOnly, TEXT("AMilitaryUnitMassSpawner_SpawnVehiclesOnly"));

bool AMilitaryUnitMassSpawner_SpawnTeam1SoldiersOnly = false;
FAutoConsoleVariableRef CVar_AMilitaryUnitMassSpawner_SpawnTeam1SoldiersOnly(TEXT("pm.AMilitaryUnitMassSpawner_SpawnTeam1SoldiersOnly"), AMilitaryUnitMassSpawner_SpawnTeam1SoldiersOnly, TEXT("AMilitaryUnitMassSpawner_SpawnTeam1SoldiersOnly"));

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

	UnitCounts = MilitaryStructureSubsystem->CreateMilitaryUnit(MilitaryUnitIndex, bIsTeam1);

	// No spawn point generators configured. Let user know and fall back to the spawner's location.
	if (SpawnDataGenerators.Num() == 0)
	{
		UE_VLOG_UELOG(this, LogTemp, Error, TEXT("No Spawn Data Generators configured."));
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

	bDidSpawnSoldiersOnly = AMilitaryUnitMassSpawner_SpawnSoldiersOnly || (bIsTeam1 && AMilitaryUnitMassSpawner_SpawnTeam1SoldiersOnly);
	bDidSpawnVehiclesOnly = bSpawnVehiclesOnly || AMilitaryUnitMassSpawner_SpawnVehiclesOnly;

	auto GenerateSpawningPoints = [this, &UnitCounts = UnitCounts]()
	{
		if (SpawnDataGenerators.Num() != 2 || EntityTypes.Num() != 2)
		{
			UE_VLOG_UELOG(this, LogTemp, Error, TEXT("AMilitaryUnitMassSpawner needs exactly two EntityTypes and SpawnDataGenerators, first for soldiers, second for vehicles."));
			return;
		}

		uint8 Index = 0;
		for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
		{
			if (Generator.GeneratorInstance)
			{
				const int32 UpperCommandSoldierCount = UnitCounts.SoldierCount - UnitCounts.SquadCount * GNumSoldiersInSquad;
				const int32 SpawnCount = Index == 0 ? (bDidSpawnVehiclesOnly ? 0 : UnitCounts.SquadCount + UpperCommandSoldierCount) : (bDidSpawnSoldiersOnly ? 0 : UnitCounts.VehicleCount);
				const int32 OtherIndex = Index == 0 ? 1 : 0;
				EntityTypes[Index].Proportion = 1.f;
				EntityTypes[OtherIndex].Proportion = 0.f;

				FFinishedGeneratingSpawnDataSignature Delegate = FFinishedGeneratingSpawnDataSignature::CreateUObject(this, &AMilitaryUnitMassSpawner::OnMilitaryUnitSpawnDataGenerationFinished, &Generator);
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

TArray<FVector> GetRelativePointsForSquad(const FVector& SquadOrigin)
{
	TArray<FVector> Result;

	for (int i = 0; i < GNumSoldiersInSquad; i++)
	{
		const FVector2D Offset(GSquadMemberOffsetsMeters[i] * 100.f * GSquadSpacingScalingFactor);
		Result.Add(SquadOrigin + FVector(Offset, 0.f));
	}

	return Result;
}

void AMilitaryUnitMassSpawner::OnMilitaryUnitSpawnDataGenerationFinished(TConstArrayView<FMassEntitySpawnDataGeneratorResult> ConstResults, FMassSpawnDataGenerator* FinishedGenerator)
{
	const FMassEntitySpawnDataGeneratorResult* Data = ConstResults.GetData();
	int32 ResultsCount = ConstResults.Num();

	TArray<FMassEntitySpawnDataGeneratorResult> ResultArray;

	for (int i = 0; i < ResultsCount; i++)
	{
		const FMassEntitySpawnDataGeneratorResult& Result = Data[i];
		FMassEntitySpawnDataGeneratorResult SquadResult;
		if (!Result.SpawnData.IsValid())
		{
			UE_VLOG_UELOG(this, LogTemp, Error, TEXT("AMilitaryUnitMassSpawner: Invalid spawn data."));
			return;
		}
		const TArray<FTransform>& ResultTransforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>().Transforms;
		SquadResult.NumEntities = UnitCounts.SoldierCount;
		SquadResult.EntityConfigIndex = Result.EntityConfigIndex;

		SquadResult.SpawnDataProcessor = UMassOrderedSpawnLocationProcessor::StaticClass();
		SquadResult.SpawnData.InitializeAs<FMassTransformsSpawnData>();
		FMassTransformsSpawnData& Transforms = SquadResult.SpawnData.GetMutable<FMassTransformsSpawnData>();

		Transforms.Transforms.Reserve(SquadResult.NumEntities);
		for (int SquadIndex = 0; SquadIndex < UnitCounts.SquadCount; SquadIndex++)
		{
			const TArray<FVector>& SquadMemberSpawnLocations = GetRelativePointsForSquad(ResultTransforms[SquadIndex].GetLocation());
			for (const FVector& SquadMemberSpawnLocation : SquadMemberSpawnLocations)
			{
				FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
				Transform.SetLocation(SquadMemberSpawnLocation);
			}
		}
		int32 NumHigherCommandSoldiers = UnitCounts.SoldierCount - GNumSoldiersInSquad * UnitCounts.SquadCount;
		for (int CommandIndex = 0; CommandIndex < NumHigherCommandSoldiers; CommandIndex++)
		{
			FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
			Transform = ResultTransforms[CommandIndex + UnitCounts.SquadCount];
		}

		ResultArray.Add(SquadResult);
	}
	TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results(ResultArray);

	// Rest is copied from AMassSpawner::OnSpawnDataGenerationFinished.

	// @todo: this can be potentially expensive copy for the instanced structs, could there be a way to use move gere instead?
	AllGeneratedResults.Append(Results.GetData(), Results.Num());

	bool bAllSpawnPointsGenerated = true;
	bool bFoundFinishedGenerator = false;
	for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
	{
		if (&Generator == FinishedGenerator)
		{
			Generator.bDataGenerated = true;
			bFoundFinishedGenerator = true;
		}

		bAllSpawnPointsGenerated &= Generator.bDataGenerated;
	}

	checkf(bFoundFinishedGenerator, TEXT("Something went wrong, we are receiving a callback on an unknow spawn point generator"));

	if (bAllSpawnPointsGenerated)
	{
		SpawnGeneratedEntities(AllGeneratedResults);
		AllGeneratedResults.Reset();
	}
}

void GatherSquadsAndHigherCommand(UMilitaryUnit* MilitaryUnit, TArray<UMilitaryUnit*>& OutSquads, TArray<UMilitaryUnit*>& OutHigherCommandSoldiers)
{
	if (MilitaryUnit->bIsVehicle)
	{
		UE_LOG(LogTemp, Error, TEXT("GatherSquadsAndHigherCommand does not support vehicles yet."));
		return;
	}

	if (MilitaryUnit->bIsSoldier)
	{
		if (MilitaryUnit->Depth <= GSquadUnitDepth)
		{
			OutHigherCommandSoldiers.Add(MilitaryUnit);
		}
		return;
	}

	// Not a soldier.
	if (MilitaryUnit->Depth == GSquadUnitDepth)
	{
		OutSquads.Add(MilitaryUnit);
	}
	else
	{
		for (UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
		{
			GatherSquadsAndHigherCommand(SubUnit, OutSquads, OutHigherCommandSoldiers);
		}
	}
}

void AMilitaryUnitMassSpawner::BeginAssignEntitiesToMilitaryUnits()
{
	// TODO: This is a bit hacky, refactor.
	if (const UMassEntityConfigAsset* SoldierEntityConfig = EntityTypes[0].EntityConfig.LoadSynchronous())
	{
		const FMassEntityTemplate* SoldierEntityTemplate = SoldierEntityConfig->GetConfig().GetOrCreateEntityTemplate(*this, *SoldierEntityConfig);
		check(SoldierEntityTemplate);
		if (AllSpawnedEntities[0].TemplateID == SoldierEntityTemplate->GetTemplateID())
		{
			AllSpawnedEntitiesSoldierIndex = 0;
			AllSpawnedEntitiesVehicleIndex = 1;
		}
		else
		{
			AllSpawnedEntitiesSoldierIndex = 1;
			AllSpawnedEntitiesVehicleIndex = 0;
		}
	}

	int32 VehicleIndex = 0;

	TArray<UMilitaryUnit*> Squads;
	TArray<UMilitaryUnit*> HigherCommandSoldiers;
	GatherSquadsAndHigherCommand(MilitaryStructureSubsystem->GetRootUnitForTeam(bIsTeam1), Squads, HigherCommandSoldiers);
	AssignEntitiesToMilitaryUnits(Squads, HigherCommandSoldiers);

	MilitaryStructureSubsystem->DidCompleteAssigningEntitiesToMilitaryUnits(bIsTeam1);
}

void AMilitaryUnitMassSpawner::SafeBindSoldier(UMilitaryUnit* SoldierMilitaryUnit, const TArray<FMassEntityHandle>& SpawnedEntities, int32& EntityIndex)
{
	if (EntityIndex < SpawnedEntities.Num())
	{
		MilitaryStructureSubsystem->BindUnitToMassEntity(SoldierMilitaryUnit, SpawnedEntities[EntityIndex++]);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AMilitaryUnitMassSpawner::SafeBindSoldier: Invalid index."));
	}
}

void AMilitaryUnitMassSpawner::AssignEntitiesToMilitaryUnits(TArray<UMilitaryUnit*>& Squads, TArray<UMilitaryUnit*>& HigherCommandSoldiers)
{
	int32 SoldierIndex = 0;
	for (int32 SquadIndex = 0; SquadIndex < Squads.Num(); SquadIndex++)
	{
		UMilitaryUnit* Squad = Squads[SquadIndex];
		Squad->SquadIndex = SquadIndex;
		int32 SquadMemberIndex = 0;
		AssignEntitiesToSquad(SoldierIndex, Squad, Squad, SquadMemberIndex);
	}

	for (UMilitaryUnit* Soldier : HigherCommandSoldiers)
	{
		SafeBindSoldier(Soldier, AllSpawnedEntities[AllSpawnedEntitiesSoldierIndex].Entities, SoldierIndex);
	}
}

void AMilitaryUnitMassSpawner::AssignEntitiesToSquad(int32& SoldierIndex, UMilitaryUnit* MilitaryUnit, UMilitaryUnit* SquadMilitaryUnit, int32& SquadMemberIndex)
{
	if (MilitaryUnit->bIsSoldier)
	{
		SafeBindSoldier(MilitaryUnit, AllSpawnedEntities[AllSpawnedEntitiesSoldierIndex].Entities, SoldierIndex);
		MilitaryUnit->SquadMemberIndex = SquadMemberIndex++;
		MilitaryUnit->SquadMilitaryUnit = SquadMilitaryUnit;
		MilitaryUnit->SquadIndex = SquadMilitaryUnit->SquadIndex;
	}
	else
	{
		for (UMilitaryUnit* SubUnit : MilitaryUnit->SubUnits)
		{
 			AssignEntitiesToSquad(SoldierIndex, SubUnit, SquadMilitaryUnit, SquadMemberIndex);
		}
	}
}
