// Fill out your copyright notice in the Description page of Project Settings.


#include "MassEnemyTargetFinderProcessor.h"

#include "MassEntityView.h"
#include "MassNavigationSubsystem.h"
#include "MassLODTypes.h"
#include "MassCommonFragments.h"
#include "MassTrackTargetProcessor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MassProjectileDamageProcessor.h"
#include <MassNavigationTypes.h>
#include "MassNavigationFragments.h"
#include "MassMoveTargetForwardCompleteProcessor.h"
#include "MassTrackedVehicleOrientationProcessor.h"

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
	TargetEntityTemplate.SearchBreadth = SearchBreadth;

	BuildContext.AddFragment<FTargetEntityFragment>();
	BuildContext.AddTag<FMassNeedsEnemyTargetTag>();

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntitySubsystem);
	const FConstSharedStruct SharedParametersFragment = EntitySubsystem->GetOrCreateConstSharedFragment(UE::StructUtils::GetStructCrc32(FConstStructView::Make(SharedParameters)), SharedParameters);
	BuildContext.AddConstSharedFragment(SharedParametersFragment);
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
	EntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FNeedsEnemyTargetSharedParameters>(EMassFragmentPresence::All);
}

void UMassEnemyTargetFinderProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
	SoundPerceptionSubsystem = UWorld::GetSubsystem<UMassSoundPerceptionSubsystem>(Owner.GetWorld());
}

static const FBox BoxForPhase(const uint8& FinderPhase, const FTransform& SearchCenterTransform, const uint8& SearchBreadth, const FMassEntityHandle& Entity, const UWorld* World)
{
	const uint8 BoxXSegment = FinderPhase / (UMassEnemyTargetFinderProcessor_FinderPhaseCount / SearchBreadth);
	const uint8 BoxYSegment = FinderPhase % (UMassEnemyTargetFinderProcessor_FinderPhaseCount / SearchBreadth);
	const float XWidth = SearchBreadth * UMassEnemyTargetFinderProcessor_CellSize;

	const FVector& Center = SearchCenterTransform.GetLocation();
	const FVector& RotationForwardVector = SearchCenterTransform.GetRotation().GetForwardVector();
	const FVector& RotationRightVector = SearchCenterTransform.GetRotation().GetRightVector();
	const FVector BoxBottomLeft = Center - RotationRightVector * (XWidth / 2.f);
	
	if (UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [World, BoxBottomLeft, RotationForwardVector, RotationRightVector, Center]()
		{
			const FVector CenterOffsetVertical = Center + FVector(0.f, 0.f, 1000.f);
			DrawDebugDirectionalArrow(World, CenterOffsetVertical, CenterOffsetVertical + RotationForwardVector * 100, 10.f, FColor::Green, false, 2.f);
			DrawDebugDirectionalArrow(World, CenterOffsetVertical, CenterOffsetVertical + RotationRightVector * 100, 10.f, FColor::Red, false, 2.f);
			DrawDebugPoint(World, BoxBottomLeft + FVector(0.f, 0.f, 1000.f), 5.f, FColor::Yellow, false, 2.f);
		});
	}

	FVector PhaseBoxBottomLeft = BoxBottomLeft + RotationRightVector * UMassEnemyTargetFinderProcessor_CellSize * BoxXSegment + RotationForwardVector * UMassEnemyTargetFinderProcessor_CellSize * BoxYSegment;
	FVector PhaseBoxTopRight = PhaseBoxBottomLeft + RotationRightVector * UMassEnemyTargetFinderProcessor_CellSize + RotationForwardVector * UMassEnemyTargetFinderProcessor_CellSize;

	const auto Pivot = (PhaseBoxBottomLeft + PhaseBoxTopRight) / 2.f;
	
	auto PhaseBoxBottomLeftDir = PhaseBoxBottomLeft - Pivot;
	PhaseBoxBottomLeftDir = SearchCenterTransform.GetRotation().UnrotateVector(PhaseBoxBottomLeftDir);
	PhaseBoxBottomLeft = PhaseBoxBottomLeftDir + PhaseBoxBottomLeft;

	return FBox(PhaseBoxBottomLeft, PhaseBoxBottomLeft + FVector(UMassEnemyTargetFinderProcessor_CellSize, UMassEnemyTargetFinderProcessor_CellSize, 0.f));
}

bool UMassEnemyTargetFinderProcessor_DrawEntitiesSearching = false;
FAutoConsoleVariableRef CVar_UMassEnemyTargetFinderProcessor_DrawEntitiesSearching(TEXT("pm.UMassEnemyTargetFinderProcessor_DrawEntitiesSearching"), UMassEnemyTargetFinderProcessor_DrawEntitiesSearching, TEXT("UMassEnemyTargetFinderProcessor_DrawEntitiesSearching"));

// TODO: Find out how to not duplicate from MassProjectileDamageProcessor.cpp. Right now only difference is number in template type for TFixedAllocator.
static void FindCloseObstacles(const FTransform& SearchCenterTransform, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
	TArray<FMassNavigationObstacleItem, TFixedAllocator<10>>& OutCloseEntities, const int32 MaxResults, const uint8& FinderPhase, const bool& DrawSearchAreas, const UWorld* World, const uint8& SearchBreadth, const FMassEntityHandle& Entity)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_FindCloseObstacles);
	OutCloseEntities.Reset();
	const FBox QueryBox = BoxForPhase(FinderPhase, SearchCenterTransform, SearchBreadth, Entity, World);

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

	bool bIsComplete = false;
	for (const FSortingCell& SortedCell : Cells)
	{
		if (const FNavigationObstacleHashGrid2D::FCell* Cell = AvoidanceObstacleGrid.FindCell(SortedCell.X, SortedCell.Y, SortedCell.Level))
		{
			const TSparseArray<FNavigationObstacleHashGrid2D::FItem>& Items = AvoidanceObstacleGrid.GetItems();
			for (int32 Idx = Cell->First; Idx != INDEX_NONE; Idx = Items[Idx].Next)
			{
				OutCloseEntities.Add(Items[Idx].ID);
				if (OutCloseEntities.Num() >= MaxResults)
				{
					bIsComplete = true;
					break;
				}
			}

			if (bIsComplete)
			{
				break;
			}
		}
	}

	if (DrawSearchAreas || UE::Mass::Debug::IsDebuggingEntity(Entity))
	{
		AsyncTask(ENamedThreads::GameThread, [QueryBox, World, FinderPhase, NumCloseEntities = OutCloseEntities.Num()]()
		{
			const FVector Center = (QueryBox.Max + QueryBox.Min) / 2.f;
			FVector Extent(UMassEnemyTargetFinderProcessor_CellSize / 2, UMassEnemyTargetFinderProcessor_CellSize / 2, 1000.f);
			DrawDebugBox(World, Center, Extent, FColor::Green, false, 2.f);
			DrawDebugString(World, Center, FString::Printf(TEXT("%d (%d)"), FinderPhase, NumCloseEntities), nullptr, FColor::Green, 2.f);
		});
	}
}

bool GetClosestEnemy(const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FTransform& EntityTransform, TArray<FMassNavigationObstacleItem, TFixedAllocator<10>>& CloseEntities, FMassEntityHandle& OutTargetEntity, const bool IsEntityOnTeam1, const uint8& FinderPhase, const bool& DrawSearchAreas, const uint8& SearchBreadth)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_GetClosestEnemy);

	FindCloseObstacles(EntityTransform, AvoidanceObstacleGrid, CloseEntities, 10, FinderPhase, DrawSearchAreas, EntitySubsystem.GetWorld(), SearchBreadth, Entity);

	for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
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

		// Skip entities that aren't team members.
		if (!OtherEntityTeamMemberFragment) {
			continue;
		}

		// Skip same team.
		if (IsEntityOnTeam1 == OtherEntityTeamMemberFragment->IsOnTeam1) {
			continue;
		}

		OutTargetEntity = OtherEntity.Entity;
		return true;
	}

	OutTargetEntity = UMassEntitySubsystem::InvalidEntity;
	return false;
}

bool CanEntityDamageTargetEntity(const float& TargetMinCaliberForDamage, const FProjectileDamagableFragment* TargetEntityProjectileDamagableFragment)
{
	return TargetEntityProjectileDamagableFragment && TargetMinCaliberForDamage >= TargetEntityProjectileDamagableFragment->MinCaliberForDamage;
}

bool IsTargetEntityVisibleViaSphereTrace(const UWorld& World, const FVector& StartLocation, const FVector& EndLocation)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_IsTargetEntityVisibleViaSphereTrace);
	FHitResult Result;
	static const float Radius = 20.f; // TODO: don't hard-code
	bool bFoundBlockingHit = UKismetSystemLibrary::SphereTraceSingle(World.GetLevel(0)->Actors[0], StartLocation, EndLocation, Radius, TraceTypeQuery1, false, TArray<AActor*>(), EDrawDebugTrace::Type::None, Result, false);
	return !bFoundBlockingHit;
}

bool ProcessEntityForVisualTarget(TQueue<FMassEntityHandle>& TargetFinderEntityQueue, FMassEntityHandle Entity, UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FTransformFragment& Location, FTargetEntityFragment& TargetEntityFragment, const bool IsEntityOnTeam1, TArray<FMassNavigationObstacleItem, TFixedAllocator<10>>& CloseEntities, const uint8& FinderPhase, FMassExecutionContext& Context, const bool& DrawSearchAreas)
{
	const FTransform& EntityTransform = Location.GetTransform();
	const FVector& EntityLocation = EntityTransform.GetLocation();
	FMassEntityHandle TargetEntity;
	auto bFoundTarget = GetClosestEnemy(Entity, EntitySubsystem, AvoidanceObstacleGrid, EntityTransform, CloseEntities, TargetEntity, IsEntityOnTeam1, FinderPhase, DrawSearchAreas, TargetEntityFragment.SearchBreadth);
	if (!bFoundTarget) {
		return false;
	}

	FMassEntityView TargetEntityView(EntitySubsystem, TargetEntity);
	
	const bool& bCanEntityDamageTargetEntity = CanEntityDamageTargetEntity(TargetEntityFragment.TargetMinCaliberForDamage, TargetEntityView.GetFragmentDataPtr<FProjectileDamagableFragment>());
	if (!bCanEntityDamageTargetEntity)
	{
		return false;
	}

	FTransformFragment& TargetTransformFragment = TargetEntityView.GetFragmentData<FTransformFragment>();

	const bool& bIsEntitySoldier = Context.DoesArchetypeHaveTag<FMassProjectileDamagableSoldierTag>();
	const FVector ProjectileOffset = FVector(0.f, 0.f, UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(bIsEntitySoldier));
	const bool& bTargetEntityVisibleViaSphereTrace = IsTargetEntityVisibleViaSphereTrace(*EntitySubsystem.GetWorld(), EntityLocation + ProjectileOffset, TargetTransformFragment.GetTransform().GetLocation() + ProjectileOffset);
	if (!bTargetEntityVisibleViaSphereTrace)
	{
		return false;
	}

	TargetEntityFragment.Entity = TargetEntity;
	TargetFinderEntityQueue.Enqueue(Entity);

	return true;
}

bool UMassEnemyTargetFinderProcessor_DrawTrackedSounds = false;
FAutoConsoleVariableRef CVarUMassEnemyTargetFinderProcessor_DrawTrackedSounds(TEXT("pm.UMassEnemyTargetFinderProcessor_DrawTrackedSounds"), UMassEnemyTargetFinderProcessor_DrawTrackedSounds, TEXT("UMassEnemyTargetFinderProcessor: Draw Tracked Sounds"));

void ProcessEntityForAudioTarget(UMassSoundPerceptionSubsystem* SoundPerceptionSubsystem, const FTransform& EntityTransform, FMassMoveTargetFragment& MoveTargetFragment, const bool& bIsEntityOnTeam1)
{
	UWorld* World = SoundPerceptionSubsystem->GetWorld();
	const FVector& EntityLocation = EntityTransform.GetLocation();
	FVector OutSoundSource;
	const bool& bIsFacingMoveTarget = IsTransformFacingDirection(EntityTransform, MoveTargetFragment.Forward);
	const bool& bIsCurrentlyStanding = MoveTargetFragment.GetCurrentAction() == EMassMovementAction::Stand ||
		(MoveTargetFragment.GetCurrentAction() == EMassMovementAction::Move && MoveTargetFragment.GetCurrentActionID() == 0);
	if (SoundPerceptionSubsystem->HasSoundAtLocation(EntityLocation, OutSoundSource, !bIsEntityOnTeam1) && bIsFacingMoveTarget && bIsCurrentlyStanding)
	{
		const FVector& NewGlobalDirection = (OutSoundSource - EntityLocation).GetSafeNormal();

		MoveTargetFragment.CreateNewAction(EMassMovementAction::Stand, *World);
		MoveTargetFragment.Center = EntityLocation;
		MoveTargetFragment.Forward = NewGlobalDirection;

		if (UMassEnemyTargetFinderProcessor_DrawTrackedSounds)
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
	if (UMassEnemyTargetFinderProcessor_DrawEntitiesSearching || UE::Mass::Debug::IsDebuggingEntity(Entity))
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

	if (!NavigationSubsystem || UMassEnemyTargetFinderProcessor_SkipFindingTargets)
	{
		return;
	}

	TQueue<FMassEntityHandle> TargetFinderEntityQueue;

	auto ExecuteFunction = [&EntitySubsystem, &NavigationSubsystem = NavigationSubsystem, &FinderPhase = FinderPhase, &TargetFinderEntityQueue, &SoundPerceptionSubsystem = SoundPerceptionSubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();
		const TArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetMutableFragmentView<FMassMoveTargetFragment>();
		const FNeedsEnemyTargetSharedParameters& SharedParameters = Context.GetConstSharedFragment<FNeedsEnemyTargetSharedParameters>();

		// Used for storing sorted list of nearest entities.
		struct FSortedObstacle
		{
			FVector LocationCached;
			FVector Forward;
			FMassNavigationObstacleItem ObstacleItem;
			float SqDist;
		};

		// TODO: We're incorrectly assuming all obstacles can be targets.
		const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGridMutable();

		const int32 NumJobs = SharedParameters.ParallelJobCount;
		const int32 CountPerJob = (NumEntities + NumJobs - 1) / NumJobs; // ceil(NumEntities / NumJobs)

		ParallelFor(NumJobs, [&](int32 JobIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_ParallelFor);
			TArray<FMassNavigationObstacleItem, TFixedAllocator<10>> CloseEntities;

			const int32 StartIndex = JobIndex * CountPerJob;
			const int32 EndIndexExclusive = StartIndex + CountPerJob;
			for (int32 EntityIndex = StartIndex; (EntityIndex < NumEntities) && (EntityIndex < EndIndexExclusive); ++EntityIndex)
			{
				const bool& bFoundVisualTarget = ProcessEntityForVisualTarget(TargetFinderEntityQueue, Context.GetEntity(EntityIndex), EntitySubsystem, AvoidanceObstacleGrid, LocationList[EntityIndex], TargetEntityList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, CloseEntities, FinderPhase, Context, SharedParameters.DrawSearchAreas);
				if (!bFoundVisualTarget)
				{
					ProcessEntityForAudioTarget(SoundPerceptionSubsystem, LocationList[EntityIndex].GetTransform(), MoveTargetList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1);
				}
				DrawEntitySearchingIfNeeded(EntitySubsystem.GetWorld(), LocationList[EntityIndex].GetTransform().GetLocation(), Context.GetEntity(EntityIndex));
			}
		});
	};

	if (UMassEnemyTargetFinderProcessor_UseParallelForEachEntityChunk)
	{
		EntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	} else {
		EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_ProcessQueue);
		while (!TargetFinderEntityQueue.IsEmpty())
		{
			FMassEntityHandle TargetFinderEntity;
			bool bSuccess = TargetFinderEntityQueue.Dequeue(TargetFinderEntity);
			check(bSuccess);

			Context.Defer().AddTag<FMassWillNeedEnemyTargetTag>(TargetFinderEntity);
			Context.Defer().RemoveTag<FMassNeedsEnemyTargetTag>(TargetFinderEntity);
		}
	}

	FinderPhase = (FinderPhase + 1) % UMassEnemyTargetFinderProcessor_FinderPhaseCount;
}

/*static*/ const float UMassEnemyTargetFinderProcessor::GetProjectileSpawnLocationZOffset(const bool& bIsSoldier)
{
	return bIsSoldier ? 150.f : 180.f; // TODO: don't hard-code
}
