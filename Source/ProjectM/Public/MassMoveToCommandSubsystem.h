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
	FMoveToCommand(const UMilitaryUnit* MilitaryUnit, FVector Target, bool bIsOnTeam1)
		: MilitaryUnit(MilitaryUnit), Target(Target), bIsOnTeam1(bIsOnTeam1)
	{
	}
	FMoveToCommand() = default;
	const UMilitaryUnit* MilitaryUnit;
	FVector Target;
	bool bIsOnTeam1;
};

UCLASS()
class PROJECTM_API UMassMoveToCommandSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EnqueueMoveToCommand(const UMilitaryUnit* MilitaryUnit, const FVector Target, const bool bIsOnTeam1);
	bool DequeueMoveToCommand(FMoveToCommand& OutMoveToCommand);

protected:
	TQueue<FMoveToCommand> MoveToCommandQueue;
};
