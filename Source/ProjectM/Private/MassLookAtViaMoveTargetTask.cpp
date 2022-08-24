// Fill out your copyright notice in the Description page of Project Settings.

#include "MassLookAtViaMoveTargetTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntitySubsystem.h"
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "DestroyedTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassNavigationTypes.h"
#include "StateTreeLinker.h"

bool FMassLookAtViaMoveTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(StashedMoveTargetHandle);
	Linker.LinkExternalData(TransformHandle);

	Linker.LinkInstanceDataProperty(TargetEntityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassLookAtViaMoveTargetTaskInstanceData, TargetEntity));

	return true;
}

void StashCurrentMoveTargetIfNeeded(FMassMoveTargetFragment& MoveTargetFragment, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const UWorld& World, const UMassEntitySubsystem&  EntitySubsystem, const FMassEntityHandle& Entity)
{
	if (MoveTargetFragment.GetCurrentAction() == EMassMovementAction::Move && MoveTargetFragment.GetCurrentActionID() > 0)
	{
		CopyMoveTarget(MoveTargetFragment, StashedMoveTargetFragment, World);
		EntitySubsystem.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
	}
}

EStateTreeRunStatus FMassLookAtViaMoveTargetTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassMoveTargetFragment& MoveTargetFragment = MassContext.GetExternalData(MoveTargetHandle);
	FMassStashedMoveTargetFragment& StashedMoveTargetFragment = MassContext.GetExternalData(StashedMoveTargetHandle);
	const FTransformFragment& TransformFragment = MassContext.GetExternalData(TransformHandle);

	const FMassEntityHandle* TargetEntity = Context.GetInstanceDataPtr(TargetEntityHandle);
	if (TargetEntity == nullptr || !TargetEntity->IsSet()) {
		return EStateTreeRunStatus::Failed;
	}
	UMassEntitySubsystem& EntitySubsystem = MassContext.GetEntitySubsystem();
	if (!EntitySubsystem.IsEntityValid(*TargetEntity)) {
		return EStateTreeRunStatus::Failed;
	}
	const FTransformFragment* TargetTransformFragment = EntitySubsystem.GetFragmentDataPtr<FTransformFragment>(*TargetEntity);
	if (!TargetTransformFragment) {
		return EStateTreeRunStatus::Failed;
	}

	const FTransform& EntityTransform = TransformFragment.GetTransform();
	const FVector EntityLocation = EntityTransform.GetLocation();
	const FVector NewGlobalDirection = (TargetTransformFragment->GetTransform().GetLocation() - EntityLocation).GetSafeNormal();

	const UWorld* World = Context.GetWorld();

	const FMassEntityHandle& Entity = MassContext.GetEntity();
	StashCurrentMoveTargetIfNeeded(MoveTargetFragment, StashedMoveTargetFragment, *World, EntitySubsystem, Entity);

	MoveTargetFragment.CreateNewAction(EMassMovementAction::Stand, *World);
	MoveTargetFragment.Center = EntityLocation;
	MoveTargetFragment.Forward = NewGlobalDirection;

	EntitySubsystem.Defer().AddTag<FMassTrackTargetTag>(Entity);
	EntitySubsystem.Defer().AddTag<FMassNeedsMoveTargetForwardCompleteSignalTag>(Entity);
	return EStateTreeRunStatus::Running;
}

// TODO: Likely not needed (if superclass does same thing).
EStateTreeRunStatus FMassLookAtViaMoveTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Succeeded;
}
