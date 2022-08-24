// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassTrackTargetProcessor.generated.h"

struct FMassMoveTargetFragment;
struct FTransformFragment;
struct FTargetEntityFragment;
class UMassEntitySubsystem;

USTRUCT()
struct FMassTrackTargetTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class PROJECTM_API UMassTrackTargetProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UMassTrackTargetProcessor();

protected:

	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	void UpdateLookAtTrackedEntity(const UMassEntitySubsystem& EntitySubsystem, const FTransformFragment& TransformFragment, const FTargetEntityFragment& TargetEntityFragment, FMassMoveTargetFragment& MoveTargetFragment) const;

	FMassEntityQuery EntityQuery;
};
