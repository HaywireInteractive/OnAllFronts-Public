// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassMoveToCommandSubsystem.generated.h"

class UMassAgentSubsystem;
class UMassSignalSubsystem;
class UMilitaryUnit;

struct FMoveToCommand
{
	FMoveToCommand(UMilitaryUnit* MilitaryUnit, FVector Target, bool bIsOnTeam1)
		: MilitaryUnit(MilitaryUnit), Target(Target), bIsOnTeam1(bIsOnTeam1)
	{
	}
	FMoveToCommand() = default;
	UMilitaryUnit* MilitaryUnit;
	FVector Target;
	bool bIsOnTeam1;
};

UCLASS()
class PROJECTM_API UMassMoveToCommandSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool DequeueMoveToCommand(FMoveToCommand& OutMoveToCommand);
	void EnqueueMoveToCommand(UMilitaryUnit* MilitaryUnit, const FVector Target, const bool bIsOnTeam1);

protected:
	TQueue<FMoveToCommand> MoveToCommandQueue;
};
