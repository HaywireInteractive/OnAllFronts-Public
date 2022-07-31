// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandEvaluator.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassStateTreeExecutionContext.h"

bool FMassMoveToCommandEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveToCommandSubsystemHandle);

	Linker.LinkInstanceDataProperty(GotMoveToCommandHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassMoveToCommandEvaluatorInstanceData, bGotMoveToCommand));
	Linker.LinkInstanceDataProperty(LastMoveToCommandTargetHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassMoveToCommandEvaluatorInstanceData, LastMoveToCommandTarget));

	return true;
}

void FMassMoveToCommandEvaluator::Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const
{
	//UE_LOG(LogTemp, Warning, TEXT("FMassMoveToCommandEvaluator::Evaluate: start"));
	UMassMoveToCommandSubsystem& MoveToCommandSubsystem = Context.GetExternalData(MoveToCommandSubsystemHandle);
	const FVector* MoveToCommandTarget = MoveToCommandSubsystem.GetLastMoveToCommandTarget();

	bool& bGotMoveToCommand = Context.GetInstanceData(GotMoveToCommandHandle);
	FVector& LastMoveToCommandTarget = Context.GetInstanceData(LastMoveToCommandTargetHandle);

	bGotMoveToCommand = false;
	LastMoveToCommandTarget = FVector::ZeroVector;

	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("FMassMoveToCommandEvaluator::Evaluate, MoveToCommandTarget is null: %d"), MoveToCommandTarget == nullptr));

	if (MoveToCommandTarget != nullptr)
	{
		//UE_LOG(LogTemp, Warning, TEXT("FMassMoveToCommandEvaluator::Evaluate: setting bGotMoveToCommand = true"));
		bGotMoveToCommand = true;
		LastMoveToCommandTarget = *MoveToCommandTarget;
	}
}
