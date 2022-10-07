// Copied from engine except uses ParallelForEachEntityChunk.

#pragma once 

#include "MassEntityTraitBase.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "MassFastAvoidanceTrait.generated.h"

USTRUCT()
struct PROJECTM_API FMassFastMovingAvoidanceParameters : public FMassMovingAvoidanceParameters
{
	GENERATED_BODY()

	FMassFastMovingAvoidanceParameters GetValidated() const
	{
		FMassFastMovingAvoidanceParameters Copy = *this;
		Copy.PredictiveAvoidanceTime = FMath::Max(Copy.PredictiveAvoidanceTime, KINDA_SMALL_NUMBER);
		Copy.ObstacleSeparationDistance = FMath::Max(Copy.ObstacleSeparationDistance, KINDA_SMALL_NUMBER);
		Copy.PredictiveAvoidanceDistance = FMath::Max(Copy.PredictiveAvoidanceDistance, KINDA_SMALL_NUMBER);
		Copy.EnvironmentSeparationDistance = FMath::Max(Copy.EnvironmentSeparationDistance, KINDA_SMALL_NUMBER);
		Copy.StartOfPathDuration = FMath::Max(Copy.StartOfPathDuration, KINDA_SMALL_NUMBER);
		Copy.EndOfPathDuration = FMath::Max(Copy.EndOfPathDuration, KINDA_SMALL_NUMBER);

		return Copy;
	}
};

USTRUCT()
struct PROJECTM_API FMassFastStandingAvoidanceParameters : public FMassStandingAvoidanceParameters
{
	GENERATED_BODY()

	FMassFastStandingAvoidanceParameters GetValidated() const
	{
		FMassFastStandingAvoidanceParameters Copy = *this;

		Copy.GhostSteeringReactionTime = FMath::Max(Copy.GhostSteeringReactionTime, KINDA_SMALL_NUMBER);

		return Copy;
	}
};

UCLASS(meta = (DisplayName = "FastAvoidance"))
class PROJECTM_API UMassFastObstacleAvoidanceTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category="")
	FMassFastMovingAvoidanceParameters MovingParameters;
	
	UPROPERTY(EditAnywhere, Category="")
	FMassFastStandingAvoidanceParameters StandingParameters;
};
