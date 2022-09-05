// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassMoveToCommandSubsystem.generated.h"

class UMassAgentSubsystem;
class UMassSignalSubsystem;
class UMilitaryUnit;

/**
 *
 */
UCLASS()
class PROJECTM_API UMassMoveToCommandSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	const FVector* GetLastMoveToCommandTarget() const;
	const bool IsLastMoveToCommandForTeam1() const;
	const UMilitaryUnit* GetLastMoveToCommandMilitaryUnit() const;
	void SetMoveToCommandTarget(UMilitaryUnit* MilitaryUnit, const FVector target, const bool bIsOnTeam1);
	void ResetLastMoveToCommand();

protected:
	UPROPERTY()
	FVector MoveToCommandTarget = FVector::ZeroVector;

	UPROPERTY()
	bool bIsLastMoveToCommandForTeam1;

	UPROPERTY()
	bool bHasMoveToCommand = false;

	UPROPERTY()
	UMilitaryUnit* MoveToCommandMilitaryUnit = nullptr;
};
