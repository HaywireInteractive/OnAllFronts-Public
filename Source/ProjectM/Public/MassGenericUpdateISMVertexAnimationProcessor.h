// Mostly copied from CitySample MassCrowdUpdateISMVertexAnimationProcessor.h

#pragma once

#include "MassUpdateISMProcessor.h"

#include "MassGenericUpdateISMVertexAnimationProcessor.generated.h"

struct FMassInstancedStaticMeshInfo;
struct FGenericAnimationFragment;

UCLASS()
class PROJECTM_API UMassGenericUpdateISMVertexAnimationProcessor : public UMassUpdateISMProcessor
{
	GENERATED_BODY()
public:
	UMassGenericUpdateISMVertexAnimationProcessor();

	static void UpdateISMVertexAnimation(FMassInstancedStaticMeshInfo& ISMInfo, FGenericAnimationFragment& AnimationData, const float LODSignificance, const float PrevLODSignificance, const int32 NumFloatsToPad = 0);

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;

	/**
	 * Execution method for this processor
	 * @param EntitySubsystem is the system to execute the lambdas on each entity chunk
	 * @param Context is the execution context to be passed when executing the lambdas */
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;
};
