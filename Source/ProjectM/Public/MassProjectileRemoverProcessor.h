// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassProjectileRemoverProcessor.generated.h"

/**
 *
 */
UCLASS()
class PROJECTM_API UMassProjectileRemoverProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassProjectileRemoverProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
