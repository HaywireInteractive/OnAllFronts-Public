// Fill out your copyright notice in the Description page of Project Settings.


#include "InvalidTargetFinderProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassMoveToCommandProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"
#include "MassNavigationSubsystem.h"
#include "MassEntityView.h"
#include "MassProjectileDamageProcessor.h"

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

UInvalidTargetFinderProcessor::UInvalidTargetFinderProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UInvalidTargetFinderProcessor::Initialize(UObject& Owner)
{
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

void UInvalidTargetFinderProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassStashedMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassWillNeedEnemyTargetTag>(EMassFragmentPresence::All);
}

bool IsTargetEntityOutOfRange(const FVector& TargetEntityLocation, const FVector &EntityLocation, const UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment, const FMassEntityHandle Entity)
{
	const double& DistanceBetweenEntities = (TargetEntityLocation - EntityLocation).Size();

	const uint8 SearchDepth = UMassEnemyTargetFinderProcessor_FinderPhaseCount / TargetEntityFragment.SearchBreadth;
	const float MaxRange = SearchDepth * UMassEnemyTargetFinderProcessor_CellSize;

	if (UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [World = EntitySubsystem.GetWorld(), EntityLocation, TargetEntityLocation]()
		{
			DrawDebugDirectionalArrow(World, EntityLocation, TargetEntityLocation, 10.f, FColor::Yellow, false, 0.1f);
		});
	}

	return DistanceBetweenEntities > MaxRange;
}

bool DidCapsulesCollide(const FCapsule& Capsule1, const FCapsule& Capsule2, const FMassEntityHandle& Entity, const UWorld& World)
{
	const bool& Result = TestCapsuleCapsule(Capsule1, Capsule2);

	if (UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [Capsule1, Capsule2, &World, Result]()
		{
			const FLinearColor& Color = Result ? FLinearColor::Red : FLinearColor::Green;
			DrawCapsule(Capsule1, World, Color, false, 0.1f);
			DrawCapsule(Capsule2, World, Color, false, 0.1f);
		});
	}

	return Result;
}

bool IsTargetEntityObstructed(const FVector& EntityLocation, const FVector& TargetEntityLocation, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FMassEntityHandle& Entity, const UMassEntitySubsystem& EntitySubsystem, const bool& IsEntityOnTeam1, const FMassExecutionContext& Context, FTargetEntityFragment& TargetEntityFragment)
{
	FVector Buffer(10.f, 10.f, 10.f); // We keep a buffer in case EntityLocation and TargetEntityLocation are same value on any axis.
	FBox QueryBounds(EntityLocation.ComponentMin(TargetEntityLocation) - Buffer, EntityLocation.ComponentMax(TargetEntityLocation) + Buffer);
	TArray<FMassNavigationObstacleItem> CloseEntities;
	AvoidanceObstacleGrid.Query(QueryBounds, CloseEntities);

	if (UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [QueryBounds, World = EntitySubsystem.GetWorld()]()
		{
			const FVector QueryCenter = (QueryBounds.Min + QueryBounds.Max) / 2.f;
			const FVector VerticalOffset(0.f, 0.f, 1000.f);
			DrawDebugBox(World, QueryCenter, QueryBounds.Max - QueryCenter + VerticalOffset, FColor::Blue, false, 0.1f);
		});
	}

	const bool& bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
	const FVector ProjectileOffset = FVector(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsEntitySoldier));
	const FVector ProjectileSpawnLocation = EntityLocation + ProjectileOffset;
	FVector ProjectileTargetLocation = TargetEntityLocation + ProjectileOffset;
	FCapsule ProjectileCapsule(ProjectileSpawnLocation, ProjectileTargetLocation, ProjectileRadius);

	for (const FMassNavigationObstacleItem& OtherEntity : CloseEntities)
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

		FMassEntityView OtherEntityView(EntitySubsystem, OtherEntity.Entity);
		FTeamMemberFragment* OtherEntityTeamMemberFragment = OtherEntityView.GetFragmentDataPtr<FTeamMemberFragment>();

		// Skip entities that don't have FTeamMemberFragment.
		if (!OtherEntityTeamMemberFragment) {
			continue;
		}

		// If same team or undamagable, check for collision.
		if (IsEntityOnTeam1 == OtherEntityTeamMemberFragment->IsOnTeam1 || !CanEntityDamageTargetEntity(TargetEntityFragment, OtherEntityView)) {
			FCapsule OtherEntityCapsule = MakeCapsuleForEntity(OtherEntityView);
			if (DidCapsulesCollide(ProjectileCapsule, OtherEntityCapsule, Entity, *EntitySubsystem.GetWorld()))
			{
				return true;
			}
		}
	}

	return false;
}

bool IsTargetValid(const FMassEntityHandle& Entity, FMassEntityHandle& TargetEntity, const UMassEntitySubsystem& EntitySubsystem, const FVector& EntityLocation, FTargetEntityFragment& TargetEntityFragment, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const bool& IsEntityOnTeam1, const FMassExecutionContext& Context)
{
	if (!EntitySubsystem.IsEntityValid(TargetEntity))
	{
		return false;
	}

	const FVector& TargetEntityLocation = EntitySubsystem.GetFragmentDataChecked<FTransformFragment>(TargetEntity).GetTransform().GetLocation();
	if (IsTargetEntityOutOfRange(TargetEntityLocation, EntityLocation, EntitySubsystem, TargetEntityFragment, Entity))
	{
		return false;
	}

	if (IsTargetEntityObstructed(EntityLocation, TargetEntityLocation, AvoidanceObstacleGrid, Entity, EntitySubsystem, IsEntityOnTeam1, Context, TargetEntityFragment))
	{
		return false;
	}

	return true;
}

void ProcessEntity(const FMassExecutionContext& Context, const FMassEntityHandle Entity, const UMassEntitySubsystem& EntitySubsystem, FTargetEntityFragment& TargetEntityFragment, const FVector &EntityLocation, const FMassStashedMoveTargetFragment* StashedMoveTargetFragment, FMassMoveTargetFragment* MoveTargetFragment, TArray<FMassEntityHandle>& TransientEntitiesToSignal, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const bool& IsEntityOnTeam1)
{
	FMassEntityHandle& TargetEntity = TargetEntityFragment.Entity;
	if (!IsTargetValid(Entity, TargetEntity, EntitySubsystem, EntityLocation, TargetEntityFragment, AvoidanceObstacleGrid, IsEntityOnTeam1, Context))
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

void UInvalidTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UInvalidTargetFinderProcessor);

	if (!NavigationSubsystem)
	{
		return;
	}

	TransientEntitiesToSignal.Reset();

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem, &TransientEntitiesToSignal = TransientEntitiesToSignal, &NavigationSubsystem = NavigationSubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();
		const TConstArrayView<FMassStashedMoveTargetFragment> StashedMoveTargetList = Context.GetFragmentView<FMassStashedMoveTargetFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();

		// TODO: We're incorrectly assuming all obstacles can be targets.
		const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGrid();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			ProcessEntity(Context, Context.GetEntity(EntityIndex), EntitySubsystem, TargetEntityList[EntityIndex], TransformList[EntityIndex].GetTransform().GetLocation(), StashedMoveTargetList.Num() > 0 ? &StashedMoveTargetList[EntityIndex] : nullptr, MoveTargetList.Num() > 0 ? &MoveTargetList[EntityIndex] : nullptr, TransientEntitiesToSignal, AvoidanceObstacleGrid, TeamMemberList[EntityIndex].IsOnTeam1);
		}
	});

	if (TransientEntitiesToSignal.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, TransientEntitiesToSignal);
	}
}
