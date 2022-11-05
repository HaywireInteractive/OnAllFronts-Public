// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandSubsystem.h"

#include <MilitaryStructureSubsystem.h>

void UMassMoveToCommandSubsystem::EnqueueMoveToCommand(const UMilitaryUnit* MilitaryUnit, const FVector Target, const bool bIsOnTeam1)
{
	MoveToCommandQueue.Enqueue(FMoveToCommand(MilitaryUnit, Target, bIsOnTeam1));
}

bool UMassMoveToCommandSubsystem::DequeueMoveToCommand(FMoveToCommand& OutMoveToCommand)
{
	if (MoveToCommandQueue.IsEmpty())
	{
		return false;
	}

	const bool bDidDequeue = MoveToCommandQueue.Dequeue(OutMoveToCommand);
	check(bDidDequeue);
	return true;
}
