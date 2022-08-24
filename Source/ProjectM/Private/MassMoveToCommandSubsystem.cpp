// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandSubsystem.h"

#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassComponentHitTypes.h"

const FVector* UMassMoveToCommandSubsystem::GetLastMoveToCommandTarget() const
{
  return bHasMoveToCommand ? &MoveToCommandTarget : nullptr;
}

const bool UMassMoveToCommandSubsystem::IsLastMoveToCommandForTeam1() const
{
	return bIsLastMoveToCommandForTeam1;
}

void UMassMoveToCommandSubsystem::SetMoveToCommandTarget(const FVector target, const bool bIsOnTeam1)
{
	bHasMoveToCommand = true;
	MoveToCommandTarget = target;
	bIsLastMoveToCommandForTeam1 = bIsOnTeam1;
}

void UMassMoveToCommandSubsystem::ResetLastMoveToCommand()
{
	bHasMoveToCommand = false;
}