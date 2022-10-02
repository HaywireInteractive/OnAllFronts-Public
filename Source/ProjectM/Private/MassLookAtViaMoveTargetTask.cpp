// Fill out your copyright notice in the Description page of Project Settings.

#include "MassLookAtViaMoveTargetTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntitySubsystem.h"
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "InvalidTargetFinderProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassNavigationTypes.h"

bool FMassLookAtViaMoveTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(StashedMoveTargetHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveForwardCompleteSignalHandle);

	Linker.LinkInstanceDataProperty(TargetEntityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassLookAtViaMoveTargetTaskInstanceData, TargetEntity));

	return true;
}

bool StashCurrentMoveTargetIfNeeded(const FMassMoveTargetFragment& MoveTargetFragment, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const UWorld& World, const UMassEntitySubsystem&  EntitySubsystem, const FMassEntityHandle& Entity, const FMassExecutionContext& Context, const bool AddHasStashTag)
{
	const bool bIsEntityCurrentMoving = MoveTargetFragment.GetCurrentAction() == EMassMovementAction::Move && MoveTargetFragment.GetCurrentActionID() > 0;
	if (!bIsEntityCurrentMoving || Context.DoesArchetypeHaveTag<FMassTrackSoundTag>())
	{
		return false;
	}

	if(Context.DoesArchetypeHaveTag<FMassHasStashedMoveTargetTag>())
	{
	  UE_LOG(LogTemp, Error, TEXT("Stashing move target when entity (idx=%d,sn=%d) already has stashed move target"), Entity.Index, Entity.SerialNumber);
	}

	CopyMoveTarget(MoveTargetFragment, StashedMoveTargetFragment, World);
	if (AddHasStashTag)
	{
		EntitySubsystem.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
	}
	return true;
}

EStateTreeRunStatus FMassLookAtViaMoveTargetTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassMoveTargetFragment& MoveTargetFragment = MassContext.GetExternalData(MoveTargetHandle);
	FMassStashedMoveTargetFragment& StashedMoveTargetFragment = MassContext.GetExternalData(StashedMoveTargetHandle);
	const FTransformFragment& TransformFragment = MassContext.GetExternalData(TransformHandle);
	FMassMoveForwardCompleteSignalFragment& MoveForwardCompleteSignalFragment = MassContext.GetExternalData(MoveForwardCompleteSignalHandle);

	const FMassEntityHandle* TargetEntity = Context.GetInstanceDataPtr(TargetEntityHandle);
	if (TargetEntity == nullptr || !TargetEntity->IsSet()) {
		return EStateTreeRunStatus::Failed;
	}
  const UMassEntitySubsystem& EntitySubsystem = MassContext.GetEntitySubsystem();
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
	StashCurrentMoveTargetIfNeeded(MoveTargetFragment, StashedMoveTargetFragment, *World, EntitySubsystem, Entity, MassContext.GetEntitySubsystemExecutionContext());

	MoveTargetFragment.CreateNewAction(EMassMovementAction::Stand, *World);
	MoveTargetFragment.Center = EntityLocation;
	MoveTargetFragment.Forward = NewGlobalDirection;

	EntitySubsystem.Defer().AddTag<FMassTrackTargetTag>(Entity);
	EntitySubsystem.Defer().AddTag<FMassNeedsMoveTargetForwardCompleteSignalTag>(Entity);
	MoveForwardCompleteSignalFragment.SignalType = EMassMoveForwardCompleteSignalType::NewStateTreeTask;

	return EStateTreeRunStatus::Running;
}

// TODO: Likely not needed (if superclass does same thing).
EStateTreeRunStatus FMassLookAtViaMoveTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Succeeded;
}
