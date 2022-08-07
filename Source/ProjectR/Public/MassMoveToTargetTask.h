// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "MassMovementTypes.h"
#include "MassStateTreeTypes.h"
#include "MassNavigationFragments.h"
#include "MassCommonFragments.h"
#include "MassMoveToTargetTask.generated.h"

USTRUCT()
struct PROJECTR_API FMassMoveToTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FVector TargetLocation;
};

USTRUCT(meta = (DisplayName = "Move To Target"))
struct PROJECTR_API FMassMoveToTargetTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassMoveToTargetTaskInstanceData::StaticStruct(); };
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;

	TStateTreeInstanceDataPropertyHandle<FVector> TargetLocationHandle;
};
