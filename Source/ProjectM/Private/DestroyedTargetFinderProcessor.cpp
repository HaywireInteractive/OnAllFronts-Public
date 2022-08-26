// Fill out your copyright notice in the Description page of Project Settings.


#include "DestroyedTargetFinderProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassMoveToCommandProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"

void CopyMoveTarget(const FMassMoveTargetFragment& Source, FMassMoveTargetFragment& Destination, const UWorld& World)
{
	Destination.CreateNewAction(Source.GetCurrentAction(), World);
	Destination.bOffBoundaries = Source.bOffBoundaries;
	Destination.Center = Source.Center;
	Destination.Forward = Source.Forward;
	Destination.DistanceToGoal = Source.DistanceToGoal;
	Destination.DesiredSpeed = Source.DesiredSpeed;
	Destination.SlackRadius = Source.SlackRadius;
	Destination.bOffBoundaries = Source.bOffBoundaries;
	Destination.bSteeringFallingBehind = Source.bSteeringFallingBehind;
	Destination.IntentAtGoal = Source.IntentAtGoal;
}

UDestroyedTargetFinderProcessor::UDestroyedTargetFinderProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UDestroyedTargetFinderProcessor::Initialize(UObject& Owner)
{
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UDestroyedTargetFinderProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassWillNeedEnemyTargetTag>(EMassFragmentPresence::All);
}

bool IsTargetEntityOutOfRange(const FMassEntityHandle& TargetEntity, const FVector &EntityLocation, const UMassEntitySubsystem& EntitySubsystem)
{
	const FTransformFragment* TargetTransformFragment = EntitySubsystem.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
	check(TargetTransformFragment);

	const FVector& TargetEntityLocation = TargetTransformFragment->GetTransform().GetLocation();
	const double DistanceBetweenEntities = (TargetEntityLocation - EntityLocation).Size();

	static const double MaxRange = 7000.f; // TODO: make configurable in data asset
	return DistanceBetweenEntities > MaxRange;
}

void ProcessEntity(const FMassExecutionContext& Context, const FMassEntityHandle Entity, const UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment, const FVector &EntityLocation, const FMassStashedMoveTargetFragment* StashedMoveTargetFragment, FMassMoveTargetFragment* MoveTargetFragment, TArray<FMassEntityHandle>& TransientEntitiesToSignal)
{
	FMassEntityHandle& TargetEntity = TargetEntityFragment.Entity;
	bool bTargetEntityWasDestroyed = !EntitySubsystem.IsEntityValid(TargetEntity);
	bool bTargetEntityOutOfRange = !bTargetEntityWasDestroyed && IsTargetEntityOutOfRange(TargetEntity, EntityLocation, EntitySubsystem);
	if (bTargetEntityWasDestroyed || bTargetEntityOutOfRange)
	{
		TargetEntity.Reset();
		TransientEntitiesToSignal.Add(Entity);
		Context.Defer().AddTag<FMassNeedsEnemyTargetTag>(Entity);
		Context.Defer().RemoveTag<FMassWillNeedEnemyTargetTag>(Entity);
		Context.Defer().RemoveTag<FMassTrackTargetTag>(Entity);

		// Unstash move target if needed.
		if (Context.DoesArchetypeHaveTag<FMassHasStashedMoveTargetTag>() && StashedMoveTargetFragment && MoveTargetFragment)
		{
			CopyMoveTarget(*StashedMoveTargetFragment, *MoveTargetFragment, *EntitySubsystem.GetWorld());
			Context.Defer().RemoveTag<FMassHasStashedMoveTargetTag>(Entity);
		}
	}
}

void UDestroyedTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UDestroyedTargetFinderProcessor);

	TransientEntitiesToSignal.Reset();

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem, &TransientEntitiesToSignal = TransientEntitiesToSignal](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();
		const TConstArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			ProcessEntity(Context, Context.GetEntity(EntityIndex), EntitySubsystem, TargetEntityList[EntityIndex], TransformList[EntityIndex].GetTransform().GetLocation(), StashedMoveTargetList.Num() > 0 ? &StashedMoveTargetList[EntityIndex] : nullptr, MoveTargetList.Num() > 0 ? &MoveTargetList[EntityIndex] : nullptr, TransientEntitiesToSignal);
		}
	});

	if (TransientEntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, TransientEntitiesToSignal);
	}
}
