#pragma once

#include "MassEntityTypes.h"
#include "HierarchicalHashGrid2D.h"
#include "Subsystems/WorldSubsystem.h"

#include "MassTargetFinderSubsystem.generated.h"

struct FMassTargetGridItem
{
	FMassTargetGridItem(FMassEntityHandle InEntity, bool bInIsOnTeam1, FVector InLocation, float InMinCaliberForDamage, FCapsule InCapsule, bool bInIsSoldier) 
		: Entity(InEntity), bIsOnTeam1(bInIsOnTeam1), Location(InLocation), MinCaliberForDamage(InMinCaliberForDamage), Capsule(InCapsule), bIsSoldier(bInIsSoldier)
	{
	}

	FMassTargetGridItem() = default;

	bool operator==(const FMassTargetGridItem& Other) const
	{
		return Entity == Other.Entity;
	}

	FMassEntityHandle Entity;
	bool bIsOnTeam1;
	FVector Location;
	float MinCaliberForDamage;
	FCapsule Capsule;
	bool bIsSoldier;
};

// TODO: Constants here may not be optimal for performance.
typedef THierarchicalHashGrid2D<2, 2, FMassTargetGridItem> FTargetHashGrid2D;

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
