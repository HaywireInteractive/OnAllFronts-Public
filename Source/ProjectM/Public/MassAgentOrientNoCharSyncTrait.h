// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "MassAgentTraits.h"
#include "MassCommonFragments.h"
#include "MassTranslator.h"
#include "MassAgentOrientNoCharSyncTrait.generated.h"

USTRUCT()
struct FMassSceneComponentOrientationCopyToMassTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassSceneComponentOrientationCopyToActorTag : public FMassTag
{
	GENERATED_BODY()
};

// Unlike UMassAgentOrientationSyncTrait, this trait doesn't depend on actor being a Character subclass.
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Agent Orientation (Without Character) Sync"))
class PROJECTM_API UMassAgentOrientNoCharSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS()
class PROJECTM_API UMassSceneComponentOrientationToMassTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassSceneComponentOrientationToMassTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

UCLASS()
class PROJECTM_API UMassSceneComponentOrientationToActorTranslator : public UMassTranslator
{
	GENERATED_BODY()

public:
	UMassSceneComponentOrientationToActorTranslator();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
