#pragma once

#include "MassEntityTraitBase.h"

#include "MassAgentRadiusTrait.generated.h"

// Usually you can add the FAgentRadiusFragment via UMassAssortedFragmentsTrait but if you want to use inheritance in data assets just for agent radius, you need a trait.
UCLASS(meta = (DisplayName = "AgentRadius"))
class PROJECTM_API UMassAgentRadiusTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

public:
	UPROPERTY(EditAnywhere, Category = "")
	float Radius = 40.f;
};

