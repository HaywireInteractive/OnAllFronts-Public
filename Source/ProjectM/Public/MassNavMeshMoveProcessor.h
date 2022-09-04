// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavMeshMoveProcessor.generated.h"

USTRUCT()
struct FMassNeedsNavMeshMoveTag : public FMassTag
{
	GENERATED_BODY()
};


UCLASS()
class PROJECTM_API UMassNavMeshMoveProcessor : public UMassProcessor
{
	GENERATED_BODY()

		UMassNavMeshMoveProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
