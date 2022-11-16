// Mostly a copy of City Sample MassContextualAnimTask.cpp

#include "MassGenericAnimTask.h"

#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "MassCommandBuffer.h"
#include "Animation/AnimMontage.h"
#include "MassNavigationFragments.h"
#include "MassZoneGraphNavigationUtils.h"

FMassGenericAnimTask::FMassGenericAnimTask()
{
}

bool FMassGenericAnimTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(MontageRequestHandle);
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);

	Linker.LinkInstanceDataProperty(TargetEntityHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassGenericAnimTaskInstanceData, TargetEntity));
	Linker.LinkInstanceDataProperty(DurationHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassGenericAnimTaskInstanceData, Duration));
	Linker.LinkInstanceDataProperty(ComputedDurationHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassGenericAnimTaskInstanceData, ComputedDuration));
	Linker.LinkInstanceDataProperty(TimeHandle, STATETREE_INSTANCEDATA_PROPERTY(FMassGenericAnimTaskInstanceData, Time));

	return true;
}

EStateTreeRunStatus FMassGenericAnimTask::EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const
{
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	float& Time = Context.GetInstanceData(TimeHandle);
	Time = 0.f;

	UE::CrowdInteractionAnim::FRequest AnimRequest;
	AnimRequest.ContextualAnimAsset = ContextualAnimAsset;
	AnimRequest.InteractorRole = InteractorRole;
	AnimRequest.AlignmentTrack = AlignmentTrack;

	FContextualAnimQueryResult& ContextualAnimQueryResult = AnimRequest.QueryResult;
	// If we have a target entity associated, use that to find the best contextual anim to play
	FMassEntityHandle* TargetEntity = Context.GetInstanceDataPtr(TargetEntityHandle); // Optional input
	if (ContextualAnimAsset != nullptr && TargetEntity != nullptr && TargetEntity->IsSet())
	{
		if (const FTransformFragment* TargetTransformFragment = MassContext.GetEntitySubsystem().GetFragmentDataPtr<FTransformFragment>(*TargetEntity))
		{
			const FTransform& TargetTransform = TargetTransformFragment->GetTransform();
			const FTransform& EntityTransform = MassContext.GetExternalData(TransformHandle).GetTransform();

			FContextualAnimQueryParams ContextualAnimQueryParams;
			ContextualAnimQueryParams.bComplexQuery = true;
			ContextualAnimQueryParams.bFindAnimStartTime = true;
			ContextualAnimQueryParams.QueryTransform = EntityTransform;

			// If we don't find a good sync point, grab the closest one.
			if (!ContextualAnimAsset->Query(InteractorRole, ContextualAnimQueryResult, ContextualAnimQueryParams, TargetTransform))
			{
				ContextualAnimQueryParams.bComplexQuery = false;
				ContextualAnimAsset->Query(InteractorRole, ContextualAnimQueryResult, ContextualAnimQueryParams, TargetTransform);
			}
		}
	}
	
	// If we didn't find a proper contextual anim, or it was not set, use a simple montage instead
	if (!ContextualAnimQueryResult.Animation.IsValid())
	{
		ContextualAnimQueryResult.Animation = FallbackMontage;
	}

	float& ComputedDuration = Context.GetInstanceData(ComputedDurationHandle);
	ComputedDuration = Context.GetInstanceData(DurationHandle);
	
	if (const UAnimMontage* Montage = ContextualAnimQueryResult.Animation.Get())
	{
		// Only override movement mode if we have root motion
		if (Montage->HasRootMotion())
		{
			const UWorld* World = Context.GetWorld();
			checkf(World != nullptr, TEXT("A valid world is expected from the execution context"));

			FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
			MoveTarget.CreateNewAction(EMassMovementAction::Animate, *World);
			if (!UE::MassNavigation::ActivateActionAnimate(*World, Context.GetOwner(), MassContext.GetEntity(), MoveTarget))
			{
				return EStateTreeRunStatus::Failed;
			}
		}

		// Grab the task duration from the montage.
		ComputedDuration = Montage->GetPlayLength();
		// Use existing fragment or push one
		FMassGenericMontageFragment* MontageFragment = MassContext.GetExternalDataPtr(MontageRequestHandle);
		if (MontageFragment != nullptr)
		{
			MontageFragment->Request(AnimRequest);
		}
		else
		{
			FMassGenericMontageFragment MontageData;
			MontageData.Request(AnimRequest);
			MassContext.GetEntitySubsystemExecutionContext().Defer().PushCommand(FCommandAddFragmentInstance(MassContext.GetEntity(), FInstancedStruct::Make(MontageData)));
		}
	}

	// A Duration <= 0 indicates that the task runs until a transition in the state tree stops it.
	// Otherwise we schedule a signal to end the task.
	if (ComputedDuration > 0.0f)
	{
		UMassSignalSubsystem& MassSignalSubsystem = MassContext.GetExternalData(MassSignalSubsystemHandle);
		MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::ContextualAnimTaskFinished, MassContext.GetEntity(), ComputedDuration);
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FMassGenericAnimTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const float ComputedDuration = Context.GetInstanceData(ComputedDurationHandle);
	float& Time = Context.GetInstanceData(TimeHandle);

	Time += DeltaTime;
	return ComputedDuration <= 0.0f ? EStateTreeRunStatus::Running : (Time < ComputedDuration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded);
}
