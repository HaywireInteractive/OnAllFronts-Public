// Adapted from MassSpawnLocationProcessor.h, just without randomness.

#pragma once 

#include "MassProcessor.h"
#include "MassOrderedSpawnLocationProcessor.generated.h"

UCLASS()
class PROJECTM_API UMassOrderedSpawnLocationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassOrderedSpawnLocationProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
