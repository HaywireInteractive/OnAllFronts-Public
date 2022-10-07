// Copied from engine except uses ParallelForEachEntityChunk.

#pragma once

#include "MassProcessor.h"
#include "MassFastAvoidanceProcessors.generated.h"

PROJECTM_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidance, Warning, All);
PROJECTM_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidanceVelocities, Warning, All);
PROJECTM_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidanceAgents, Warning, All);
PROJECTM_API DECLARE_LOG_CATEGORY_EXTERN(LogAvoidanceObstacles, Warning, All);

class UMassNavigationSubsystem;

/** Experimental: move using cumulative forces to avoid close agents */
UCLASS()
class PROJECTM_API UMassFastMovingAvoidanceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassFastMovingAvoidanceProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UWorld> World;
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};

/** Avoidance while standing. */
UCLASS()
class PROJECTM_API UMassFastStandingAvoidanceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassFastStandingAvoidanceProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UWorld> World;
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};
