// Copied from engine except uses ParallelForEachEntityChunk.

#include "MassFastAvoidanceTrait.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "MassNavigationFragments.h"
#include "Engine/World.h"

void UMassFastObstacleAvoidanceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntitySubsystem);

	BuildContext.AddFragment<FAgentRadiusFragment>();
	BuildContext.AddFragment<FMassNavigationEdgesFragment>();
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FMassForceFragment>();
	BuildContext.AddFragment<FMassMoveTargetFragment>();

	const FMassFastMovingAvoidanceParameters MovingValidated = MovingParameters.GetValidated();
	const uint32 MovingHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(MovingValidated));
	const FConstSharedStruct MovingFragment = EntitySubsystem->GetOrCreateConstSharedFragment(MovingHash, MovingValidated);
	BuildContext.AddConstSharedFragment(MovingFragment);

	const FMassFastStandingAvoidanceParameters StandingValidated = StandingParameters.GetValidated();
	const uint32 StandingHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(StandingValidated));
	const FConstSharedStruct StandingFragment = EntitySubsystem->GetOrCreateConstSharedFragment(StandingHash, StandingValidated);
	BuildContext.AddConstSharedFragment(StandingFragment);
}
