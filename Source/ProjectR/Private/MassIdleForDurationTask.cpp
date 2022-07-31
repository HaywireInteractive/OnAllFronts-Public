// Fill out your copyright notice in the Description page of Project Settings.


#include "MassIdleForDurationTask.h"

#include "StateTreeExecutionContext.h"
#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "MassSignalSubsystem.h"

bool FMassIdleForDurationTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassSignalSubsystemHandle);

	Linker.LinkInstanceDataProperty(DurationHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassIdleForDurationTaskInstanceData, Duration));
	Linker.LinkInstanceDataProperty(TimeHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassIdleForDurationTaskInstanceData, Time));

	return true;
}

EStateTreeRunStatus FMassIdleForDurationTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	float& Time = Context.GetInstanceData(TimeHandle);
	Time = 0.f;

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	const float Duration = Context.GetInstanceData(DurationHandle);

	UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity(), Duration);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassIdleForDurationTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	float& Time = Context.GetInstanceData(TimeHandle);
	const float Duration = Context.GetInstanceData(DurationHandle);

	Time += DeltaTime;

	// TODO: If we don't subtract 1 here, we sometimes tick before duration has elapsed and never tick again. Need to figure out why. 
	return (Time < Duration - 1) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}
