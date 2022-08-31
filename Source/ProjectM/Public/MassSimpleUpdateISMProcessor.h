// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassUpdateISMProcessor.h"
#include "MassEntityTypes.h"
#include "MassSimpleUpdateISMProcessor.generated.h"

USTRUCT()
struct FMassSimpleUpdateISMTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS(meta = (DisplayName = "SimpleUpdateISM"))
class PROJECTM_API UMassSimpleUpdateISMTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

// We need this processor to update ISMs because UMassUpdateISMProcessor is not auto registered and if you try to auto register it, City Sample will crash because it already has its own UMassUpdateISMProcessor replacement (UMassProcessor_CrowdVisualizationCustomData).
UCLASS()
class PROJECTM_API UMassSimpleUpdateISMProcessor : public UMassUpdateISMProcessor
{
	GENERATED_BODY()

	UMassSimpleUpdateISMProcessor();
protected:
	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;
};
