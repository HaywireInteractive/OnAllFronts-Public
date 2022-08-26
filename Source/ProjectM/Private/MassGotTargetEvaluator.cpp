// Fill out your copyright notice in the Description page of Project Settings.


#include "MassGotTargetEvaluator.h"
#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"

bool FMassGotTargetEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TargetEntityFragmentHandle);

	Linker.LinkInstanceDataProperty(GotTargetHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassGotTargetEvaluatorInstanceData, bGotTarget));
	Linker.LinkInstanceDataProperty(TargetEntityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassGotTargetEvaluatorInstanceData, TargetEntity));

	return true;
}

void FMassGotTargetEvaluator::Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const
{
	bool& bGotTarget = Context.GetInstanceData(GotTargetHandle);
	FMassEntityHandle& TargetEntity = Context.GetInstanceData(TargetEntityHandle);

	bGotTarget = false;
	TargetEntity.Reset();

	const FTargetEntityFragment& TargetEntityFragment = Context.GetExternalData(TargetEntityFragmentHandle);

	if (TargetEntityFragment.Entity.IsValid())
	{
		bGotTarget = true;
		TargetEntity = TargetEntityFragment.Entity;
	}
}
