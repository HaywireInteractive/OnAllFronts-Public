// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassSoundPerceptionSubsystem.h"
#include "MassCollisionProcessor.h"

#include "MassEnemyTargetFinderProcessor.generated.h"

struct FTargetEntityFragment;
class UMassNavigationSubsystem;
class UMassTargetFinderSubsystem;

inline bool UMassEnemyTargetFinderProcessor_SkipFindingTargets = false;
inline FAutoConsoleVariableRef CVarUMassEnemyTargetFinderProcessor_SkipFindingTargets(TEXT("pm.UMassEnemyTargetFinderProcessor_SkipFindingTargets"), UMassEnemyTargetFinderProcessor_SkipFindingTargets, TEXT("UMassEnemyTargetFinderProcessor: Skip Finding Targets"));

constexpr float UMassEnemyTargetFinder_FinestCellSize = 5000.f; // TODO: Don't hard-code, get from data asset.
constexpr float ProjectileRadius = 3.f; // TODO: Use Radius from projectile Data Asset.

bool CanEntityDamageTargetEntity(const FTargetEntityFragment& TargetEntityFragment, const float& MinCaliberForDamage);
FCapsule GetProjectileTraceCapsuleToTarget(const bool bIsEntitySoldier, const bool bIsTargetEntitySoldier, const FTransform& EntityTransform, const FVector& TargetEntityLocation);
float GetProjectileInitialXYVelocityMagnitude(const bool bIsEntitySoldier);
float GetEntityRange(const bool bIsEntitySoldier);
bool IsTargetEntityVisibleViaSphereTrace(const UWorld& World, const FVector& StartLocation, const FVector& EndLocation, const bool DrawTrace);

USTRUCT()
struct PROJECTM_API FTargetEntityFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The entity that the fragment owner is targeting. */
	FMassEntityHandle Entity;
	
	float TargetMinCaliberForDamage;

	float VerticalAimOffset = 0.f;
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
struct FMassTrackSoundTag : public FMassTag
{
	GENERATED_BODY()
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

UCLASS(meta = (DisplayName = "NeedsEnemyTarget"))
class PROJECTM_API UMassNeedsEnemyTargetTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "", EditAnywhere)
	float ProjectileCaliber = 5.f; // TODO: DRY this with the Caliber value in projectile data assets.
};

UCLASS()
class PROJECTM_API UMassEnemyTargetFinderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassEnemyTargetFinderProcessor();
	static float GetProjectileSpawnLocationZOffset(const bool& bIsSoldier);
	static FVector GetProjectileSpawnLocationOffset(const FTransform& EntityTransform, const bool& bIsSoldier);

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassTargetFinderSubsystem> TargetFinderSubsystem;
	FMassEntityQuery PreSphereTraceEntityQuery;
	FMassEntityQuery PostSphereTraceEntityQuery;
};
