// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassMoveToCommandProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassCommonFragments.h"

//----------------------------------------------------------------------//
//  UMassCommandableTrait
//----------------------------------------------------------------------//
void UMassCommandableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FMassStashedMoveTargetFragment>();
}

//----------------------------------------------------------------------//
//  UMassMoveToCommandProcessor
//----------------------------------------------------------------------//
UMassMoveToCommandProcessor::UMassMoveToCommandProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassMoveToCommandProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassMoveToCommandProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	MoveToCommandSubsystem = UWorld::GetSubsystem<UMassMoveToCommandSubsystem>(Owner.GetWorld());
}

void ProcessEntity(const FTeamMemberFragment& TeamMemberFragment, FMassMoveTargetFragment& MoveTargetFragment, const bool& IsLastMoveToCommandForTeam1, const FVector& LastMoveToCommandTarget, const UMassEntitySubsystem& EntitySubsystem, const FVector& EntityLocation, const FMassExecutionContext& Context, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const FMassEntityHandle &Entity)
{
	if (TeamMemberFragment.IsOnTeam1 != IsLastMoveToCommandForTeam1)
	{
		return;
	}

	// TODO: would it be faster to have two separate entity queries instead of checking tag here?
	const bool& bUseStashedMoveTarget = Context.DoesArchetypeHaveTag<FMassTrackTargetTag>();
	FMassMoveTargetFragment& MoveTargetFragmentToModify = bUseStashedMoveTarget ? StashedMoveTargetFragment : MoveTargetFragment;

	UWorld* World = EntitySubsystem.GetWorld();
	MoveTargetFragmentToModify.CreateNewAction(EMassMovementAction::Move, *World);
	MoveTargetFragmentToModify.Center = LastMoveToCommandTarget;
	MoveTargetFragmentToModify.Forward = (LastMoveToCommandTarget - EntityLocation).GetSafeNormal();
	float Distance = (EntityLocation - LastMoveToCommandTarget).Size();
	MoveTargetFragmentToModify.DistanceToGoal = Distance;
	MoveTargetFragmentToModify.bOffBoundaries = true;
	MoveTargetFragmentToModify.DesiredSpeed.Set(200.f); // TODO
	MoveTargetFragmentToModify.IntentAtGoal = EMassMovementAction::Stand;

	if (bUseStashedMoveTarget) {
		Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
	}
}

void UMassMoveToCommandProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassMoveToCommandProcessor_Execute);

	if (!MoveToCommandSubsystem)
	{
		return;
	}

	const FVector* LastMoveToCommandTarget = MoveToCommandSubsystem->GetLastMoveToCommandTarget();
	if (!LastMoveToCommandTarget)
	{
		return;
	}

	const bool& IsLastMoveToCommandForTeam1 = MoveToCommandSubsystem->IsLastMoveToCommandForTeam1();

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem, &IsLastMoveToCommandForTeam1, LastMoveToCommandTarget = *LastMoveToCommandTarget](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			ProcessEntity(TeamMemberList[i], MoveTargetList[i], IsLastMoveToCommandForTeam1, LastMoveToCommandTarget, EntitySubsystem, TransformList[i].GetTransform().GetLocation(), Context, StashedMoveTargetList[i], Context.GetEntity(i));
		}
	});

	MoveToCommandSubsystem->ResetLastMoveToCommand();
}
