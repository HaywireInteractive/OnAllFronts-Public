// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandSubsystem.h"

#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassComponentHitTypes.h"
#include <MilitaryStructureSubsystem.h>

const FVector* UMassMoveToCommandSubsystem::GetLastMoveToCommandTarget() const
{
  return bHasMoveToCommand ? &MoveToCommandTarget : nullptr;
}

const bool UMassMoveToCommandSubsystem::IsLastMoveToCommandForTeam1() const
{
	return bIsLastMoveToCommandForTeam1;
}

const UMilitaryUnit* UMassMoveToCommandSubsystem::GetLastMoveToCommandMilitaryUnit() const
{
	return MoveToCommandMilitaryUnit;
}

void UMassMoveToCommandSubsystem::SetMoveToCommandTarget(UMilitaryUnit* MilitaryUnit, const FVector target, const bool bIsOnTeam1)
{
	bHasMoveToCommand = true;
	MoveToCommandTarget = target;
	MoveToCommandMilitaryUnit = MilitaryUnit;
	bIsLastMoveToCommandForTeam1 = bIsOnTeam1;
}

void UMassMoveToCommandSubsystem::ResetLastMoveToCommand()
{
	bHasMoveToCommand = false;
}