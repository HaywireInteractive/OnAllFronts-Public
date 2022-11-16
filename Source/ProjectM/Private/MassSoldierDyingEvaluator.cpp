// Fill out your copyright notice in the Description page of Project Settings.


#include "MassSoldierDyingEvaluator.h"

#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"

bool FMassSoldierDyingEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkInstanceDataProperty(IsDyingHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassSoldierDyingEvaluatorInstanceData, bIsDying));

	return true;
}

void FMassSoldierDyingEvaluator::Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const
{
	bool& bIsDying = Context.GetInstanceData(IsDyingHandle);

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	bIsDying = MassContext.GetEntitySubsystemExecutionContext().DoesArchetypeHaveTag<FMassSoldierIsDyingTag>();
}
