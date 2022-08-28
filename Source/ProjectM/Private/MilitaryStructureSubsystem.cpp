// Fill out your copyright notice in the Description page of Project Settings.


#include "MilitaryStructureSubsystem.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "MyNamespace" // TODO

typedef TTuple<const uint8, FName> TUnitPair;

static const TUnitPair ArmyDivisionUnitSizes[] =  {
    // pairs are in order of (SubUnitCount, UnitName)
    TUnitPair(3, TEXT("Division")),
    TUnitPair(4, TEXT("Brigade")),
    TUnitPair(3, TEXT("Battalion")),
    TUnitPair(3, TEXT("Company")),
    TUnitPair(3, TEXT("Platoon")),
    TUnitPair(2, TEXT("Squad")),
    TUnitPair(3, TEXT("Fire team")),
    TUnitPair(0, TEXT("Soldier")),
};
static const uint8 ArmyDivisionUnitSizes_Count = sizeof(ArmyDivisionUnitSizes)/sizeof(ArmyDivisionUnitSizes[0]);

void RecursivelyCreateUnits(UMilitaryUnit *Unit, uint8 Depth = 0, uint8 Index = 1)
{
    Unit->Depth = Depth;
    Unit->Name = FText::Format(LOCTEXT("TODO", "{0} {1}"), FText::FromName(ArmyDivisionUnitSizes[Depth].Value), Index);
    
    uint8 SubUnitCount = ArmyDivisionUnitSizes[Depth].Key;
    
    if (SubUnitCount == 0)
    {
        return;
    }
    
    UMilitaryUnit* Commander = NewObject<UMilitaryUnit>();
    Commander->Depth = Depth + 1;
    Commander->Name = FText::Format(LOCTEXT("TODO", "{0} Commander"), Unit->Name);
    Unit->SubUnits.Add(Commander);

    for (uint8 i = 0; i < SubUnitCount; i++)
    {
        UMilitaryUnit* SubUnit = NewObject<UMilitaryUnit>();
        RecursivelyCreateUnits(SubUnit, Depth + 1, i + 1);
        Unit->SubUnits.Add(SubUnit);
    }
}

void UMilitaryStructureSubsystem::CreateArmyDivision()
{
    RootUnit = NewObject<UMilitaryUnit>();
    RecursivelyCreateUnits(RootUnit);
}

void PrintMilitaryStructure(UMilitaryUnit *Unit, FString Prefix = FString(""))
{
    UE_LOG(LogTemp, Warning, TEXT("%sName: %s"), *Prefix, *Unit->Name.ToString());
    
    for (UMilitaryUnit* SubUnit: Unit->SubUnits)
    {
        PrintMilitaryStructure(SubUnit, Prefix + FString("  "));
    }
}

void UMilitaryStructureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    CreateArmyDivision();
}
