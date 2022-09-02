// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"

// TODO: rename files to MassMapDisplayableTrait.h/.cpp
#include "MassMapTranslatorProcessor.generated.h"

UCLASS(meta = (DisplayName = "MapDisplayable"))
class PROJECTM_API UMassMapDisplayableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;
};

USTRUCT()
struct FMassMapDisplayableTag : public FMassTag
{
	GENERATED_BODY()
};
