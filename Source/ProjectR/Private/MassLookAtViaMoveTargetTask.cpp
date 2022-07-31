// Fill out your copyright notice in the Description page of Project Settings.

#include "MassLookAtViaMoveTargetTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "MassEntitySubsystem.h"
#include "MassMoveTargetCompleteProcessor.h"
#include "MassNavigationTypes.h"

bool FMassLookAtViaMoveTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(TransformHandle);

	Linker.LinkInstanceDataProperty(TargetEntityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassLookAtViaMoveTargetTaskInstanceData, TargetEntity));

	return true;
}

EStateTreeRunStatus FMassLookAtViaMoveTargetTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassMoveTargetFragment& MoveTargetFragment = MassContext.GetExternalData(MoveTargetHandle);
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
	MoveTargetFragment.CreateNewAction(EMassMovementAction::Stand, *World);
	MoveTargetFragment.Forward = EntityTransform.InverseTransformVector(NewGlobalDirection);

	EntitySubsystem.AddTagToEntity(MassContext.GetEntity(), FMassNeedsMoveTargetCompleteSignalTag::StaticStruct());
	return EStateTreeRunStatus::Running;
}

// TODO: Likely not needed (if superclass does same thing).
EStateTreeRunStatus FMassLookAtViaMoveTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Running;
}
