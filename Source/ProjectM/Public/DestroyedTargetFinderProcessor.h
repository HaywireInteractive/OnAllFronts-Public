// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavigationFragments.h"
#include "DestroyedTargetFinderProcessor.generated.h"

class UMassSignalSubsystem;

void CopyMoveTarget(const FMassMoveTargetFragment& Source, FMassMoveTargetFragment& Destination, const UWorld& World);

UCLASS()
class PROJECTM_API UDestroyedTargetFinderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UDestroyedTargetFinderProcessor();

protected:
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	UPROPERTY(Transient)
	UMassSignalSubsystem* SignalSubsystem;

private:
	FMassEntityQuery EntityQuery;

	// Frame buffer, it gets reset every frame.
	TArray<FMassEntityHandle> TransientEntitiesToSignal;
};
