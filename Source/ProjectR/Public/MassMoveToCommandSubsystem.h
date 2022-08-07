// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassMoveToCommandSubsystem.generated.h"

class UMassAgentSubsystem;
class UMassSignalSubsystem;

/**
 * 
 */
UCLASS()
class PROJECTR_API UMassMoveToCommandSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	const FVector* GetLastMoveToCommandTarget() const;
	const bool IsLastMoveToCommandForTeam1() const;
	void SetMoveToCommandTarget(const FVector target, const bool bIsOnTeam1);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void RegisterEntity(const FMassEntityHandle Entity);
	void UnregisterEntity(FMassEntityHandle Entity);

	UPROPERTY()
	UMassSignalSubsystem* SignalSubsystem;

	UPROPERTY()
	UMassAgentSubsystem* AgentSubsystem;

	UPROPERTY()
	TSet<FMassEntityHandle> Entities;

	UPROPERTY()
	FVector MoveToCommandTarget = FVector::ZeroVector;

	UPROPERTY()
	bool bIsLastMoveToCommandForTeam1;
};
