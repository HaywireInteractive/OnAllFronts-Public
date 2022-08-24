// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassStateTreeTypes.h"
#include "MassEntityTypes.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassGotTargetEvaluator.generated.h"

USTRUCT()
struct PROJECTM_API FMassGotTargetEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bGotTarget = false;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassEntityHandle TargetEntity;
};

USTRUCT(meta = (DisplayName = "Mass GotTarget Eval"))
struct PROJECTM_API FMassGotTargetEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassGotTargetEvaluatorInstanceData::StaticStruct(); }
	virtual void Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FTargetEntityFragment> TargetEntityFragmentHandle;

	TStateTreeInstanceDataPropertyHandle<bool> GotTargetHandle;
	TStateTreeInstanceDataPropertyHandle<FMassEntityHandle> TargetEntityHandle;
};
