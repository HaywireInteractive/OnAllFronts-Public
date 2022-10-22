// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassFollowEntityProcessor.generated.h"

UCLASS()
class PROJECTM_API UMassFollowEntityProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UMassFollowEntityProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
