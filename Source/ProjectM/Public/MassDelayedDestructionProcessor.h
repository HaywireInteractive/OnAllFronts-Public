// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"

#include "MassDelayedDestructionProcessor.generated.h"

USTRUCT()
struct PROJECTM_API FMassDelayedDestructionFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	float SecondsLeftTilDestruction;
};

UCLASS(meta = (DisplayName = "DelayedDestruction"))
class PROJECTM_API UMassDelayedDestructionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "", EditAnywhere)
	float SecondsDelay = 3.0f;
};

UCLASS()
class PROJECTM_API UMassDelayedDestructionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassDelayedDestructionProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
