#pragma once

#include "MassEntityTraitBase.h"
#include "MassObserverProcessor.h"
#include "MassEntityConfigAsset.h"

#include "MassSwapEntityOnDestructionTrait.generated.h"

USTRUCT()
struct PROJECTM_API FMassSwapEntityOnDestructionFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	int16 SwappedEntityConfigIndex = -1;
};

UCLASS(meta = (DisplayName = "SwapEntityOnDestruction"))
class PROJECTM_API UMassSwapEntityOnDestructionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(EditAnywhere)
	UMassEntityConfigAsset* SwappedEntityConfig = nullptr;
};

UCLASS()
class PROJECTM_API UMassSwapEntityOnDestructionProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UMassSwapEntityOnDestructionProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
