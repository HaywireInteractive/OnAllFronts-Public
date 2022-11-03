// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandSubsystem.h"

#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassComponentHitTypes.h"
#include <MilitaryStructureSubsystem.h>

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

void UMassMoveToCommandSubsystem::EnqueueMoveToCommand(UMilitaryUnit* MilitaryUnit, const FVector Target, const bool bIsOnTeam1)
{
	MoveToCommandQueue.Enqueue(FMoveToCommand(MilitaryUnit, Target, bIsOnTeam1));
}
