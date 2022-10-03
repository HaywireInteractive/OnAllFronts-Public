// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavigationFragments.h"
#include "InvalidTargetFinderProcessor.generated.h"

class UMassTargetFinderSubsystem;
class UMassSignalSubsystem;
struct FCapsule;

void CopyMoveTarget(const FMassMoveTargetFragment& Source, FMassMoveTargetFragment& Destination, const UWorld& World);
bool DidCapsulesCollide(const FCapsule& Capsule1, const FCapsule& Capsule2, const FMassEntityHandle& Entity, const UWorld& World);

UCLASS()
class PROJECTM_API UInvalidTargetFinderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UInvalidTargetFinderProcessor();

protected:
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;
	TObjectPtr<UMassTargetFinderSubsystem> TargetFinderSubsystem;

private:
	FMassEntityQuery BuildQueueEntityQuery;
	FMassEntityQuery InvalidateTargetsEntityQuery;

	// Frame buffer, it gets reset every frame.
	TArray<FMassEntityHandle> TransientEntitiesToSignal;
};
