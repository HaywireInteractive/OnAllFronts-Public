// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandEvaluator.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassStateTreeExecutionContext.h"

bool FMassMoveToCommandEvaluator::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveToCommandSubsystemHandle);
	Linker.LinkExternalData(TeamMemberHandle);

	Linker.LinkInstanceDataProperty(GotMoveToCommandHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassMoveToCommandEvaluatorInstanceData, bGotMoveToCommand));
	Linker.LinkInstanceDataProperty(LastMoveToCommandTargetHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassMoveToCommandEvaluatorInstanceData, LastMoveToCommandTarget));

	return true;
}

void FMassMoveToCommandEvaluator::Evaluate(FStateTreeExecutionContext& Context, const EStateTreeEvaluationType EvalType, const float DeltaTime) const
{
	UMassMoveToCommandSubsystem& MoveToCommandSubsystem = Context.GetExternalData(MoveToCommandSubsystemHandle);
	const FTeamMemberFragment& TeamMemberFragment = Context.GetExternalData(TeamMemberHandle);
	const bool IsLastMoveToCommandForTeam1 = MoveToCommandSubsystem.IsLastMoveToCommandForTeam1();
	const FVector* MoveToCommandTarget = MoveToCommandSubsystem.GetLastMoveToCommandTarget();

	bool& bGotMoveToCommand = Context.GetInstanceData(GotMoveToCommandHandle);
	FVector& LastMoveToCommandTarget = Context.GetInstanceData(LastMoveToCommandTargetHandle);

	bGotMoveToCommand = false;
	LastMoveToCommandTarget = FVector::ZeroVector;

	if (MoveToCommandTarget != nullptr && TeamMemberFragment.IsOnTeam1 == IsLastMoveToCommandForTeam1)
	{
		bGotMoveToCommand = true;
		LastMoveToCommandTarget = *MoveToCommandTarget;
	}
}
