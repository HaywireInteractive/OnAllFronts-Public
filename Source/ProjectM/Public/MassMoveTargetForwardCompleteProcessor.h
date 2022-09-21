// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityTypes.h"
#include "MassMoveTargetForwardCompleteProcessor.generated.h"

class UMassSignalSubsystem;

USTRUCT()
struct FMassNeedsMoveTargetForwardCompleteSignalTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS()
class PROJECTM_API UMassMoveTargetForwardCompleteProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassMoveTargetForwardCompleteProcessor();

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
