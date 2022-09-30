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
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "InvalidTargetFinderProcessor.h"
#include "MassRepresentationTypes.h"
#include "MassTargetGridProcessors.h"

struct FPotentialTargetSphereTraceData
{
  FPotentialTargetSphereTraceData(FMassEntityHandle InEntity, FMassEntityHandle InTargetEntity, FVector InTraceStart, FVector InTraceEnd, float InMinCaliberForDamage, FVector InLocation, bool bInIsSoldier)
    : Entity(InEntity), TargetEntity(InTargetEntity), TraceStart(InTraceStart), TraceEnd(InTraceEnd), MinCaliberForDamage(InMinCaliberForDamage), Location(InLocation), bIsSoldier(bInIsSoldier)
  {
  }

	FPotentialTargetSphereTraceData() = default;

  FMassEntityHandle Entity;
  FMassEntityHandle TargetEntity;
	FVector TraceStart;
	FVector TraceEnd;
  float MinCaliberForDamage;
  FVector Location;
	bool bIsSoldier;
};

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
	FMassEntityQuery BaseEntityQuery;

  BaseEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BaseEntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	BaseEntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);

	PreSphereTraceEntityQuery = BaseEntityQuery;
	PreSphereTraceEntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);

  PostSphereTraceEntityQuery = BaseEntityQuery;
}

void UMassEnemyTargetFinderProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	TargetFinderSubsystem = UWorld::GetSubsystem<UMassTargetFinderSubsystem>(Owner.GetWorld());
}

bool CanEntityDamageTargetEntity(const FTargetEntityFragment& TargetEntityFragment, const float& MinCaliberForDamage)
{
	return TargetEntityFragment.TargetMinCaliberForDamage >= MinCaliberForDamage;
}

bool CanEntityDamageTargetEntity(const FTargetEntityFragment& TargetEntityFragment, const FMassEntityView& OtherEntityView)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_CanEntityDamageTargetEntity");

	const FProjectileDamagableFragment* TargetEntityProjectileDamagableFragment = OtherEntityView.GetFragmentDataPtr<FProjectileDamagableFragment>();
	return TargetEntityProjectileDamagableFragment && CanEntityDamageTargetEntity(TargetEntityFragment, TargetEntityProjectileDamagableFragment->MinCaliberForDamage);
}

FCapsule GetProjectileTraceCapsuleToTarget(const bool bIsEntitySoldier, const bool bIsTargetEntitySoldier, const FTransform& EntityTransform, const FVector& TargetEntityLocation)
{
	const FVector& EntityLocation = EntityTransform.GetLocation();
	const FVector ProjectileZOffset(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsEntitySoldier));
	const FVector ProjectileSpawnLocation = EntityLocation + UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationOffset(EntityTransform, bIsEntitySoldier);

	const bool bShouldAimAtFeet = !bIsEntitySoldier && bIsTargetEntitySoldier;
	static constexpr float VerticalBuffer = 50.f; // This is needed because otherwise for tanks we always hit the ground when doing trace. TODO: Come up with a better way to handle this.
  const FVector ProjectileTargetLocation = bShouldAimAtFeet ? TargetEntityLocation + FVector(0.f, 0.f, ProjectileRadius + VerticalBuffer) : TargetEntityLocation + ProjectileZOffset;
	return FCapsule(ProjectileSpawnLocation, ProjectileTargetLocation, ProjectileRadius);
}

bool AreCloseUnhittableEntitiesBlockingTarget(const FCapsule& ProjectileTraceCapsule, const TArray<FCapsule>& CloseUnhittableEntities, const FMassEntityHandle& Entity, const UWorld& World)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_AreCloseUnhittableEntitiesBlockingTarget");

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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_IsTargetEntityVisibleViaSphereTrace");
	FHitResult Result;
	static constexpr float Radius = 20.f; // TODO: don't hard-code
  const bool bFoundBlockingHit = UKismetSystemLibrary::SphereTraceSingle(World.GetLevel(0)->Actors[0], StartLocation, EndLocation, Radius, TraceTypeQuery1, false, TArray<AActor*>(), DrawTrace ? EDrawDebugTrace::Type::ForDuration : EDrawDebugTrace::Type::None, Result, false, FLinearColor::Red, FLinearColor::Green, 2.f);
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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_IsTargetEntityOutOfRange");

	const FTransformFragment& TargetEntityTransformFragment = TargetEntityView.GetFragmentData<FTransformFragment>();
	const FVector& TargetEntityLocation = TargetEntityTransformFragment.GetTransform().GetLocation();
	return IsTargetEntityOutOfRange(EntityLocation, bIsEntitySoldier, TargetEntityLocation);
}

void GetCloseUnhittableEntities(const TArray<FMassTargetGridItem> &CloseEntities, TArray<FCapsule>& OutCloseUnhittableEntities, const bool& IsEntityOnTeam1, const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment, const FVector& EntityLocation, const bool bIsEntitySoldier)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_GetCloseUnhittableEntities");

	auto AddEntityToCloseUnhittablesIfNeeded = [&Entity, &EntitySubsystem, &EntityLocation, &bIsEntitySoldier, &IsEntityOnTeam1, &OutCloseUnhittableEntities, &TargetEntityFragment](const FMassTargetGridItem& OtherEntity) {
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_AddEntityToCloseUnhittablesIfNeeded");

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

struct FPotentialTarget
{
	FPotentialTarget(FMassEntityHandle InEntity, FVector InLocation, float InMinCaliberForDamage, bool bIsSoldier)
    : Entity(InEntity), Location(InLocation), MinCaliberForDamage(InMinCaliberForDamage), bIsSoldier(bIsSoldier)
  {
  }

	FMassEntityHandle Entity;
	FVector Location;
	float MinCaliberForDamage;
	bool bIsSoldier;
};

void GetPotentialTargetSphereTraces(const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, const FTargetHashGrid2D& TargetGrid, const FTransform& EntityTransform, FMassEntityHandle& OutTargetEntity, const bool& IsEntityOnTeam1, FTargetEntityFragment& TargetEntityFragment, FMassExecutionContext& Context, FVector& OutTargetEntityLocation, bool& bOutIsTargetEntitySoldier, const bool bIsEntitySoldier, TQueue<FPotentialTargetSphereTraceData>& OutPotentialTargetsNeedingSphereTrace)
{
	const UWorld* World = EntitySubsystem.GetWorld();

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
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_GetPotentialTargets_TargetGridQuery");
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

		OutPotentialTargetsNeedingSphereTrace.Enqueue(FPotentialTargetSphereTraceData(Entity, OtherEntity.Entity, ProjectileTraceCapsule.a, ProjectileTraceCapsule.b, OtherEntity.MinCaliberForDamage, OtherEntityLocation, OtherEntity.bIsSoldier));
	}
}

float GetProjectileInitialXYVelocityMagnitude(const bool bIsEntitySoldier)
{
	return bIsEntitySoldier ? 6000.f : 10000.f; // TODO: make this configurable in data asset and get from there?
}

void ProcessEntityForVisualTarget(TQueue<FMassEntityHandle>& TargetFinderEntityQueue, FMassEntityHandle Entity, UMassEntitySubsystem& EntitySubsystem, const FTransformFragment& TranformFragment, FTargetEntityFragment& TargetEntityFragment, const bool IsEntityOnTeam1, FMassExecutionContext& Context, const FTargetHashGrid2D& TargetGrid, const bool bIsEntitySoldier, TQueue<FPotentialTargetSphereTraceData>& PotentialTargetsNeedingSphereTrace)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor_ProcessEntityForVisualTarget");

	const FTransform& EntityTransform = TranformFragment.GetTransform();
	FMassEntityHandle TargetEntity;
	FVector OutTargetEntityLocation;
	bool bOutIsTargetEntitySoldier;
	GetPotentialTargetSphereTraces(Entity, EntitySubsystem, TargetGrid, EntityTransform, TargetEntity, IsEntityOnTeam1, TargetEntityFragment, Context, OutTargetEntityLocation, bOutIsTargetEntitySoldier, bIsEntitySoldier, PotentialTargetsNeedingSphereTrace);
}

bool UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk = true;
FAutoConsoleVariableRef CVarUMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk(TEXT("pm.UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk"), UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk, TEXT("Use ParallelForEachEntityChunk in UMassEnemyTargetFinderProcessor to improve performance"));

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

struct FProcessSphereTracesContext
{
  FProcessSphereTracesContext(TQueue<FPotentialTargetSphereTraceData>& PotentialTargetsNeedingSphereTraceQueue, UWorld& World, TMap<FMassEntityHandle, TArray<FPotentialTarget>>& OutEntityToPotentialTargetEntities)
    : PotentialTargetsNeedingSphereTraceQueue(PotentialTargetsNeedingSphereTraceQueue), World(World), EntityToPotentialTargetEntities(OutEntityToPotentialTargetEntities)
  {
  }

	void Execute()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FProcessSphereTracesContext.Execute");

		ConvertSphereTraceQueueToArray();
		ProcessSphereTraces();
		ConvertPotentialVisibleTargetsQueueToMap();
	}

private:

	void ConvertSphereTraceQueueToArray()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FProcessSphereTracesContext.ConvertSphereTraceQueueToArray");

		while (!PotentialTargetsNeedingSphereTraceQueue.IsEmpty())
		{
			FPotentialTargetSphereTraceData PotentialTarget;
      const bool bSuccess = PotentialTargetsNeedingSphereTraceQueue.Dequeue(PotentialTarget);
			check(bSuccess);
			PotentialTargetsNeedingSphereTrace.Add(PotentialTarget);
		}
	}

	void ProcessSphereTraces()
  {
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FProcessSphereTracesContext.ProcessSphereTraces");

	  ParallelFor(PotentialTargetsNeedingSphereTrace.Num(), [&](const int32 JobIndex)
		{
			if (IsTargetEntityVisibleViaSphereTrace(World, PotentialTargetsNeedingSphereTrace[JobIndex].TraceStart, PotentialTargetsNeedingSphereTrace[JobIndex].TraceEnd, false))
			{
				PotentialVisibleTargets.Enqueue(PotentialTargetsNeedingSphereTrace[JobIndex]);
			}
		});
  }

	void ConvertPotentialVisibleTargetsQueueToMap()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FProcessSphereTracesContext.ConvertPotentialVisibleTargetsQueueToMap");

		while (!PotentialVisibleTargets.IsEmpty())
		{
			FPotentialTargetSphereTraceData PotentialTarget;
			const bool bSuccess = PotentialVisibleTargets.Dequeue(PotentialTarget);
			check(bSuccess);
			if (!EntityToPotentialTargetEntities.Contains(PotentialTarget.Entity))
			{
				EntityToPotentialTargetEntities.Add(PotentialTarget.Entity, TArray<FPotentialTarget>());
			}
			EntityToPotentialTargetEntities[PotentialTarget.Entity].Add(FPotentialTarget(PotentialTarget.TargetEntity, PotentialTarget.Location, PotentialTarget.MinCaliberForDamage, PotentialTarget.bIsSoldier));
		}
	}

	TQueue<FPotentialTargetSphereTraceData>& PotentialTargetsNeedingSphereTraceQueue;
	UWorld& World;
	TMap<FMassEntityHandle, TArray<FPotentialTarget>>& EntityToPotentialTargetEntities;
	TArray<FPotentialTargetSphereTraceData> PotentialTargetsNeedingSphereTrace;
	TQueue<FPotentialTargetSphereTraceData> PotentialVisibleTargets;
};

struct FSelectBestTargetProcessEntityContext
{
	FSelectBestTargetProcessEntityContext(UMassEntitySubsystem& EntitySubsystem, TQueue<FMassEntityHandle>& TargetFinderEntityQueue, const FMassEntityHandle& Entity, const FTransform& EntityTransform, FTargetEntityFragment& TargetEntityFragment, TArray<FPotentialTarget>& PotentialTargets, const bool bIsEntitySoldier)
		: EntitySubsystem(EntitySubsystem), TargetFinderEntityQueue(TargetFinderEntityQueue), Entity(Entity), EntityLocation(EntityTransform.GetLocation()), EntityTransform(EntityTransform), bIsEntitySoldier(bIsEntitySoldier), TargetEntityFragment(TargetEntityFragment), PotentialTargets(PotentialTargets)
	{
	}

	void ProcessEntity() const
  {
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSelectBestTargetProcessEntityContext.ProcessEntity");

		TSortedMap<float, TArray<FPotentialTarget>> PotentialTargetsByCaliber;
		GroupByCaliber(PotentialTargetsByCaliber);

		FMassEntityHandle TargetEntity;
		FVector TargetEntityLocation;
		bool bIsTargetEntitySoldier;
		if (SelectBestTarget(PotentialTargetsByCaliber, TargetEntity, TargetEntityLocation, bIsTargetEntitySoldier))
		{
			TargetEntityFragment.Entity = TargetEntity;
			TargetEntityFragment.VerticalAimOffset = GetVerticalAimOffset(TargetEntityLocation, bIsTargetEntitySoldier);
			TargetFinderEntityQueue.Enqueue(Entity);
		}
	}

	void GroupByCaliber(TSortedMap<float, TArray<FPotentialTarget>>& OutPotentialTargetsByCaliber) const
  {
		for (FPotentialTarget& PotentialTarget : PotentialTargets)
		{
			if (!OutPotentialTargetsByCaliber.Contains(PotentialTarget.MinCaliberForDamage))
			{
				OutPotentialTargetsByCaliber.Add(PotentialTarget.MinCaliberForDamage, TArray<FPotentialTarget>());
			}
			OutPotentialTargetsByCaliber[PotentialTarget.MinCaliberForDamage].Add(PotentialTarget);
		}
	}

	bool SelectBestTarget(TSortedMap<float, TArray<FPotentialTarget>>& PotentialTargetsByCaliber, FMassEntityHandle& OutTargetEntity, FVector& OutTargetEntityLocation, bool& bIsTargetEntitySoldier) const
  {
		if (PotentialTargetsByCaliber.Num() == 0)
		{
			OutTargetEntity = UMassEntitySubsystem::InvalidEntity;
			return false;
		}

		TArray<float> OutKeys;
		PotentialTargetsByCaliber.GetKeys(OutKeys);
		TArray<FPotentialTarget> PotentialTargetsWithBestCaliber = PotentialTargetsByCaliber[OutKeys.Last()];

		auto DistanceSqFromEntityToLocation = [&EntityLocation = EntityLocation](const FVector& OtherLocation) {
			return (OtherLocation - EntityLocation).SizeSquared();
		};

		PotentialTargetsWithBestCaliber.Sort([&DistanceSqFromEntityToLocation](const FPotentialTarget& A, const FPotentialTarget& B) { return DistanceSqFromEntityToLocation(A.Location) < DistanceSqFromEntityToLocation(B.Location); });

		OutTargetEntity = PotentialTargetsWithBestCaliber[0].Entity;
		OutTargetEntityLocation = PotentialTargetsWithBestCaliber[0].Location;
		bIsTargetEntitySoldier = PotentialTargetsWithBestCaliber[0].bIsSoldier;

		return true;
	}

	float GetVerticalAimOffset(const FVector& TargetEntityLocation, const bool bIsTargetEntitySoldier) const
  {
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

private:
	UMassEntitySubsystem& EntitySubsystem;
	TQueue<FMassEntityHandle>& TargetFinderEntityQueue;
	const FMassEntityHandle& Entity;
	const FVector EntityLocation;
	const FTransform& EntityTransform;
	const bool bIsEntitySoldier;
	FTargetEntityFragment& TargetEntityFragment;
	TArray<FPotentialTarget>& PotentialTargets;
};

struct FSelectBestTargetContext
{
	FSelectBestTargetContext(FMassEntityQuery& EntityQuery, UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context, TMap<FMassEntityHandle, TArray<FPotentialTarget>>& EntityToPotentialTargetEntities, TQueue<FMassEntityHandle>& TargetFinderEntityQueue)
    : EntityQuery(EntityQuery), EntitySubsystem(EntitySubsystem), Context(Context), EntityToPotentialTargetEntities(EntityToPotentialTargetEntities), TargetFinderEntityQueue(TargetFinderEntityQueue)
  {
  }

	void Execute() const
  {
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSelectBestTargetContext.Execute");

	  EntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, [this](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();

			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
				if (EntityToPotentialTargetEntities.Contains(Entity))
				{
					const bool bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
					FSelectBestTargetProcessEntityContext(EntitySubsystem, TargetFinderEntityQueue, Entity, LocationList[EntityIndex].GetTransform(), TargetEntityList[EntityIndex], EntityToPotentialTargetEntities[Entity], bIsEntitySoldier).ProcessEntity();
				}
			}
		});
	}

private:
  FMassEntityQuery& EntityQuery;
	UMassEntitySubsystem& EntitySubsystem;
	FMassExecutionContext& Context;
	TMap<FMassEntityHandle, TArray<FPotentialTarget>>& EntityToPotentialTargetEntities;
	TQueue<FMassEntityHandle>& TargetFinderEntityQueue;
};

void UMassEnemyTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor");

	if (UMassEnemyTargetFinderProcessor_SkipFindingTargets)
	{
		return;
	}

	TQueue<FMassEntityHandle> TargetFinderEntityQueue;
	TQueue<FPotentialTargetSphereTraceData> PotentialTargetsNeedingSphereTrace;

	auto ExecuteFunction = [&EntitySubsystem, &TargetFinderEntityQueue, &TargetFinderSubsystem = TargetFinderSubsystem, &PotentialTargetsNeedingSphereTrace](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();

		const FTargetHashGrid2D& TargetGrid = TargetFinderSubsystem->GetTargetGrid();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FMassEntityHandle& Entity = Context.GetEntity(EntityIndex);
			const bool& bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
			ProcessEntityForVisualTarget(TargetFinderEntityQueue, Entity, EntitySubsystem, LocationList[EntityIndex], TargetEntityList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, Context, TargetGrid, bIsEntitySoldier, PotentialTargetsNeedingSphereTrace);
			DrawEntitySearchingIfNeeded(EntitySubsystem.GetWorld(), LocationList[EntityIndex].GetTransform().GetLocation(), Context.GetEntity(EntityIndex));
		}
	};

	if (UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor.ParallelForEachEntityChunk");

	  PreSphereTraceEntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	} else {
		PreSphereTraceEntityQuery.ForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}

	if (PotentialTargetsNeedingSphereTrace.IsEmpty())
	{
		return;
	}

	TMap<FMassEntityHandle, TArray<FPotentialTarget>> EntityToPotentialTargetEntities;
	FProcessSphereTracesContext(PotentialTargetsNeedingSphereTrace, *EntitySubsystem.GetWorld(), EntityToPotentialTargetEntities).Execute();
	FSelectBestTargetContext(PostSphereTraceEntityQuery, EntitySubsystem, Context, EntityToPotentialTargetEntities, TargetFinderEntityQueue).Execute();

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassEnemyTargetFinderProcessor.ProcessQueues");
		while (!TargetFinderEntityQueue.IsEmpty())
		{
			FMassEntityHandle TargetFinderEntity;
      const bool bSuccess = TargetFinderEntityQueue.Dequeue(TargetFinderEntity);
			check(bSuccess);

			Context.Defer().AddTag<FMassWillNeedEnemyTargetTag>(TargetFinderEntity);
			Context.Defer().RemoveTag<FMassNeedsEnemyTargetTag>(TargetFinderEntity);
		}
	}
}

/*static*/ float UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(const bool& bIsSoldier)
{
	return bIsSoldier ? 150.f : 180.f; // TODO: don't hard-code
}

/*static*/ FVector UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationOffset(const FTransform& EntityTransform, const bool& bIsSoldier)
{
	const float ForwardVectorMagnitude = bIsSoldier ? 300.f : 800.f; // TODO: don't hard-code
	const FVector& ProjectileZOffset = FVector(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsSoldier));
	return EntityTransform.GetRotation().GetForwardVector() * ForwardVectorMagnitude + ProjectileZOffset;
}
