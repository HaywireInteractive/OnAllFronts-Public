// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "DestroyedTargetFinderProcessor.generated.h"

UCLASS()
class PROJECTR_API UDestroyedTargetFinderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UDestroyedTargetFinderProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
