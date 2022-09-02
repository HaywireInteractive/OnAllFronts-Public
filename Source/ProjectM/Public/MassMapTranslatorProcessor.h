// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"

#include "MassMapTranslatorProcessor.generated.h"

USTRUCT()
struct PROJECTM_API FMapLocationFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FVector2D MapLocation;
};

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

UCLASS()
class PROJECTM_API UMassMapTranslatorProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassMapTranslatorProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
