#include "MassProjectileDamageProcessor.h"

#include "MassEntityView.h"
#include "MassLODTypes.h"
#include "MassCommonFragments.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MassPlayerSubsystem.h"
#include "Character/CommanderCharacter.h"
#include "MilitaryStructureSubsystem.h"
#include <MassVisualEffectsSubsystem.h>
#include "MassCollisionProcessor.h"
#include <MassEnemyTargetFinderProcessor.h>
#include <MassSoundPerceptionSubsystem.h>

static constexpr uint32 GUMassProjectileWithDamageTrait_MaxClosestEntitiesToFind = 20;
typedef TArray<FMassNavigationObstacleItem, TFixedAllocator<GUMassProjectileWithDamageTrait_MaxClosestEntitiesToFind>> TProjectileDamageObstacleItemArray;

//----------------------------------------------------------------------//
//	UMassProjectileWithDamageTrait
//----------------------------------------------------------------------//
void UMassProjectileWithDamageTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FMassPreviousLocationFragment>();

	FProjectileDamageFragment& ProjectileDamageFragment = BuildContext.AddFragment_GetRef<FProjectileDamageFragment>();
	ProjectileDamageFragment.DamagePerHit = DamagePerHit;
	ProjectileDamageFragment.Caliber = Caliber;
	ProjectileDamageFragment.SplashDamageRadius = SplashDamageRadius;

	if (ExplosionEntityConfig)
	{
		UMassVisualEffectsSubsystem* MassVisualEffectsSubsystem = UWorld::GetSubsystem<UMassVisualEffectsSubsystem>(&World);
		ProjectileDamageFragment.ExplosionEntityConfigIndex = MassVisualEffectsSubsystem->FindOrAddEntityConfig(ExplosionEntityConfig);
	}

	BuildContext.AddTag<FMassProjectileWithDamageTag>();

	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	FMassForceFragment& ForceTemplate = BuildContext.AddFragment_GetRef<FMassForceFragment>();
	ForceTemplate.Value = FVector(0.f, 0.f, GravityMagnitude);

	// Needed because of UMassApplyMovementProcessor::ConfigureQueries requirements even though it's not used
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntitySubsystem);
	const FConstSharedStruct MovementFragment = EntitySubsystem->GetOrCreateConstSharedFragment(UE::StructUtils::GetStructCrc32(FConstStructView::Make(Movement)), Movement);
	BuildContext.AddConstSharedFragment(MovementFragment);

	const FConstSharedStruct MinZFragment = EntitySubsystem->GetOrCreateConstSharedFragment(UE::StructUtils::GetStructCrc32(FConstStructView::Make(MinZ)), MinZ);
	BuildContext.AddConstSharedFragment(MinZFragment);

	const FConstSharedStruct DebugParametersFragment = EntitySubsystem->GetOrCreateConstSharedFragment(UE::StructUtils::GetStructCrc32(FConstStructView::Make(DebugParameters)), DebugParameters);
	BuildContext.AddConstSharedFragment(DebugParametersFragment);
}

//----------------------------------------------------------------------//
//	UMassProjectileDamagableTrait
//----------------------------------------------------------------------//
void UMassProjectileDamagableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	if (bIsSoldier)
	{
		BuildContext.AddTag<FMassProjectileDamagableSoldierTag>();
	}

	FProjectileDamagableFragment& ProjectileDamagableTemplate = BuildContext.AddFragment_GetRef<FProjectileDamagableFragment>();
	ProjectileDamagableTemplate.MinCaliberForDamage = MinCaliberForDamage;
}

//----------------------------------------------------------------------//
//	UMassProjectileDamageProcessor
//----------------------------------------------------------------------//
UMassProjectileDamageProcessor::UMassProjectileDamageProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void UMassProjectileDamageProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FProjectileDamageFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassPreviousLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassProjectileWithDamageTag>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FDebugParameters>(EMassFragmentPresence::All);
}

void UMassProjectileDamageProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

static void FindCloseObstacles(const FVector& Center, const float SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
	TProjectileDamageObstacleItemArray& OutCloseEntities)
{
	OutCloseEntities.Reset();
	const FVector Extent(SearchRadius, SearchRadius, 0.f);
	const FBox QueryBox = FBox(Center - Extent, Center + Extent);

	struct FSortingCell
	{
		int32 X;
		int32 Y;
		int32 Level;
		float SqDist;
	};
	TArray<FSortingCell, TInlineAllocator<64>> Cells;
	const FVector QueryCenter = QueryBox.GetCenter();

	for (int32 Level = 0; Level < AvoidanceObstacleGrid.NumLevels; Level++)
	{
		const float CellSize = AvoidanceObstacleGrid.GetCellSize(Level);
		const FNavigationObstacleHashGrid2D::FCellRect Rect = AvoidanceObstacleGrid.CalcQueryBounds(QueryBox, Level);
		for (int32 Y = Rect.MinY; Y <= Rect.MaxY; Y++)
		{
			for (int32 X = Rect.MinX; X <= Rect.MaxX; X++)
			{
				const float CenterX = (X + 0.5f) * CellSize;
				const float CenterY = (Y + 0.5f) * CellSize;
				const float DX = CenterX - QueryCenter.X;
				const float DY = CenterY - QueryCenter.Y;
				const float SqDist = DX * DX + DY * DY;
				FSortingCell SortCell;
				SortCell.X = X;
				SortCell.Y = Y;
				SortCell.Level = Level;
				SortCell.SqDist = SqDist;
				Cells.Add(SortCell);
			}
		}
	}

	Cells.Sort([](const FSortingCell& A, const FSortingCell& B) { return A.SqDist < B.SqDist; });

	for (const FSortingCell& SortedCell : Cells)
	{
		if (const FNavigationObstacleHashGrid2D::FCell* Cell = AvoidanceObstacleGrid.FindCell(SortedCell.X, SortedCell.Y, SortedCell.Level))
		{
			const TSparseArray<FNavigationObstacleHashGrid2D::FItem>& Items = AvoidanceObstacleGrid.GetItems();
			for (int32 Idx = Cell->First; Idx != INDEX_NONE; Idx = Items[Idx].Next)
			{
				OutCloseEntities.Add(Items[Idx].ID);
				if (OutCloseEntities.Num() >= GUMassProjectileWithDamageTrait_MaxClosestEntitiesToFind)
				{
					return;
				}
			}
		}
	}
}

// Returns true if found at least one close entity that is not Entity. Note that OutCloseEntities may include Entity if it is a navigation obstacle.
bool GetClosestEntities(const FMassEntityHandle& Entity, const UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FVector& Location, const float Radius, TProjectileDamageObstacleItemArray& OutCloseEntities)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassProjectileDamageProcessor.GetClosestEntity);

	// TODO: Assumes that all avoidance obstacles are entities that could be damaged. Change instead to the same hash grid used for enemy target finder?
	FindCloseObstacles(Location, Radius, AvoidanceObstacleGrid, OutCloseEntities);

	for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : OutCloseEntities)
	{
		// Skip self
		if (OtherEntity.Entity == Entity)
		{
			continue;
		}

		// Skip invalid entities.
		if (!EntitySubsystem.IsEntityValid(OtherEntity.Entity))
		{
			continue;
		}
		return true;
	}

	return false;
}

bool DidCollideViaLineTrace(const UWorld &World, const FVector& StartLocation, const FVector &EndLocation, const bool& DrawLineTraces)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassProjectileDamageProcessor.DidCollideViaLineTrace);

	FHitResult HitResult;
	bool const bSuccess = World.LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECollisionChannel::ECC_Visibility);
	if (DrawLineTraces)
	{
		AsyncTask(ENamedThreads::GameThread, [HitResult, &World]()
		{
			static const FLinearColor TraceColor = FLinearColor::Red;
			static const FLinearColor TraceHitColor = FLinearColor::Green;

			if (HitResult.bBlockingHit)
			{
				// Red up to the blocking hit, green thereafter
				::DrawDebugLine(&World, HitResult.TraceStart, HitResult.ImpactPoint, TraceColor.ToFColor(true), true);
				::DrawDebugLine(&World, HitResult.ImpactPoint, HitResult.TraceEnd, TraceHitColor.ToFColor(true), true);
				::DrawDebugPoint(&World, HitResult.ImpactPoint, 16.f, TraceColor.ToFColor(true), true);
			}
			else
			{
				// no hit means all red
				::DrawDebugLine(&World, HitResult.TraceStart, HitResult.TraceEnd, TraceColor.ToFColor(true), true);
			}
		});
	}

	return bSuccess;
}

bool UMassProjectileDamageProcessor_DrawCapsules = false;
FAutoConsoleVariableRef CVarUMassProjectileDamageProcessor_DrawCapsules(TEXT("pm.UMassProjectileDamageProcessor_DrawCapsules"), UMassProjectileDamageProcessor_DrawCapsules, TEXT("UMassProjectileDamageProcessor: Debug draw capsules used for collisions detection"));

bool DidCollideWithEntity(const FVector& StartLocation, const FVector& EndLocation, const float Radius, const bool& DrawCapsules, const UWorld& World, const FMassEntityView& OtherEntityView)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassProjectileDamageProcessor.DidCollideWithEntity);

	const FCapsule ProjectileCapsule(StartLocation, EndLocation, Radius);
	const FCapsule& OtherEntityCapsule = MakeCapsuleForEntity(OtherEntityView);
	const bool& bDidCollide = TestCapsuleCapsule(ProjectileCapsule, OtherEntityCapsule);

	if (DrawCapsules || UMassProjectileDamageProcessor_DrawCapsules)
	{
		AsyncTask(ENamedThreads::GameThread, [&World, bDidCollide, ProjectileCapsule, OtherEntityCapsule]()
		{
			const auto& CapsuleColor = bDidCollide ? FLinearColor::Green : FLinearColor::Red;
			DrawCapsule(ProjectileCapsule, World, CapsuleColor);
			DrawCapsule(OtherEntityCapsule, World, CapsuleColor);
		});
	}

	return bDidCollide;
}

bool CanProjectileDamageEntity(const FProjectileDamagableFragment* ProjectileDamagableFragment, const float& ProjectileCaliber)
{
	return ProjectileDamagableFragment && ProjectileCaliber >= ProjectileDamagableFragment->MinCaliberForDamage;
}

bool UMassProjectileDamageProcessor_SkipDealingDamage = false;
FAutoConsoleVariableRef CVarUMassProjectileDamageProcessor_SkipDealingDamage(TEXT("pm.UMassProjectileDamageProcessor_SkipDealingDamage"), UMassProjectileDamageProcessor_SkipDealingDamage, TEXT("UMassProjectileDamageProcessor: Skip dealing damage"));

bool UMassProjectileDamageProcessor_DrawDamageDealt = false;
FAutoConsoleVariableRef CVarUUMassProjectileDamageProcessor_DrawDamageDealt(TEXT("pm.UMassProjectileDamageProcessor_DrawDamageDealt"), UMassProjectileDamageProcessor_DrawDamageDealt, TEXT("UMassProjectileDamageProcessor: Debug draw damage dealt"));

void DealDamage(const FVector& ImpactLocation, const FMassEntityView EntityToDealDamageToView, const FProjectileDamageFragment& ProjectileDamageFragment, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& SoldiersToDestroy, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& PlayersToDestroy, UWorld* World, const bool OverrideSplashDamageWithRegularDamage = false)
{
	FMassHealthFragment* EntityToDealDamageToHealthFragment = EntityToDealDamageToView.GetFragmentDataPtr<FMassHealthFragment>();
	if (!EntityToDealDamageToHealthFragment)
	{
		return;
	}

	const bool& bCanProjectileDamageOtherEntity = CanProjectileDamageEntity(EntityToDealDamageToView.GetFragmentDataPtr<FProjectileDamagableFragment>(), ProjectileDamageFragment.Caliber);
	if (!bCanProjectileDamageOtherEntity)
	{
		return;
	}

	const FTransformFragment* EntityToDealDamageToTransformFragment = EntityToDealDamageToView.GetFragmentDataPtr<FTransformFragment>();
	if (!EntityToDealDamageToTransformFragment)
	{
		return;
	}
	const auto EntityToDealDamageToLocation = EntityToDealDamageToTransformFragment->GetTransform().GetLocation();

	// If splash damage calculate scaled amount of damage based on distance from impact.
	int16 DamageToDeal = ProjectileDamageFragment.DamagePerHit;
	if (ProjectileDamageFragment.SplashDamageRadius > 0 && !OverrideSplashDamageWithRegularDamage)
	{
		const auto DistanceBetweenImpactAndEntityToDealDamageTo = (ImpactLocation - EntityToDealDamageToLocation).Size();
		const auto SplashDamageScale = ProjectileDamageFragment.SplashDamageRadius - DistanceBetweenImpactAndEntityToDealDamageTo;
		if (SplashDamageScale <= 0)
		{
			return;
		}

		DamageToDeal = SplashDamageScale / ProjectileDamageFragment.SplashDamageRadius * ProjectileDamageFragment.DamagePerHit;
	}

	EntityToDealDamageToHealthFragment->Value -= DamageToDeal;

	// Handle health reaching 0.
	if (EntityToDealDamageToHealthFragment->Value <= 0)
	{
		const bool bHasPlayerTag = EntityToDealDamageToView.HasTag<FMassPlayerControllableCharacterTag>();
		if (!bHasPlayerTag)
		{
			SoldiersToDestroy.Enqueue(EntityToDealDamageToView.GetEntity());
		}
		else {
			PlayersToDestroy.Enqueue(EntityToDealDamageToView.GetEntity());
		}
	}

	if (UMassProjectileDamageProcessor_DrawDamageDealt)
	{
		AsyncTask(ENamedThreads::GameThread, [World, DamageToDeal, EntityToDealDamageToLocation]()
		{
			DrawDebugString(World, EntityToDealDamageToLocation, FString::FromInt(DamageToDeal), nullptr, FColor::Red, 5.f);
		});
	}
}

void HandleProjectImpactSoundPerception(UWorld* World, const FVector& Location, const FMassEntityHandle& CollidedEntity, const UMassEntitySubsystem& EntitySubsystem)
{
	const bool& bHasCollidedEntity = CollidedEntity.IsSet();
	bool bIsCollidedEntityOnTeam1;

	if (bHasCollidedEntity)
	{
		const FMassEntityView CollidedEntityView(EntitySubsystem, CollidedEntity);
		const FTeamMemberFragment* CollidedEntityTeamMemberFragment = CollidedEntityView.GetFragmentDataPtr<FTeamMemberFragment>();
		if (CollidedEntityTeamMemberFragment)
		{
			bIsCollidedEntityOnTeam1 = CollidedEntityTeamMemberFragment->IsOnTeam1;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HandleProjectImpactSoundPerception: Collided Entity does not have expected FTeamMemberFragment."));
		}
	}

	AsyncTask(ENamedThreads::GameThread, [World, Location = Location, bHasCollidedEntity, bIsCollidedEntityOnTeam1]()
	{
		UMassSoundPerceptionSubsystem* SoundPerceptionSubsystem = UWorld::GetSubsystem<UMassSoundPerceptionSubsystem>(World);
		check(SoundPerceptionSubsystem);
		if (bHasCollidedEntity)
		{
			SoundPerceptionSubsystem->AddSoundPerception(Location, bIsCollidedEntityOnTeam1);
		}
		else
		{
			SoundPerceptionSubsystem->AddSoundPerception(Location);
		}
	});
}

void HandleProjectileImpact(TQueue<FMassEntityHandle, EQueueMode::Mpsc>& ProjectilesToDestroy, const FMassEntityHandle Entity, UWorld* World, const FProjectileDamageFragment& ProjectileDamageFragment, const FVector& Location, const TProjectileDamageObstacleItemArray& CloseEntities, const bool& DrawLineTraces, const UMassEntitySubsystem& EntitySubsystem, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& SoldiersToDestroy, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& PlayersToDestroy, const FMassEntityHandle& CollidedEntity)
{
	ProjectilesToDestroy.Enqueue(Entity);

	HandleProjectImpactSoundPerception(World, Location, CollidedEntity, EntitySubsystem);

	if (ProjectileDamageFragment.ExplosionEntityConfigIndex >= 0)
	{
		UMassVisualEffectsSubsystem* MassVisualEffectsSubsystem = UWorld::GetSubsystem<UMassVisualEffectsSubsystem>(World);
		check(MassVisualEffectsSubsystem);

		// Must be done async because we can't spawn Mass entities in the middle of a Mass processor's Execute method.
		AsyncTask(ENamedThreads::GameThread, [MassVisualEffectsSubsystem, ExplosionEntityConfigIndex = ProjectileDamageFragment.ExplosionEntityConfigIndex, Location]()
		{
			MassVisualEffectsSubsystem->SpawnEntity(ExplosionEntityConfigIndex, Location);
		});
	}

	if (UMassProjectileDamageProcessor_SkipDealingDamage)
	{
		return;
	}

	if (CloseEntities.Num() == 0)
	{
		return;
	}

	const bool bDealSplashDamage = ProjectileDamageFragment.SplashDamageRadius > 0;
	if (bDealSplashDamage)
	{
		for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
		{
			if (!EntitySubsystem.IsEntityValid(OtherEntity.Entity))
			{
				continue;
			}
			FMassEntityView OtherEntityEntityView(EntitySubsystem, OtherEntity.Entity);
			if (CollidedEntity.IsValid() && CollidedEntity == OtherEntity.Entity)
			{
				// Deal full damage (not splash damage) to entity which was collided with.
				DealDamage(Location, OtherEntityEntityView, ProjectileDamageFragment, SoldiersToDestroy, PlayersToDestroy, World, true);
				continue;
			}
			const FTransformFragment* OtherEntityTransformFragment = OtherEntityEntityView.GetFragmentDataPtr<FTransformFragment>();
			if (!OtherEntityTransformFragment)
			{
				continue;
			}

			if (!DidCollideViaLineTrace(*World, Location, OtherEntityTransformFragment->GetTransform().GetLocation(), DrawLineTraces))
			{
				DealDamage(Location, OtherEntityEntityView, ProjectileDamageFragment, SoldiersToDestroy, PlayersToDestroy, World);
			}

		}
	}
	else if (CollidedEntity.IsValid())
	{
		const FMassEntityView OtherEntityEntityView(EntitySubsystem, CollidedEntity);
		DealDamage(Location, OtherEntityEntityView, ProjectileDamageFragment, SoldiersToDestroy, PlayersToDestroy, World);
	}
}

void ProcessProjectileDamageEntity(FMassExecutionContext& Context, FMassEntityHandle Entity, const UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FTransformFragment& Location, const FAgentRadiusFragment& Radius, const FProjectileDamageFragment& ProjectileDamageFragment, TProjectileDamageObstacleItemArray& OutCloseEntities, const FMassPreviousLocationFragment& PreviousLocationFragment, const bool& DrawLineTraces, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& ProjectilesToDestroy, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& SoldiersToDestroy, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& PlayersToDestroy)
{
	UWorld* World = EntitySubsystem.GetWorld();

	const FVector& CurrentLocation = Location.GetTransform().GetLocation();
	const float DistanceFromLastFrameToCurrentFrame = (CurrentLocation - PreviousLocationFragment.Location).Size();
	const bool bHasCloseEntity = GetClosestEntities(Entity, EntitySubsystem, AvoidanceObstacleGrid, CurrentLocation, DistanceFromLastFrameToCurrentFrame, OutCloseEntities);

	// If collide via line trace, we hit the environment, so destroy projectile and deal splash damage if needed.
	if (DidCollideViaLineTrace(*World, PreviousLocationFragment.Location, CurrentLocation, DrawLineTraces))
	{
		HandleProjectileImpact(ProjectilesToDestroy, Entity, World, ProjectileDamageFragment, CurrentLocation, OutCloseEntities, DrawLineTraces, EntitySubsystem, SoldiersToDestroy, PlayersToDestroy, FMassEntityHandle());
		return;
	}

	if (!bHasCloseEntity) {
		return;
	}
	
	for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : OutCloseEntities)
	{
		if (!EntitySubsystem.IsEntityValid(OtherEntity.Entity))
		{
			continue;
		}

		FMassEntityView ClosestOtherEntityView(EntitySubsystem, OtherEntity.Entity);

		if (!DidCollideWithEntity(PreviousLocationFragment.Location, CurrentLocation, Radius.Radius, DrawLineTraces, *World, ClosestOtherEntityView))
		{
			continue;
		}

		HandleProjectileImpact(ProjectilesToDestroy, Entity, World, ProjectileDamageFragment, CurrentLocation, OutCloseEntities, DrawLineTraces, EntitySubsystem, SoldiersToDestroy, PlayersToDestroy, OtherEntity.Entity);

		// If we got here, we've collided with an entity, so no need to check other close entities.
		return;
	}
}

bool UMassProjectileDamageProcessor_UseParallelForEachEntityChunk = true;
FAutoConsoleVariableRef CVarUMassProjectileDamageProcessor_UseParallelForEachEntityChunk(TEXT("pm.UMassProjectileDamageProcessor_UseParallelForEachEntityChunk"), UMassProjectileDamageProcessor_UseParallelForEachEntityChunk, TEXT("Use ParallelForEachEntityChunk in UMassProjectileDamageProcessor::Execute to improve performance"));

void ProcessQueues(TQueue<FMassEntityHandle, EQueueMode::Mpsc>& ProjectilesToDestroy, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& SoldiersToDestroy, TQueue<FMassEntityHandle, EQueueMode::Mpsc>& PlayersToDestroy, const UWorld* World, const FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassProjectileDamageProcessor.ProcessQueues);

	// Destroy projectiles.
	while (!ProjectilesToDestroy.IsEmpty())
	{
		FMassEntityHandle EntityToDestroy;
		const bool bSuccess = ProjectilesToDestroy.Dequeue(EntityToDestroy);
		check(bSuccess);
		Context.Defer().DestroyEntity(EntityToDestroy);
	}

	// Destroy AI soldiers.
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(World);
	check(MilitaryStructureSubsystem);
	while (!SoldiersToDestroy.IsEmpty())
	{
		FMassEntityHandle EntityToDestroy;
		const bool bSuccess = SoldiersToDestroy.Dequeue(EntityToDestroy);
		check(bSuccess);
		Context.Defer().DestroyEntity(EntityToDestroy);
		MilitaryStructureSubsystem->DestroyEntity(EntityToDestroy);
	}

	// Destroy player soldiers.
	UMassPlayerSubsystem* PlayerSubsystem = UWorld::GetSubsystem<UMassPlayerSubsystem>(World);
	check(PlayerSubsystem);
	while (!PlayersToDestroy.IsEmpty())
	{
		FMassEntityHandle EntityToDestroy;
		const bool bSuccess = PlayersToDestroy.Dequeue(EntityToDestroy);
		check(bSuccess);
		AActor* OtherActor = PlayerSubsystem->GetActorForEntity(EntityToDestroy);
		check(OtherActor);
		ACommanderCharacter* Character = CastChecked<ACommanderCharacter>(OtherActor);
		AsyncTask(ENamedThreads::GameThread, [Character]()
		{
			Character->DidDie();
		});
	}
}

void UMassProjectileDamageProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMassProjectileDamageProcessor);

	if (!NavigationSubsystem)
	{
		return;
	}

	TQueue<FMassEntityHandle, EQueueMode::Mpsc> ProjectilesToDestroy;
	TQueue<FMassEntityHandle, EQueueMode::Mpsc> SoldiersToDestroy;
	TQueue<FMassEntityHandle, EQueueMode::Mpsc> PlayersToDestroy;

	auto ExecuteFunction = [&EntitySubsystem, &NavigationSubsystem = NavigationSubsystem, &ProjectilesToDestroy, &SoldiersToDestroy, &PlayersToDestroy](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FProjectileDamageFragment> ProjectileDamageList = Context.GetFragmentView<FProjectileDamageFragment>();
		const TArrayView<FMassPreviousLocationFragment> PreviousLocationList = Context.GetMutableFragmentView<FMassPreviousLocationFragment>();
		const FDebugParameters& DebugParameters = Context.GetConstSharedFragment<FDebugParameters>();

		TProjectileDamageObstacleItemArray CloseEntities;

		// Used for storing sorted list of nearest obstacles.
		struct FSortedObstacle
		{
			FVector LocationCached;
			FVector Forward;
			FMassNavigationObstacleItem ObstacleItem;
			float SqDist;
		};

		// TODO: We're incorrectly assuming all obstacles can get damaged by projectile.
		const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGridMutable();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			ProcessProjectileDamageEntity(Context, Context.GetEntity(EntityIndex), EntitySubsystem, AvoidanceObstacleGrid, LocationList[EntityIndex], RadiusList[EntityIndex], ProjectileDamageList[EntityIndex], CloseEntities, PreviousLocationList[EntityIndex], DebugParameters.DrawLineTraces, ProjectilesToDestroy, SoldiersToDestroy, PlayersToDestroy);
			PreviousLocationList[EntityIndex].Location = LocationList[EntityIndex].GetTransform().GetLocation();
		}
	};

	if (UMassProjectileDamageProcessor_UseParallelForEachEntityChunk)
	{
		EntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}
	else
	{
		EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}

	ProcessQueues(ProjectilesToDestroy, SoldiersToDestroy, PlayersToDestroy, EntitySubsystem.GetWorld(), Context);
}
