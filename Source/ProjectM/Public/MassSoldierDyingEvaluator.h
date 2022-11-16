// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassStateTreeTypes.h"
#include "MassEntityTypes.h"
#include "MassEnemyTargetFinderProcessor.h"

#include "MassSoldierDyingEvaluator.generated.h"

USTRUCT()
struct PROJECTM_API FMassSoldierDyingEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bIsDying = false;
};

USTRUCT(meta = (DisplayName = "Mass SoldierDying Eval"))
struct PROJECTM_API FMassSoldierDyingEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassSoldierDyingEvaluatorInstanceData::StaticStruct(); }
	virtual void Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const override;

	TStateTreeInstanceDataPropertyHandle<bool> IsDyingHandle;
};
