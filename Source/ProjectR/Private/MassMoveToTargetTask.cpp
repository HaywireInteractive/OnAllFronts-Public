// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToTargetTask.h"

#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "MassEntitySubsystem.h"
#include "MassMoveTargetCompleteProcessor.h"

bool FMassMoveToTargetTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(TransformHandle);

	Linker.LinkInstanceDataProperty(TargetLocationHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassMoveToTargetTaskInstanceData, TargetLocation));

	return true;
}

EStateTreeRunStatus FMassMoveToTargetTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	FMassMoveTargetFragment& MoveTarget = MassContext.GetExternalData(MoveTargetHandle);
	FTransformFragment& TransformFragment = MassContext.GetExternalData(TransformHandle);
	FVector CurrentLocation = TransformFragment.GetTransform().GetLocation();
	MoveTarget.Center = FVector(0.f, 0.f, 0.f); // TODO
	float Distance = (CurrentLocation - MoveTarget.Center).Size();
	MoveTarget.DistanceToGoal = Distance;
	MoveTarget.bOffBoundaries = true;
	MoveTarget.DesiredSpeed.Set(200.f); // TODO
	MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
	const UWorld* World = Context.GetWorld();
	MoveTarget.CreateNewAction(EMassMovementAction::Move, *World);

	UMassEntitySubsystem& EntitySubsystem = MassContext.GetEntitySubsystem();
	EntitySubsystem.Defer().AddTag<FMassNeedsMoveTargetCompleteSignalTag>(MassContext.GetEntity());

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassMoveToTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	return EStateTreeRunStatus::Succeeded;
}
