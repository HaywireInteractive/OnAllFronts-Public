// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "MassStateTreeTypes.h"
#include "MassNavigationFragments.h"
#include "MassCommonFragments.h"
#include "MassMoveToCommandProcessor.h"
#include "MassLookAtViaMoveTargetTask.generated.h"

USTRUCT()
struct PROJECTM_API FMassLookAtViaMoveTargetTaskInstanceData
{
	GENERATED_BODY()

		UPROPERTY(EditAnywhere, Category = Input)
		FMassEntityHandle TargetEntity;
};

USTRUCT(meta = (DisplayName = "Mass LookAtViaMoveTarget Task"))
struct PROJECTM_API FMassLookAtViaMoveTargetTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassLookAtViaMoveTargetTaskInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	TStateTreeExternalDataHandle<FMassStashedMoveTargetFragment> StashedMoveTargetHandle;
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;

	TStateTreeInstanceDataPropertyHandle<FMassEntityHandle> TargetEntityHandle;
};
