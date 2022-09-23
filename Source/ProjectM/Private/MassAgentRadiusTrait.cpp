#include "MassAgentRadiusTrait.h"

#include "MassCommonFragments.h"

void UMassAgentRadiusTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	FAgentRadiusFragment& AgentRadiusFragment = BuildContext.AddFragment_GetRef<FAgentRadiusFragment>();
	AgentRadiusFragment.Radius = Radius;
}
