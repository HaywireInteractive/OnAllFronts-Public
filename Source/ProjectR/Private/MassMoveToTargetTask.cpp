// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToTargetTask.h"

#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FMassMoveToTargetTask::Link(FStateTreeLinker& Linker)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, FString::Printf(TEXT("FMassMoveToTargetTask::Link")));

	Linker.LinkExternalData(MoveToTargetHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(TransformHandle);

	Linker.LinkInstanceDataProperty(TargetLocationHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassMoveToTargetTaskInstanceData, TargetLocation));

	return true;
}

EStateTreeRunStatus FMassMoveToTargetTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	FMassMoveToTargetFragment& MoveToTargetFragment = MassContext.GetExternalData(MoveToTargetHandle);
	MoveToTargetFragment.Reset();
	const FVector TargetLocation = Context.GetInstanceData(TargetLocationHandle);
	MoveToTargetFragment.TargetLocation = TargetLocation;

	FMassMoveTargetFragment& MoveTarget = MassContext.GetExternalData(MoveTargetHandle);
	FTransformFragment& TransformFragment = MassContext.GetExternalData(TransformHandle);
	FVector CurrentLocation = TransformFragment.GetTransform().GetLocation();
	MoveTarget.Center = CurrentLocation + FVector(5000.f, 5000.f, 0.f);
	float Distance = (CurrentLocation - MoveTarget.Center).Size();
	MoveTarget.DistanceToGoal = Distance;
	MoveTarget.bOffBoundaries = true;
	MoveTarget.DesiredSpeed.Set(200.f);
	MoveTarget.IntentAtGoal = EMassMovementAction::Stand;
	const UWorld* World = Context.GetWorld();
	MoveTarget.CreateNewAction(EMassMovementAction::Move, *World);

	return EStateTreeRunStatus::Running;
}

void FMassMoveToTargetTask::ExitState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	UE_LOG(LogTemp, Warning, TEXT("FMassMoveToTargetTask::ExitState"));

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassMoveToTargetFragment& MoveToTargetFragment = MassContext.GetExternalData(MoveToTargetHandle);

	MoveToTargetFragment.Reset();
}

EStateTreeRunStatus FMassMoveToTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	//UE_LOG(LogTemp, Warning, TEXT("FMassMoveToTargetTask::Tick"));

	const FMassMoveToTargetFragment& MoveToTargetFragment = Context.GetExternalData(MoveToTargetHandle);

	return MoveToTargetFragment.IsDone() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}
