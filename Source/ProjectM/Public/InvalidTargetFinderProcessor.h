// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavigationFragments.h"
#include "InvalidTargetFinderProcessor.generated.h"

class UMassSignalSubsystem;
class UMassNavigationSubsystem;

void CopyMoveTarget(const FMassMoveTargetFragment& Source, FMassMoveTargetFragment& Destination, const UWorld& World);

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
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;

private:
	FMassEntityQuery EntityQuery;

	// Frame buffer, it gets reset every frame.
	TArray<FMassEntityHandle> TransientEntitiesToSignal;
};
