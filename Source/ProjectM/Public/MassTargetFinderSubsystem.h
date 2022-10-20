#pragma once

#include "MassEntityTypes.h"
#include "HierarchicalHashGrid2D.h"
#include "Subsystems/WorldSubsystem.h"

#include "MassTargetFinderSubsystem.generated.h"

struct FMassTargetGridItem
{
	FMassTargetGridItem(FMassEntityHandle InEntity, bool bInIsOnTeam1,float InMinCaliberForDamage, bool bInIsSoldier) 
		: Entity(InEntity), bIsOnTeam1(bInIsOnTeam1), MinCaliberForDamage(InMinCaliberForDamage), bIsSoldier(bInIsSoldier)
	{
	}

	FMassTargetGridItem() = default;

	bool operator==(const FMassTargetGridItem& Other) const
	{
		return Entity == Other.Entity;
	}

	FMassEntityHandle Entity;
	bool bIsOnTeam1;
	float MinCaliberForDamage;
	bool bIsSoldier;
};

// We cannot store this data in FMassTargetGridItem because the grid gets updated only when entities move to a new cell.
struct FMassTargetGridItemDynamicData
{
	FMassTargetGridItemDynamicData(FVector Location, FCapsule Capsule)
		: Location(Location), Capsule(Capsule)
	{
	}

	FMassTargetGridItemDynamicData() = default;

	FVector Location;
	FCapsule Capsule;
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

	const TMap<FMassEntityHandle, FMassTargetGridItemDynamicData>& GetTargetDynamicData() const { return TargetDynamicData; }
	TMap<FMassEntityHandle, FMassTargetGridItemDynamicData>& GetTargetDynamicDataMutable() { return TargetDynamicData; }

protected:
	FTargetHashGrid2D TargetGrid;
	TMap<FMassEntityHandle, FMassTargetGridItemDynamicData> TargetDynamicData;
};
