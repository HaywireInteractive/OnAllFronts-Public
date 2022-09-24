// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include <MassSoundPerceptionSubsystem.h>
#include <MassCollisionProcessor.h>

#include "MassEnemyTargetFinderProcessor.generated.h"

class UMassNavigationSubsystem;

const float ProjectileRadius = 3.f; // TODO: Use Radius from projectile Data Asset.

const uint8 UMassEnemyTargetFinderProcessor_FinderPhaseCountSqrt = 8;
const uint8 UMassEnemyTargetFinderProcessor_FinderPhaseCount = UMassEnemyTargetFinderProcessor_FinderPhaseCountSqrt * UMassEnemyTargetFinderProcessor_FinderPhaseCountSqrt;
const float UMassEnemyTargetFinderProcessor_SearchRadius = 5000.f; // TODO: don't hard-code
const float UMassEnemyTargetFinderProcessor_CellSize = UMassEnemyTargetFinderProcessor_SearchRadius / (UMassEnemyTargetFinderProcessor_FinderPhaseCountSqrt / 2.0f);

struct FCloseUnhittableEntityData
{
	FCloseUnhittableEntityData() = default;
	FCloseUnhittableEntityData(FCapsule InCapsule, uint8 InPhasesLeft) : Capsule(InCapsule), PhasesLeft(InPhasesLeft) {}
	FCapsule Capsule;
	uint8 PhasesLeft;
};

const uint8 UMassEnemyTargetFinderProcessor_MaxCachedCloseUnhittableEntities = 50;

bool CanEntityDamageTargetEntity(const FTargetEntityFragment& TargetEntityFragment, const FMassEntityView& OtherEntityView);

USTRUCT()
struct PROJECTM_API FTargetEntityFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassEntityHandle Entity;
	float TargetMinCaliberForDamage;

	/** Cache close teammates so when we find a target we can ensure we won't hit teammate. */
	TStaticArray<FCloseUnhittableEntityData, UMassEnemyTargetFinderProcessor_MaxCachedCloseUnhittableEntities> CachedCloseUnhittableEntities;
	uint8 CachedCloseUnhittableEntitiesNextIndex = 0;

	/** The number of cells to search across from where target is looking. The depth is determined by 64 / SearchBreadth so make sure this value is divisible into 64. */
	uint8 SearchBreadth = 8;
};

USTRUCT()
struct PROJECTM_API FTeamMemberFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	bool IsOnTeam1 = true;
};

UCLASS(meta = (DisplayName = "TeamMember"))
class PROJECTM_API UMassTeamMemberTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

public:
	UPROPERTY(Category = "Team", EditAnywhere)
	bool IsOnTeam1 = true;
};

USTRUCT()
struct FMassNeedsEnemyTargetTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassWillNeedEnemyTargetTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct PROJECTM_API FNeedsEnemyTargetSharedParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(Category = "", EditAnywhere)
	int32 ParallelJobCount = 1; // This is not as useful anymore now that we use ParallelForEachEntityChunk since that parallelizes better.

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool DrawSearchAreas = false;
};

UCLASS(meta = (DisplayName = "NeedsEnemyTarget"))
class PROJECTM_API UMassNeedsEnemyTargetTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "", EditAnywhere)
	FNeedsEnemyTargetSharedParameters SharedParameters;

	UPROPERTY(Category = "", EditAnywhere)
	float ProjectileCaliber = 5.f; // TODO: DRY this with the Caliber value in projectile data assets.

	/** The number of cells to search across from where target is looking. The depth is determined by 64 / SearchBreadth so make sure this value is divisible into 64. */
	UPROPERTY(Category = "", EditAnywhere, meta = (ClampMin = 1, ClampMax = 64))
	uint8 SearchBreadth = 8;
};

UCLASS()
class PROJECTM_API UMassEnemyTargetFinderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassEnemyTargetFinderProcessor();
	static const float GetProjectileSpawnLocationZOffset(const bool& bIsSoldier);

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	TObjectPtr<UMassSoundPerceptionSubsystem> SoundPerceptionSubsystem;
	FMassEntityQuery EntityQuery;
	uint8 FinderPhase = 0;
};
