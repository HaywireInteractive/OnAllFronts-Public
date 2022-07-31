// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityTypes.h"
#include "MassMoveTargetCompleteProcessor.generated.h"

class UMassSignalSubsystem;

USTRUCT()
struct FMassNeedsMoveTargetCompleteSignalTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class PROJECTR_API UMassMoveTargetCompleteProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassMoveTargetCompleteProcessor();

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
