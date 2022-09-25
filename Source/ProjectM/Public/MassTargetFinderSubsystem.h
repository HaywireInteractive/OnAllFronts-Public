#pragma once

#include "MassEntityTypes.h"
#include "HierarchicalHashGrid2D.h"
#include "Subsystems/WorldSubsystem.h"

#include "MassTargetFinderSubsystem.generated.h"

// TODO: Constants here may not be optimal for performance.
typedef THierarchicalHashGrid2D<2, 2, FMassEntityHandle> FTargetHashGrid2D;	// 2 levels of hierarchy, 4 ratio between levels

UCLASS()
class PROJECTM_API UMassTargetFinderSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UMassTargetFinderSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FTargetHashGrid2D& GetTargetGrid() const { return TargetGrid; }
	FTargetHashGrid2D& GetTargetGridMutable() { return TargetGrid; }

protected:
	FTargetHashGrid2D TargetGrid;
};
