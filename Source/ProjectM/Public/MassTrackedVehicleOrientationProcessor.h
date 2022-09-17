// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassTrackedVehicleOrientationProcessor.generated.h"

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
