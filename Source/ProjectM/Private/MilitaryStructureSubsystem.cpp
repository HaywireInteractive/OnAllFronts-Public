// Fill out your copyright notice in the Description page of Project Settings.


#include "MilitaryStructureSubsystem.h"

#include "Internationalization/Internationalization.h"
#include <Kismet/GameplayStatics.h>
#include "MilitaryUnitMassSpawner.h"

#define LOCTEXT_NAMESPACE "MyNamespace" // TODO

struct FMilitaryUnitLevel
{
	FMilitaryUnitLevel(uint8 SubUnitCount, FName Name, uint8 ArmorSubUnitCount = 0)
	{
		this->SubUnitCount = SubUnitCount;
		this->Name = Name;
		this->ArmorSubUnitCount = ArmorSubUnitCount;
	}
	uint8 SubUnitCount;
	FName Name;
	uint8 ArmorSubUnitCount;
};

static const FName GSquadLevelName = TEXT("Squad");

static const FMilitaryUnitLevel GMilitaryUnitLevels[] = {
	FMilitaryUnitLevel(3, TEXT("Corps")),
	FMilitaryUnitLevel(3, TEXT("Division")),
	FMilitaryUnitLevel(4, TEXT("Brigade")),
	FMilitaryUnitLevel(3, TEXT("Battalion"), 2),
	FMilitaryUnitLevel(3, TEXT("Company")),
	FMilitaryUnitLevel(3, TEXT("Platoon")),
	FMilitaryUnitLevel(2, GSquadLevelName),
	FMilitaryUnitLevel(3, TEXT("Fire team")),
	FMilitaryUnitLevel(0, TEXT("Soldier")),
};

static const FMilitaryUnitLevel GArmorUnitLevels[] = {
	FMilitaryUnitLevel(3, TEXT("Company")),
	FMilitaryUnitLevel(2, TEXT("Platoon")),
	FMilitaryUnitLevel(2, TEXT("Section")),
	FMilitaryUnitLevel(0, TEXT("Tank")),
};

static const uint8 GMilitaryUnitLevels_Count = sizeof(GMilitaryUnitLevels) / sizeof(GMilitaryUnitLevels[0]);

FMilitaryUnitCounts RecursivelyCreateArmorUnits(UMilitaryUnit* Unit, UMilitaryUnit* Parent, uint8 GlobalDepth = 0, uint8 ArmorDepth = 0, uint8 Index = 1);

FMilitaryUnitCounts RecursivelyCreateUnits(UMilitaryUnit* Unit, UMilitaryUnit* Parent, uint8 Depth = 0, uint8 Index = 1)
{
	Unit->Parent = Parent;
	Unit->Depth = Depth;
	Unit->Name = FText::Format(LOCTEXT("TODO", "{0} {1}"), FText::FromName(GMilitaryUnitLevels[Depth].Name), Index);
	
	uint8 SubUnitCount = GMilitaryUnitLevels[Depth].SubUnitCount;
	uint8 ArmorSubUnitCount = GMilitaryUnitLevels[Depth].ArmorSubUnitCount;

	FMilitaryUnitCounts Result;

	if (GMilitaryUnitLevels[Depth].Name.IsEqual(GSquadLevelName))
	{
		Result.SquadCount++;
	}

	if (SubUnitCount == 0)
	{
		Unit->bIsSoldier = true;
		Result.SoldierCount += 1;
		return Result;
	}

	UMilitaryUnit* Commander = NewObject<UMilitaryUnit>();
	Commander->bIsSoldier = true;
	Commander->bIsCommander = true;
	Commander->Depth = Depth + 1;
	Commander->Name = FText::Format(LOCTEXT("TODO", "{0} Commander"), Unit->Name);
	Commander->Parent = Unit;
	Unit->Commander = Commander;
	Unit->SubUnits.Add(Commander);
	Result.SoldierCount += 1;

	uint8 IndexOffset = 0;
	if (ArmorSubUnitCount > 0 && !AMilitaryUnitMassSpawner_SpawnSoldiersOnly)
	{
		IndexOffset++;
		UMilitaryUnit* RootArmorUnit = NewObject<UMilitaryUnit>();
		FMilitaryUnitCounts UnitCounts = RecursivelyCreateArmorUnits(RootArmorUnit, Unit, Depth + 1);
		Result.SoldierCount += UnitCounts.SoldierCount;
		Result.VehicleCount += UnitCounts.VehicleCount;
		Result.SquadCount += UnitCounts.SquadCount;
		Unit->SubUnits.Add(RootArmorUnit);
	}

	for (uint8 i = 0; i < SubUnitCount; i++)
	{
		UMilitaryUnit* SubUnit = NewObject<UMilitaryUnit>();
		FMilitaryUnitCounts UnitCounts = RecursivelyCreateUnits(SubUnit, Unit, Depth + 1, IndexOffset + i + 1);
		Result.SoldierCount += UnitCounts.SoldierCount;
		Result.VehicleCount += UnitCounts.VehicleCount;
		Result.SquadCount += UnitCounts.SquadCount;
		Unit->SubUnits.Add(SubUnit);
	}

	return Result;
}

FMilitaryUnitCounts RecursivelyCreateArmorUnits(UMilitaryUnit* Unit, UMilitaryUnit* Parent, uint8 GlobalDepth, uint8 ArmorDepth, uint8 Index)
{
	Unit->Parent = Parent;
	Unit->Depth = GlobalDepth;
	Unit->Name = FText::Format(LOCTEXT("TODO", "{0} {1}"), FText::FromName(GArmorUnitLevels[ArmorDepth].Name), Index);

	uint8 SubUnitCount = GArmorUnitLevels[ArmorDepth].SubUnitCount;

	FMilitaryUnitCounts Result;

	if (SubUnitCount == 0)
	{
		Unit->bIsVehicle = true;
		Result.VehicleCount += 1;
		return Result;
	}

	UMilitaryUnit* Commander = NewObject<UMilitaryUnit>();
	Commander->bIsSoldier = true;
	Commander->bIsCommander = true;
	Commander->Depth = GlobalDepth + 1;
	Commander->Name = FText::Format(LOCTEXT("TODO", "{0} Commander"), Unit->Name);
	Commander->Parent = Unit;
	Unit->Commander = Commander;
	Unit->SubUnits.Add(Commander);
	Result.SoldierCount += 1;

	for (uint8 i = 0; i < SubUnitCount; i++)
	{
		UMilitaryUnit* SubUnit = NewObject<UMilitaryUnit>();
		FMilitaryUnitCounts UnitCounts = RecursivelyCreateArmorUnits(SubUnit, Unit, GlobalDepth + 1, ArmorDepth + 1, i + 1);
		Result.SoldierCount += UnitCounts.SoldierCount;
		Result.VehicleCount += UnitCounts.VehicleCount;
		Unit->SubUnits.Add(SubUnit);
	}

	return Result;
}

//----------------------------------------------------------------------//
//  UMilitaryStructureSubsystem
//----------------------------------------------------------------------//
FMilitaryUnitCounts UMilitaryStructureSubsystem::CreateMilitaryUnit(uint8 MilitaryUnitIndex, bool bIsTeam1)
{
	UMilitaryUnit* RootUnit = NewObject<UMilitaryUnit>();
	FMilitaryUnitCounts Counts = RecursivelyCreateUnits(RootUnit, nullptr, MilitaryUnitIndex);
	if (bIsTeam1)
	{
		Team1RootUnit = RootUnit;
	}
	else
	{
		Team2RootUnit = RootUnit;
	}
	return Counts;
}

void PrintMilitaryStructure(UMilitaryUnit* Unit, FString Prefix = FString(""))
{
	UE_LOG(LogTemp, Warning, TEXT("%sName: %s"), *Prefix, *Unit->Name.ToString());

	for (UMilitaryUnit* SubUnit : Unit->SubUnits)
	{
		PrintMilitaryStructure(SubUnit, Prefix + FString("  "));
	}
}

void UMilitaryStructureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UMilitaryStructureSubsystem::BindUnitToMassEntity(UMilitaryUnit* MilitaryUnit, FMassEntityHandle Entity)
{
	if (EntityToUnitMap.Contains(Entity))
	{
		EntityToUnitMap.Remove(Entity);
	}
	MilitaryUnit->MassEntityIndex = Entity.Index;
	MilitaryUnit->MassEntitySerialNumber = Entity.SerialNumber;
	EntityToUnitMap.Add(Entity, MilitaryUnit);
}

void UMilitaryStructureSubsystem::DestroyEntity(FMassEntityHandle Entity)
{
	UMilitaryUnit** MilitaryUnit = EntityToUnitMap.Find(Entity);
	if (MilitaryUnit)
	{
		(*MilitaryUnit)->RemoveFromParent();
		EntityToUnitMap.Remove(Entity);
	}
}

UMilitaryUnit* UMilitaryStructureSubsystem::GetRootUnitForTeam(const bool bIsTeam1)
{
	return bIsTeam1 ? Team1RootUnit : Team2RootUnit;
}

UMilitaryUnit* UMilitaryStructureSubsystem::GetUnitForEntity(const FMassEntityHandle Entity)
{
	UMilitaryUnit** MilitaryUnit = EntityToUnitMap.Find(Entity);
	if (!MilitaryUnit)
	{
		return nullptr;
	}

	return *MilitaryUnit;
}

void UMilitaryStructureSubsystem::DidCompleteAssigningEntitiesToMilitaryUnits(const bool bIsTeam1)
{
	TArray<AActor*> MilitaryUnitMassSpawners;
	UGameplayStatics::GetAllActorsOfClass(this, AMilitaryUnitMassSpawner::StaticClass(), MilitaryUnitMassSpawners);


	(bIsTeam1 ? bDidCompleteAssigningEntitiesToMilitaryUnitsForTeam1 : bDidCompleteAssigningEntitiesToMilitaryUnitsForTeam2) = true;

	if (MilitaryUnitMassSpawners.Num() == 1)
	{
		OnCompletedAssigningEntitiesToMilitaryUnitsEvent.Broadcast();
	}
	else if (bDidCompleteAssigningEntitiesToMilitaryUnitsForTeam1 && bDidCompleteAssigningEntitiesToMilitaryUnitsForTeam2)
	{
		OnCompletedAssigningEntitiesToMilitaryUnitsEvent.Broadcast();
	}
}

//----------------------------------------------------------------------//
//  UMilitaryUnit
//----------------------------------------------------------------------//
void UMilitaryUnit::RemoveFromParent()
{
	if (Parent->Commander == this)
	{
		Parent->Commander = nullptr;
	}
	Parent->SubUnits.Remove(this);
}

FMassEntityHandle UMilitaryUnit::GetMassEntityHandle() const
{
	return FMassEntityHandle(MassEntityIndex, MassEntitySerialNumber);
}

bool UMilitaryUnit::IsChildOfUnit(const UMilitaryUnit* ParentUnit)
{
	if (!ParentUnit) {
		return false;
	}

	UMilitaryUnit* ChildUnit = this;
	if (ParentUnit->bIsSoldier)
	{
		return ChildUnit == ParentUnit;
	}

	// If we got here, ParentUnit is set to a non-soldier.

	while (ChildUnit)
	{
		if (ChildUnit == ParentUnit) {
			return true;
		}
		ChildUnit = ChildUnit->Parent;
	}

	return false;
}

bool UMilitaryUnit::IsLeafUnit() const
{
	return bIsSoldier || bIsVehicle;
}

