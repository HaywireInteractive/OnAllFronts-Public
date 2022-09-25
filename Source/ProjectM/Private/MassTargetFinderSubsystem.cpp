#include "MassTargetFinderSubsystem.h"

#include "MassSimulationSubsystem.h"

UMassTargetFinderSubsystem::UMassTargetFinderSubsystem()
	// TODO: Constant here may not be optimal for performance.
	: TargetGrid(UMassEnemyTargetFinder_FinestCellSize)
{
}

void UMassTargetFinderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSimulationSubsystem>();
}
