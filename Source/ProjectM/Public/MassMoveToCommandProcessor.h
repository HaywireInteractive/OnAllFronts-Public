// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavigationFragments.h"
#include "MassEntityTraitBase.h"
#include "MassMoveToCommandProcessor.generated.h"

class UMassMoveToCommandSubsystem;

USTRUCT()
struct FMassHasStashedMoveTargetTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS(meta = (DisplayName = "Commandable"))
class PROJECTM_API UMassCommandableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

USTRUCT()
struct PROJECTM_API FMassStashedMoveTargetFragment : public FMassMoveTargetFragment
{
	GENERATED_BODY()
};

UCLASS()
class PROJECTM_API UMassMoveToCommandProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UMassMoveToCommandProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassMoveToCommandSubsystem> MoveToCommandSubsystem;
	FMassEntityQuery EntityQuery;
};
