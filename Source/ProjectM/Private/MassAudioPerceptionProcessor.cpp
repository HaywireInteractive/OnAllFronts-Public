#include "MassAudioPerceptionProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassLookAtViaMoveTargetTask.h"
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "MassMoveToCommandProcessor.h"
#include "MassProjectileDamageProcessor.h"
#include "MassRepresentationTypes.h"
#include "MassTrackedVehicleOrientationProcessor.h"
#include "Containers/BinaryHeap.h"

UMassAudioPerceptionProcessor::UMassAudioPerceptionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::StartPhysics; // TODO: Find a better way to ensure this runs after UMassEnemyTargetFinderProcessor
}

void UMassAudioPerceptionProcessor::Initialize(UObject& Owner)
{
	SoundPerceptionSubsystem = UWorld::GetSubsystem<UMassSoundPerceptionSubsystem>(Owner.GetWorld());
}

void UMassAudioPerceptionProcessor::ConfigureQueries()
{
	PreLineTracesEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PreLineTracesEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	PreLineTracesEntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	PreLineTracesEntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);

	PostLineTracesEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PostLineTracesEntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	PostLineTracesEntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	PostLineTracesEntityQuery.AddRequirement<FMassMoveForwardCompleteSignalFragment>(EMassFragmentAccess::ReadWrite);
	PostLineTracesEntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);
}

struct FSoundTraceData
{
	FSoundTraceData() = default;
	FSoundTraceData(const FMassEntityHandle& Entity, const FVector& TraceStart)
		: Entity(Entity), TraceStart(TraceStart)
	{
	  
	}
	FMassEntityHandle Entity;
	FVector TraceStart;
	TArray<FVector> SoundLocations;
};

void EnqueueClosestSoundsToTraceQueue(TArray<FVector>& CloseSounds, TQueue<FSoundTraceData>& SoundTraceQueue, const FVector& EntityLocation, const bool bIsEntitySoldier, const FMassEntityHandle& Entity)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("EnqueueClosestSoundsToTraceQueue");

  const FVector TraceStart = EntityLocation + FVector(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsEntitySoldier));

	FBinaryHeap<float> CloseSoundsHeap;

  {
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("EnqueueClosestSoundsToTraceQueue.Heapify");

		auto DistanceSqToLocation = [&TraceStart](const FVector& SoundSource) {
			return (SoundSource - TraceStart).SizeSquared();
		};

		int32 i = 0;
		for (const FVector& SoundSource : CloseSounds)
		{
			CloseSoundsHeap.Add(DistanceSqToLocation(SoundSource), i);
			i++;
		}
	}

	FSoundTraceData SoundTraceData(Entity, TraceStart);
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("EnqueueClosestSoundsToTraceQueue.SoundLocations.Add");

	  static constexpr uint8 MaxSoundsToConsider = 3;
    for (uint32 i = 0; i < MaxSoundsToConsider && i < CloseSoundsHeap.Num(); i++)
    {
			CloseSoundsHeap.Pop();
      SoundTraceData.SoundLocations.Add(CloseSounds[CloseSoundsHeap.Top()]);
    }
  }

  {
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("EnqueueClosestSoundsToTraceQueue.Enqueue");
		SoundTraceQueue.Enqueue(SoundTraceData);
  }
}

void ProcessEntityForAudioTarget(UMassSoundPerceptionSubsystem* SoundPerceptionSubsystem, const FTransform& EntityTransform, const FMassMoveTargetFragment& MoveTargetFragment, const bool& bIsEntityOnTeam1, const FMassEntityHandle& Entity, const bool bIsEntitySoldier, TQueue<FSoundTraceData>& SoundTraceQueue)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.ProcessEntityForAudioTarget");

	const FVector& EntityLocation = EntityTransform.GetLocation();
	const bool& bIsFacingMoveTarget = IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward);
	if (!bIsFacingMoveTarget)
	{
		return;
	}
	TArray<FVector> CloseSounds;
	if (SoundPerceptionSubsystem->GetSoundsNearLocation(EntityLocation, CloseSounds, !bIsEntityOnTeam1))
	{
		EnqueueClosestSoundsToTraceQueue(CloseSounds, SoundTraceQueue, EntityLocation, bIsEntitySoldier, Entity);
	}
}

struct FSoundTraceResult
{
	FMassEntityHandle Entity;
	FVector BestSoundLocation;
	FSoundTraceResult(const FMassEntityHandle& Entity, const FVector& BestSoundLocation)
		: Entity(Entity), BestSoundLocation(BestSoundLocation)
	{
	}
	FSoundTraceResult() = default;
};

void DoLineTraces(TQueue<FSoundTraceData>& SoundTraceQueue, const UWorld& World, TMap<FMassEntityHandle, FVector>& OutEntityToBestSoundLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.DoLineTraces");

	TArray<FSoundTraceData> SoundTraces;
	while (!SoundTraceQueue.IsEmpty())
	{
		FSoundTraceData SoundTraceData;
		const bool bSuccess = SoundTraceQueue.Dequeue(SoundTraceData);
		check(bSuccess);
		SoundTraces.Add(SoundTraceData);
	}

	TQueue<FSoundTraceResult> BestSoundLocations;

	ParallelFor(SoundTraces.Num(), [&](const int32 JobIndex)
	{
	  FSoundTraceData SoundTrace = SoundTraces[JobIndex];
		for (int32 i = 0; i < SoundTrace.SoundLocations.Num(); i++)
		{
			const FVector& SoundLocation = SoundTrace.SoundLocations[i];
			bool bHasBlockingHit;
			{
        FHitResult Result;
        TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.DoLineTraces.LineTraceSingleByChannel");
				bHasBlockingHit = World.LineTraceSingleByChannel(Result, SoundTrace.TraceStart, SoundLocation, ECollisionChannel::ECC_Visibility);
			}
			if (!bHasBlockingHit)
			{
				BestSoundLocations.Enqueue(FSoundTraceResult(SoundTrace.Entity, SoundLocation));
				break;
			}
		}
	});

	while (!BestSoundLocations.IsEmpty())
	{
		FSoundTraceResult SoundTraceResult;
		const bool bSuccess = BestSoundLocations.Dequeue(SoundTraceResult);
		check(bSuccess);
		OutEntityToBestSoundLocation.Add(SoundTraceResult.Entity, SoundTraceResult.BestSoundLocation);
	}
}

void PostLineTracesProcessEntity(const FVector& BestSoundLocation, FMassMoveTargetFragment& MoveTargetFragment, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const UWorld& World, const UMassEntitySubsystem& EntitySubsystem, const FMassEntityHandle& Entity, TQueue<FMassEntityHandle>& TrackingSoundWhileNavigatingQueue, FMassMoveForwardCompleteSignalFragment& MoveForwardCompleteSignalFragment, const FVector& EntityLocation, const FMassExecutionContext& Context)
{
	const bool bDidStashCurrentMoveTarget = StashCurrentMoveTargetIfNeeded(MoveTargetFragment, StashedMoveTargetFragment, World, EntitySubsystem, Entity, Context, false);
	if (bDidStashCurrentMoveTarget)
	{
		TrackingSoundWhileNavigatingQueue.Enqueue(Entity);
		MoveForwardCompleteSignalFragment.SignalType = EMassMoveForwardCompleteSignalType::TrackSoundComplete;
	}

	const FVector& NewGlobalDirection = (BestSoundLocation - EntityLocation).GetSafeNormal();
	MoveTargetFragment.CreateNewAction(EMassMovementAction::Stand, World);
	MoveTargetFragment.Center = EntityLocation;
	MoveTargetFragment.Forward = NewGlobalDirection;
	MoveTargetFragment.DistanceToGoal = 0.f;
	MoveTargetFragment.bOffBoundaries = true;
	MoveTargetFragment.DesiredSpeed.Set(0.f);
	MoveTargetFragment.IntentAtGoal = bDidStashCurrentMoveTarget ? EMassMovementAction::Move : EMassMovementAction::Stand;
}

void UMassAudioPerceptionProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor");

	if (UMassEnemyTargetFinderProcessor_SkipFindingTargets)
	{
		return;
	}

	TQueue<FSoundTraceData> SoundTraceQueue;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.Execute.PreLineTracesEntityQuery.ParallelForEachEntityChunk");
		PreLineTracesEntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, [&SoundPerceptionSubsystem = SoundPerceptionSubsystem, &SoundTraceQueue = SoundTraceQueue](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
			const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
				const bool& bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
				ProcessEntityForAudioTarget(SoundPerceptionSubsystem, LocationList[EntityIndex].GetTransform(), MoveTargetList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, Entity, bIsEntitySoldier, SoundTraceQueue);
			}
		});
	}

	if (SoundTraceQueue.IsEmpty())
	{
		return;
	}

	TMap<FMassEntityHandle, FVector> EntityToBestSoundLocation;
	DoLineTraces(SoundTraceQueue, *EntitySubsystem.GetWorld(), EntityToBestSoundLocation);

	if (EntityToBestSoundLocation.IsEmpty())
	{
		return;
	}

	TQueue<FMassEntityHandle> TrackingSoundWhileNavigatingQueue;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.Execute.PostLineTracesEntityQuery.ParallelForEachEntityChunk");
		PostLineTracesEntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, [&EntityToBestSoundLocation, &EntitySubsystem, &TrackingSoundWhileNavigatingQueue](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
			const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
			const TArrayView<FMassMoveForwardCompleteSignalFragment> MoveForwardCompleteSignalList = Context.GetMutableFragmentView<FMassMoveForwardCompleteSignalFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
				if (EntityToBestSoundLocation.Contains(Entity))
				{
					PostLineTracesProcessEntity(EntityToBestSoundLocation[Entity], MoveTargetList[EntityIndex], StashedMoveTargetList[EntityIndex], *EntitySubsystem.GetWorld(), EntitySubsystem, Entity, TrackingSoundWhileNavigatingQueue, MoveForwardCompleteSignalList[EntityIndex], LocationList[EntityIndex].GetTransform().GetLocation(), Context);
				}
			}
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.Execute.ProcessQueues");
		while (!TrackingSoundWhileNavigatingQueue.IsEmpty())
		{
			FMassEntityHandle Entity;
      const bool bSuccess = TrackingSoundWhileNavigatingQueue.Dequeue(Entity);
			check(bSuccess);

			Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
			Context.Defer().AddTag<FMassTrackSoundTag>(Entity);
			Context.Defer().AddTag<FMassNeedsMoveTargetForwardCompleteSignalTag>(Entity);
		}
	}
}
