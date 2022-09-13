// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEnemyTargetFinderProcessor.generated.h"

class UMassNavigationSubsystem;

USTRUCT()
struct PROJECTM_API FTargetEntityFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "")
	float TargetMinCaliberForDamage;
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
	FMassEntityQuery EntityQuery;
	uint8 FinderPhase = 0;
};
