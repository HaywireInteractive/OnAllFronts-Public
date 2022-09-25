#pragma once

#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "MassTargetGridProcessors.generated.h"

class UMassTargetFinderSubsystem;

USTRUCT()
struct PROJECTM_API FMassInTargetGridTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct PROJECTM_API FMassTargetGridCellLocationFragment : public FMassFragment
{
	GENERATED_BODY()
	FTargetHashGrid2D::FCellLocation CellLoc;
};

/** Processor to update target grid. Mosty a copy of UMassNavigationObstacleGridProcessor. */
UCLASS()
class PROJECTM_API UMassTargetGridProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassTargetGridProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassTargetFinderSubsystem> TargetFinderSubsystem;
	FMassEntityQuery AddToGridEntityQuery;
	FMassEntityQuery UpdateGridEntityQuery;
};

/** Deinitializer processor to remove targets from the target grid. Mostly a copy of UMassNavigationObstacleRemoverProcessor. */
UCLASS()
class PROJECTM_API UMassTargetRemoverProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UMassTargetRemoverProcessor();
	TObjectPtr<UMassTargetFinderSubsystem> TargetFinderSubsystem;

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
