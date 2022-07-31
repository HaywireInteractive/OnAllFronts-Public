// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentTraits.h"
#include "MassVisualizationTrait.h"
#include "MassVisualizationComponent.h"
#include "MassUpdateISMProcessor.h"
#include "MassRepresentationSubsystem.h"
#include "MassRepresentationProcessor.h"
#include "MassVisualizationLODProcessor.h"
#include "MassLODCollectorProcessor.h"
#include "MassAgentLocationSyncTrait.generated.h"

class UMassNavigationSubsystem;
struct FMassNavigationObstacleItem;

USTRUCT()
struct FMassProjectileTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassProjectileVisualizationTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassLocationCopyToActorTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassProjectileUpdateCollisionTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS(meta = (DisplayName = "Projectile"))
class PROJECTR_API UMassProjectileTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

UCLASS(meta = (DisplayName = "ProjectileUpdateCollision"))
class PROJECTR_API UMassProjectileUpdateCollisionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

/**
 * 
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Agent Location Sync"))
class PROJECTR_API UMassAgentLocationSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

UCLASS()
class PROJECTR_API ULocationToActorTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	ULocationToActorTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

UCLASS()
class PROJECTR_API UMassProjectileUpdateISMProcessor : public UMassUpdateISMProcessor
{
	GENERATED_BODY()
public:
	UMassProjectileUpdateISMProcessor();

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;
};

UCLASS()
class PROJECTR_API UMassProjectileUpdateISMCollisionsProcessor : public UMassUpdateISMProcessor
{
	GENERATED_BODY()
public:
	UMassProjectileUpdateISMCollisionsProcessor();

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;

	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;
};

UCLASS()
class PROJECTR_API UMassProjectileRepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

public:
	AMassVisualizer* GetVisualizer();

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem END

	void OnPrePhysicsProcessingPhaseStarted(const float DeltaSeconds, const EMassProcessingPhase Phase);

	bool bNeedUpdateCollisions = true;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Projectile Visualization"))
class PROJECTR_API UMassProjectileVisualizationTrait : public UMassVisualizationTrait
{
	GENERATED_BODY()
public:
	UMassProjectileVisualizationTrait();

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

UCLASS(meta = (DisplayName = "Mass Projectile Visualization"))
class PROJECTR_API UMassProjectileVisualizationProcessor : public UMassVisualizationProcessor
{
	GENERATED_BODY()
public:
	UMassProjectileVisualizationProcessor();

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;
};

UCLASS(meta = (DisplayName = "Projectile visualization LOD"))
class PROJECTR_API UMassProjectileVisualizationLODProcessor : public UMassVisualizationLODProcessor
{
	GENERATED_BODY()
public:
	UMassProjectileVisualizationLODProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;
};

UCLASS(meta = (DisplayName = "Projectile LOD Collector"))
class PROJECTR_API UMassProjectileLODCollectorProcessor : public UMassLODCollectorProcessor
{
	GENERATED_BODY()

		UMassProjectileLODCollectorProcessor();

protected:
	virtual void ConfigureQueries() override;
};
