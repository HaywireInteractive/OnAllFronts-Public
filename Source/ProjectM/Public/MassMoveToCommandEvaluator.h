// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassStateTreeTypes.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassMoveToCommandEvaluator.generated.h"

class UMassMoveToCommandSubsystem;

USTRUCT()
struct PROJECTM_API FMassMoveToCommandEvaluatorInstanceData
{
	GENERATED_BODY()

		UPROPERTY(VisibleAnywhere, Category = Output)
		bool bGotMoveToCommand = false;

	UPROPERTY(VisibleAnywhere, Category = Output)
		FVector LastMoveToCommandTarget;
};

USTRUCT(meta = (DisplayName = "Mass MoveToCommand Eval"))
struct PROJECTM_API FMassMoveToCommandEvaluator : public FMassStateTreeEvaluatorBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassMoveToCommandEvaluatorInstanceData::StaticStruct(); }
	virtual void Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<UMassMoveToCommandSubsystem> MoveToCommandSubsystemHandle;
	TStateTreeExternalDataHandle<FTeamMemberFragment> TeamMemberHandle;

	TStateTreeInstanceDataPropertyHandle<bool> GotMoveToCommandHandle;
	TStateTreeInstanceDataPropertyHandle<FVector> LastMoveToCommandTargetHandle;
};
