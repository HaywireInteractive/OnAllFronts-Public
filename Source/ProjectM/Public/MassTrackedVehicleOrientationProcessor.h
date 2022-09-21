// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassTrackedVehicleOrientationProcessor.generated.h"

bool IsTransformFacingDirection(const FTransform& Transform, const FVector& TargetDirection, float* OutCurrentHeadingRadians = nullptr, float* OutDesiredHeadingRadians = nullptr, float* OutDeltaAngleRadians = nullptr, float* OutAbsDeltaAngleRadians = nullptr);

USTRUCT()
struct FMassTrackedVehicleOrientationTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct PROJECTM_API FMassTrackedVehicleOrientationParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	/** Measured in degrees per second. */
	UPROPERTY(EditAnywhere, Category = "Orientation")
	float TurningSpeed;
};

UCLASS(meta = (DisplayName = "Tracked Vehicle Orientation"))
class PROJECTM_API UMassTrackedVehicleOrientationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "")
	FMassTrackedVehicleOrientationParameters Orientation;
};

/** Processor for turning trached vehicles towards their MoveTarget at a constant turning speed. The one used for soldiers UMassSmoothOrientationProcessor cannot be used because it does not turn entity at constant speed. */
UCLASS()
class PROJECTM_API UMassTrackedVehicleOrientationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassTrackedVehicleOrientationProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
