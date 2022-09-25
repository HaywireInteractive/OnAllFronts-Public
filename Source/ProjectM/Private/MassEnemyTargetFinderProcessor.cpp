// Fill out your copyright notice in the Description page of Project Settings.


#include "MassEnemyTargetFinderProcessor.h"

#include "MassEntityView.h"
#include "MassTargetFinderSubsystem.h"
#include "MassLODTypes.h"
#include "MassCommonFragments.h"
#include "MassTrackTargetProcessor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MassProjectileDamageProcessor.h"
#include <MassNavigationTypes.h>
#include "MassNavigationFragments.h"
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "MassTrackedVehicleOrientationProcessor.h"
#include "InvalidTargetFinderProcessor.h"
#include "MassLookAtViaMoveTargetTask.h"
#include "MassTargetGridProcessors.h"

//----------------------------------------------------------------------//
//  UMassTeamMemberTrait
//----------------------------------------------------------------------//
void UMassTeamMemberTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	FTeamMemberFragment& TeamMemberTemplate = BuildContext.AddFragment_GetRef<FTeamMemberFragment>();
	TeamMemberTemplate.IsOnTeam1 = IsOnTeam1;
}

//----------------------------------------------------------------------//
//  UMassNeedsEnemyTargetTrait
//----------------------------------------------------------------------//
void UMassNeedsEnemyTargetTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	FTargetEntityFragment& TargetEntityTemplate = BuildContext.AddFragment_GetRef<FTargetEntityFragment>();
	TargetEntityTemplate.TargetMinCaliberForDamage = ProjectileCaliber;

	BuildContext.AddFragment<FMassMoveForwardCompleteSignalFragment>();
	BuildContext.AddFragment<FMassTargetGridCellLocationFragment>();
	BuildContext.AddTag<FMassNeedsEnemyTargetTag>();
}

//----------------------------------------------------------------------//
//  UMassEnemyTargetFinderProcessor
//----------------------------------------------------------------------//
UMassEnemyTargetFinderProcessor::UMassEnemyTargetFinderProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassEnemyTargetFinderProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveForwardCompleteSignalFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);
}

void UMassEnemyTargetFinderProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	TargetFinderSubsystem = UWorld::GetSubsystem<UMassTargetFinderSubsystem>(Owner.GetWorld());
	SoundPerceptionSubsystem = UWorld::GetSubsystem<UMassSoundPerceptionSubsystem>(Owner.GetWorld());
}

bool CanEntityDamageTargetEntity(const FTargetEntityFragment& TargetEntityFragment, const float& MinCaliberForDamage)
{
	return TargetEntityFragment.TargetMinCaliberForDamage >= MinCaliberForDamage;
}

bool CanEntityDamageTargetEntity(const FTargetEntityFragment& TargetEntityFragment, const FMassEntityView& OtherEntityView)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_CanEntityDamageTargetEntity);

	const FProjectileDamagableFragment* TargetEntityProjectileDamagableFragment = OtherEntityView.GetFragmentDataPtr<FProjectileDamagableFragment>();
	return TargetEntityProjectileDamagableFragment && CanEntityDamageTargetEntity(TargetEntityFragment, TargetEntityProjectileDamagableFragment->MinCaliberForDamage);
}

FCapsule GetProjectileTraceCapsuleToTarget(const bool bIsEntitySoldier, const bool bIsTargetEntitySoldier, const FTransform& EntityTransform, const FVector& TargetEntityLocation)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const FVector ProjectileZOffset(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsEntitySoldier));
	const FVector ProjectileSpawnLocation = EntityLocation + UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationOffset(EntityTransform, bIsEntitySoldier);

	const bool bShouldAimAtFeet = !bIsEntitySoldier && bIsTargetEntitySoldier;
	static const float VerticalBuffer = 50.f; // This is needed because otherwise for tanks we always hit the ground when doing trace. TODO: Come up with a better way to handle this.
	FVector ProjectileTargetLocation = bShouldAimAtFeet ? TargetEntityLocation + FVector(0.f, 0.f, ProjectileRadius + VerticalBuffer) : TargetEntityLocation + ProjectileZOffset;
	return FCapsule(ProjectileSpawnLocation, ProjectileTargetLocation, ProjectileRadius);
}

bool AreCloseUnhittableEntitiesBlockingTarget(const FCapsule& ProjectileTraceCapsule, const TArray<FCapsule>& CloseUnhittableEntities, const FMassEntityHandle& Entity, const UWorld& World)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_AreCloseUnhittableEntitiesBlockingTarget);

	for (const FCapsule& CloseUnhittableCapsule : CloseUnhittableEntities)
	{
		if (DidCapsulesCollide(ProjectileTraceCapsule, CloseUnhittableCapsule, Entity, World))
		{
			return true;
		}
	}

	return false;
}

bool IsTargetEntityVisibleViaSphereTrace(const UWorld& World, const FVector& StartLocation, const FVector& EndLocation, const bool DrawTrace)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_IsTargetEntityVisibleViaSphereTrace);
	FHitResult Result;
	static const float Radius = 20.f; // TODO: don't hard-code
	bool bFoundBlockingHit = UKismetSystemLibrary::SphereTraceSingle(World.GetLevel(0)->Actors[0], StartLocation, EndLocation, Radius, TraceTypeQuery1, false, TArray<AActor*>(), DrawTrace ? EDrawDebugTrace::Type::ForDuration : EDrawDebugTrace::Type::None, Result, false, FLinearColor::Red, FLinearColor::Green, 2.f);
	return !bFoundBlockingHit;
}

float GetEntityRange(const bool bIsEntitySoldier)
{
	return UMassEnemyTargetFinder_FinestCellSize * (bIsEntitySoldier ?  2.f : 4.f); // TODO: Don't hard-code, get from data asset.
}

bool IsTargetEntityOutOfRange(const FVector& EntityLocation, const bool bIsEntitySoldier, const FVector& TargetEntityLocation)
{
	const float DistanceToTarget = (TargetEntityLocation - EntityLocation).Size();
	return DistanceToTarget > GetEntityRange(bIsEntitySoldier);
}

bool IsTargetEntityOutOfRange(const FVector& EntityLocation, const bool bIsEntitySoldier, const FMassEntityView& TargetEntityView)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_IsTargetEntityOutOfRange);

	const FTransformFragment& TargetEntityTransformFragment = TargetEntityView.GetFragmentData<FTransformFragment>();
	const FVector& TargetEntityLocation = TargetEntityTransformFragment.GetTransform().GetLocation();
	return IsTargetEntityOutOfRange(EntityLocation, bIsEntitySoldier, TargetEntityLocation);
}

void GetCloseUnhittableEntities(const TArray<FMassTargetGridItem> &CloseEntities, TArray<FCapsule>& OutCloseUnhittableEntities, const bool& IsEntityOnTeam1, const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment, const FVector& EntityLocation, const bool bIsEntitySoldier)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_GetCloseUnhittableEntities);

	auto AddEntityToCloseUnhittablesIfNeeded = [&Entity, &EntitySubsystem, &EntityLocation, &bIsEntitySoldier, &IsEntityOnTeam1, &OutCloseUnhittableEntities, &TargetEntityFragment](const FMassTargetGridItem& OtherEntity) {
		QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_AddEntityToCloseUnhittablesIfNeeded);

		// Skip self.
		if (OtherEntity.Entity == Entity)
		{
			return;
		}

		// Skip invalid entities.
		if (!EntitySubsystem.IsEntityValid(OtherEntity.Entity))
		{
			return;
		}

		// Skip entities out of range.
		if (IsTargetEntityOutOfRange(EntityLocation, bIsEntitySoldier, OtherEntity.Location))
		{
			return;
		}

		// Same team entities are unhittable.
		if (IsEntityOnTeam1 == OtherEntity.bIsOnTeam1) {
			OutCloseUnhittableEntities.Add(OtherEntity.Capsule);
			return;
		}

		// Undamagable enemies are unhittable.
		if (!CanEntityDamageTargetEntity(TargetEntityFragment, OtherEntity.MinCaliberForDamage))
		{
			OutCloseUnhittableEntities.Add(OtherEntity.Capsule);
		}
	};

	for (const FMassTargetGridItem& OtherEntity : CloseEntities)
	{
		AddEntityToCloseUnhittablesIfNeeded(OtherEntity);
	}
}

bool GetClosestValidEnemy(const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, const FTargetHashGrid2D& TargetGrid, const FTransform& EntityTransform, FMassEntityHandle& OutTargetEntity, const bool& IsEntityOnTeam1, FTargetEntityFragment& TargetEntityFragment, FMassExecutionContext& Context, FVector& OutTargetEntityLocation, bool& bOutIsTargetEntitySoldier)
{
	const UWorld* World = EntitySubsystem.GetWorld();

	const bool& bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const FVector& EntityForwardVector = EntityTransform.GetRotation().GetForwardVector();
	
	const float OffsetMagnitude = GetEntityRange(bIsEntitySoldier) / 2.f; 
	const FVector SearchOffset = EntityForwardVector * OffsetMagnitude;
	const FVector SearchCenter = EntityLocation + SearchOffset;
	const FVector SearchOffsetAbs = SearchOffset.GetAbs();
	const FVector SearchExtent(FMath::Max(SearchOffsetAbs.X, SearchOffsetAbs.Y));

	const FBox SearchBounds(SearchCenter - SearchExtent, SearchCenter + SearchExtent);
	TArray<FMassTargetGridItem> CloseEntities;
	CloseEntities.Reserve(300);

	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_GetClosestValidEnemy_TargetGridQuery);
		TargetGrid.Query(SearchBounds, CloseEntities);
	}

	if (UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [SearchCenter, SearchExtent, World, NumCloseEntities = CloseEntities.Num()]()
		{
			DrawDebugBox(World, SearchCenter, SearchExtent, FColor::Green, false, 1.f);
			DrawDebugString(World, SearchCenter, FString::Printf(TEXT("%d"), NumCloseEntities), nullptr, FColor::Green, 1.f);
		});
	}

	TArray<FCapsule> OutCloseUnhittableEntities;
	OutCloseUnhittableEntities.Reserve(300);

	GetCloseUnhittableEntities(CloseEntities, OutCloseUnhittableEntities, IsEntityOnTeam1, Entity, EntitySubsystem, TargetEntityFragment, EntityLocation, bIsEntitySoldier);

	for (const FMassTargetGridItem& OtherEntity : CloseEntities)
	{
		// Skip self.
		if (OtherEntity.Entity == Entity)
		{
			continue;
		}

		// Skip invalid entities.
		if (!EntitySubsystem.IsEntityValid(OtherEntity.Entity))
		{
			continue;
		}

		// Skip same team.
		if (IsEntityOnTeam1 == OtherEntity.bIsOnTeam1) {
			continue;
		}

		if (!CanEntityDamageTargetEntity(TargetEntityFragment, OtherEntity.MinCaliberForDamage))
		{
			continue;
		}

		if (IsTargetEntityOutOfRange(EntityLocation, bIsEntitySoldier, OtherEntity.Location))
		{
			continue;
		}

		bOutIsTargetEntitySoldier = OtherEntity.bIsSoldier;
		const FVector& OtherEntityLocation = OtherEntity.Location;
		const FCapsule& ProjectileTraceCapsule = GetProjectileTraceCapsuleToTarget(bIsEntitySoldier, bOutIsTargetEntitySoldier, EntityTransform, OtherEntityLocation);

		if (AreCloseUnhittableEntitiesBlockingTarget(ProjectileTraceCapsule, OutCloseUnhittableEntities, Entity, *World))
		{
			continue;
		}

		if (!IsTargetEntityVisibleViaSphereTrace(*World, ProjectileTraceCapsule.a, ProjectileTraceCapsule.b, UE::Mass::Debug::IsDebuggingEntity(Entity)))
		{
			continue;
		}

		OutTargetEntity = OtherEntity.Entity;
		OutTargetEntityLocation = OtherEntityLocation;
		return true;
	}

	OutTargetEntity = UMassEntitySubsystem::InvalidEntity;
	return false;
}

float GetProjectileInitialXYVelocityMagnitude(const bool bIsEntitySoldier)
{
	return bIsEntitySoldier ? 6000.f : 10000.f; // TODO: make this configurable in data asset and get from there?
}

float GetVerticalAimOffset(FMassExecutionContext& Context, const bool bIsTargetEntitySoldier, const FTransform& EntityTransform, const FVector& TargetEntityLocation, UMassEntitySubsystem& EntitySubsystem)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const bool bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
	const bool bShouldAimAtFeet = !bIsEntitySoldier && bIsTargetEntitySoldier;
	const FVector ProjectileSpawnLocation = EntityLocation + UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationOffset(EntityTransform, bIsEntitySoldier);
	const FVector ProjectileTargetLocation = bShouldAimAtFeet ? TargetEntityLocation : FVector(TargetEntityLocation.X, TargetEntityLocation.Y, ProjectileSpawnLocation.Z);
	const float XYDistanceToTarget = (FVector2D(ProjectileTargetLocation) - FVector2D(ProjectileSpawnLocation)).Size();
	const float ProjectileInitialXYVelocityMagnitude = GetProjectileInitialXYVelocityMagnitude(bIsEntitySoldier);
	const float TimeToTarget = XYDistanceToTarget / ProjectileInitialXYVelocityMagnitude;
	const float VerticalDistanceToTravel = ProjectileTargetLocation.Z - UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsEntitySoldier);
	const float& GravityZ = EntitySubsystem.GetWorld()->GetGravityZ();
	const float VerticalDistanceTraveledDueToGravity = (1.f / 2.f) * GravityZ * TimeToTarget * TimeToTarget;
	const float Result = (VerticalDistanceToTravel - VerticalDistanceTraveledDueToGravity) / TimeToTarget;

	return Result;
}

bool ProcessEntityForVisualTarget(TQueue<FMassEntityHandle>& TargetFinderEntityQueue, FMassEntityHandle Entity, UMassEntitySubsystem& EntitySubsystem, const FTransformFragment& TranformFragment, FTargetEntityFragment& TargetEntityFragment, const bool IsEntityOnTeam1, FMassExecutionContext& Context, const FTargetHashGrid2D& TargetGrid)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_ProcessEntityForVisualTarget);

	const FTransform& EntityTransform = TranformFragment.GetTransform();
	FMassEntityHandle TargetEntity;
	FVector OutTargetEntityLocation;
	bool bOutIsTargetEntitySoldier;
	auto bFoundTarget = GetClosestValidEnemy(Entity, EntitySubsystem, TargetGrid, EntityTransform, TargetEntity, IsEntityOnTeam1, TargetEntityFragment, Context, OutTargetEntityLocation, bOutIsTargetEntitySoldier);
	if (!bFoundTarget) {
		return false;
	}

	TargetEntityFragment.Entity = TargetEntity;

	TargetEntityFragment.VerticalAimOffset = GetVerticalAimOffset(Context, bOutIsTargetEntitySoldier, EntityTransform, OutTargetEntityLocation, EntitySubsystem);
	TargetFinderEntityQueue.Enqueue(Entity);

	return true;
}

bool UMassEnemyTargetFinderProcessor_DrawTrackedSounds = false;
FAutoConsoleVariableRef CVarUMassEnemyTargetFinderProcessor_DrawTrackedSounds(TEXT("pm.UMassEnemyTargetFinderProcessor_DrawTrackedSounds"), UMassEnemyTargetFinderProcessor_DrawTrackedSounds, TEXT("UMassEnemyTargetFinderProcessor: Draw Tracked Sounds"));

void ProcessEntityForAudioTarget(UMassSoundPerceptionSubsystem* SoundPerceptionSubsystem, const FTransform& EntityTransform, FMassMoveTargetFragment& MoveTargetFragment, const bool& bIsEntityOnTeam1, FMassStashedMoveTargetFragment& StashedMoveTargetFragment, const UMassEntitySubsystem& EntitySubsystem, const FMassEntityHandle& Entity, TQueue<FMassEntityHandle>& TrackingSoundWhileNavigatingQueue, FMassMoveForwardCompleteSignalFragment& MoveForwardCompleteSignalFragment)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_ProcessEntityForAudioTarget);

	UWorld* World = SoundPerceptionSubsystem->GetWorld();
	const FVector& EntityLocation = EntityTransform.GetLocation();
	FVector OutSoundSource;
	const bool& bIsFacingMoveTarget = IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward);
	if (SoundPerceptionSubsystem->HasSoundAtLocation(EntityLocation, OutSoundSource, !bIsEntityOnTeam1) && bIsFacingMoveTarget)
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

		if (UMassEnemyTargetFinderProcessor_DrawTrackedSounds || UE::Mass::Debug::IsDebuggingEntity(Entity))
		{
			AsyncTask(ENamedThreads::GameThread, [World, EntityLocation, OutSoundSource]()
			{
				FVector VerticalOffset(0.f, 0.f, 400.f);
				DrawDebugDirectionalArrow(World, EntityLocation + VerticalOffset, OutSoundSource + VerticalOffset, 100.f, FColor::Green, false, 5.f);
			});
		}
	}
}

bool UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk = true;
FAutoConsoleVariableRef CVarUMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk(TEXT("pm.UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk"), UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk, TEXT("Use ParallelForEachEntityChunk in UMassEnemyTargetFinderProcessor::Execute to improve performance"));

bool UMassEnemyTargetFinderProcessor_SkipFindingTargets = false;
FAutoConsoleVariableRef CVarUMassEnemyTargetFinderProcessor_SkipFindingTargets(TEXT("pm.UMassEnemyTargetFinderProcessor_SkipFindingTargets"), UMassEnemyTargetFinderProcessor_SkipFindingTargets, TEXT("UMassEnemyTargetFinderProcessor: Skip Finding Targets"));

void DrawEntitySearchingIfNeeded(UWorld* World, const FVector& Location, const FMassEntityHandle& Entity)
{
	if (UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [World, Location]()
		{
			::DrawDebugSphere(World, Location + FVector(0.f, 0.f, 300.f), 200.f, 10, FColor::Yellow, false, 0.1f);
		});
	}
}

void UMassEnemyTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor);

	if (UMassEnemyTargetFinderProcessor_SkipFindingTargets)
	{
		return;
	}

	TQueue<FMassEntityHandle> TargetFinderEntityQueue;
	TQueue<FMassEntityHandle> TrackingSoundWhileNavigatingQueue;

	auto ExecuteFunction = [&EntitySubsystem, &TargetFinderEntityQueue, &SoundPerceptionSubsystem = SoundPerceptionSubsystem, &TrackingSoundWhileNavigatingQueue, &TargetFinderSubsystem = TargetFinderSubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetMutableFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassMoveForwardCompleteSignalFragment> MoveForwardCompleteSignalList = Context.GetMutableFragmentView<FMassMoveForwardCompleteSignalFragment>();

		const FTargetHashGrid2D& TargetGrid = TargetFinderSubsystem->GetTargetGrid();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
			const bool& bFoundVisualTarget = ProcessEntityForVisualTarget(TargetFinderEntityQueue, Entity, EntitySubsystem, LocationList[EntityIndex], TargetEntityList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, Context, TargetGrid);
			if (!bFoundVisualTarget)
			{
				ProcessEntityForAudioTarget(SoundPerceptionSubsystem, LocationList[EntityIndex].GetTransform(), MoveTargetList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, StashedMoveTargetList[EntityIndex], EntitySubsystem, Entity, TrackingSoundWhileNavigatingQueue, MoveForwardCompleteSignalList[EntityIndex]);
			}
			DrawEntitySearchingIfNeeded(EntitySubsystem.GetWorld(), LocationList[EntityIndex].GetTransform().GetLocation(), Context.GetEntity(EntityIndex));
		}
	};

	if (UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk)
	{
		EntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	} else {
		EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_ProcessQueues);
		while (!TargetFinderEntityQueue.IsEmpty())
		{
			FMassEntityHandle TargetFinderEntity;
			bool bSuccess = TargetFinderEntityQueue.Dequeue(TargetFinderEntity);
			check(bSuccess);

			Context.Defer().AddTag<FMassWillNeedEnemyTargetTag>(TargetFinderEntity);
			Context.Defer().RemoveTag<FMassNeedsEnemyTargetTag>(TargetFinderEntity);
		}

		while (!TrackingSoundWhileNavigatingQueue.IsEmpty())
		{
			FMassEntityHandle Entity;
			bool bSuccess = TrackingSoundWhileNavigatingQueue.Dequeue(Entity);
			check(bSuccess);

			Context.Defer().AddTag<FMassHasStashedMoveTargetTag>(Entity);
			Context.Defer().AddTag<FMassTrackSoundTag>(Entity);
			Context.Defer().AddTag<FMassNeedsMoveTargetForwardCompleteSignalTag>(Entity);
		}
	}
}

/*static*/ const float UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(const bool& bIsSoldier)
{
	return bIsSoldier ? 150.f : 180.f; // TODO: don't hard-code
}

/*static*/ const FVector UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationOffset(const FTransform& EntityTransform, const bool& bIsSoldier)
{
	const float ForwardVectorMagnitude = bIsSoldier ? 300.f : 800.f; // TODO: don't hard-code
	const FVector& ProjectileZOffset = FVector(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsSoldier));
	return EntityTransform.GetRotation().GetForwardVector() * ForwardVectorMagnitude + ProjectileZOffset;
}
