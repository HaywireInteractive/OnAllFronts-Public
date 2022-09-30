#include "MassAudioPerceptionProcessor.h"

#include "MassEnemyTargetFinderProcessor.h"
#include "MassLookAtViaMoveTargetTask.h"
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "MassMoveToCommandProcessor.h"
#include "MassProjectileDamageProcessor.h"
#include "MassRepresentationTypes.h"
#include "MassTrackedVehicleOrientationProcessor.h"

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
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveForwardCompleteSignalFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);
}

void ProcessEntityForAudioTarget(UMassSoundPerceptionSubsystem* SoundPerceptionSubsystem, const FTransform& EntityTransform, FMassMoveTargetFragment& MoveTargetFragment, const bool& bIsEntityOnTeam1, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const UMassEntitySubsystem& EntitySubsystem, const FMassEntityHandle& Entity, TQueue<FMassEntityHandle>& TrackingSoundWhileNavigatingQueue, FMassMoveForwardCompleteSignalFragment& MoveForwardCompleteSignalFragment, const bool bIsEntitySoldier)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor.ProcessEntityForAudioTarget");

	UWorld* World = SoundPerceptionSubsystem->GetWorld();
	const FVector& EntityLocation = EntityTransform.GetLocation();
	FVector OutSoundSource;
	const bool& bIsFacingMoveTarget = IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward);
	if (SoundPerceptionSubsystem->GetClosestSoundWithLineOfSightAtLocation(EntityLocation, OutSoundSource, !bIsEntityOnTeam1, bIsEntitySoldier) && bIsFacingMoveTarget)
	{
		const bool bDidStashCurrentMoveTarget = StashCurrentMoveTargetIfNeeded(MoveTargetFragment, StashedMoveTargetFragment, *World, EntitySubsystem, Entity, false);
		if (bDidStashCurrentMoveTarget)
		{
			TrackingSoundWhileNavigatingQueue.Enqueue(Entity);
			MoveForwardCompleteSignalFragment.SignalType = EMassMoveForwardCompleteSignalType::TrackSoundComplete;
		}

		const FVector& NewGlobalDirection = (OutSoundSource - EntityLocation).GetSafeNormal();
		MoveTargetFragment.CreateNewAction(EMassMovementAction::Stand, *World);
		MoveTargetFragment.Center = EntityLocation;
		MoveTargetFragment.Forward = NewGlobalDirection;
		MoveTargetFragment.DistanceToGoal = 0.f;
		MoveTargetFragment.bOffBoundaries = true;
		MoveTargetFragment.DesiredSpeed.Set(0.f);
		MoveTargetFragment.IntentAtGoal = bDidStashCurrentMoveTarget ? EMassMovementAction::Move : EMassMovementAction::Stand;

		if (UE::Mass::Debug::IsDebuggingEntity(Entity))
		{
			AsyncTask(ENamedThreads::GameThread, [World, EntityLocation, OutSoundSource]()
				{
					FVector VerticalOffset(0.f, 0.f, 400.f);
					DrawDebugDirectionalArrow(World, EntityLocation + VerticalOffset, OutSoundSource + VerticalOffset, 100.f, FColor::Green, false, 5.f);
				});
		}
	}
}

void UMassAudioPerceptionProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassAudioPerceptionProcessor");

	if (UMassEnemyTargetFinderProcessor_SkipFindingTargets)
	{
		return;
	}

	TQueue<FMassEntityHandle> TrackingSoundWhileNavigatingQueue;

	auto ExecuteFunction = [&EntitySubsystem, &SoundPerceptionSubsystem = SoundPerceptionSubsystem, &TrackingSoundWhileNavigatingQueue](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassMoveForwardCompleteSignalFragment> MoveForwardCompleteSignalList = Context.GetMutableFragmentView<FMassMoveForwardCompleteSignalFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
			const bool& bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
			ProcessEntityForAudioTarget(SoundPerceptionSubsystem, LocationList[EntityIndex].GetTransform(), MoveTargetList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, StashedMoveTargetList[EntityIndex], EntitySubsystem, Entity, TrackingSoundWhileNavigatingQueue, MoveForwardCompleteSignalList[EntityIndex], bIsEntitySoldier);
		}
	};

  EntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);

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
