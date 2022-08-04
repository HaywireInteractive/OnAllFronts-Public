// Fill out your copyright notice in the Description page of Project Settings.


#include "MassEnemyTargetFinderProcessor.h"

#include "MassEntityView.h"
#include "MassNavigationSubsystem.h"
#include "MassLODTypes.h"
#include "MassCommonFragments.h"

//----------------------------------------------------------------------//
//  UMassTeamMemberTrait
//----------------------------------------------------------------------//
void UMassTeamMemberTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FTeamMemberFragment& TeamMemberTemplate = BuildContext.AddFragment_GetRef<FTeamMemberFragment>();
	TeamMemberTemplate.IsOnTeam1 = IsOnTeam1;
}

//----------------------------------------------------------------------//
//  UMassNeedsEnemyTargetTrait
//----------------------------------------------------------------------//
void UMassNeedsEnemyTargetTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FTargetEntityFragment>();
	BuildContext.AddTag<FMassNeedsEnemyTargetTag>();
}

//----------------------------------------------------------------------//
//  UMassEnemyTargetFinderProcessor
//----------------------------------------------------------------------//
UMassEnemyTargetFinderProcessor::UMassEnemyTargetFinderProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassEnemyTargetFinderProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTeamMemberFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTargetEntityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassNeedsEnemyTargetTag>(EMassFragmentPresence::All);
}

void UMassEnemyTargetFinderProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

static FBox GetBoxForFinderPhase(const uint8& FinderPhase, const FVector& Center, const float& SearchRadius)
{
	// assume x,y axes positive is up to right
	switch (FinderPhase)
	{
	case 0: // bottom left
	{
		const FVector ExtentTopRight(SearchRadius, SearchRadius, 0.f);
		return FBox(Center - ExtentTopRight, Center);
		break;
	}
	case 1: // bottom right
	{
		const FVector ExtentBottomRight(SearchRadius, -SearchRadius, 0.f);
		return FBox(Center, Center + ExtentBottomRight);
		break;
	}
	case 2: // top left
	{
		const FVector ExtentBottomRight(SearchRadius, -SearchRadius, 0.f);
		return FBox(Center - ExtentBottomRight, Center);
		break;
	}
	case 3: // top right
	{
		const FVector ExtentTopRight(SearchRadius, SearchRadius, 0.f);
		return FBox(Center, Center + ExtentTopRight);
		break;
	}
	default:
		check(false);
		return FBox();
	}
}

// TODO: Find out how to not duplicate from MassProjectileDamageProcessor.cpp. Right now only difference is number in template type for TFixedAllocator.
static void FindCloseObstacles(const FVector& Center, const float SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
	TArray<FMassNavigationObstacleItem, TFixedAllocator<10>>& OutCloseEntities, const int32 MaxResults, const uint8& FinderPhase)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_FindCloseObstacles);
	OutCloseEntities.Reset();
	FBox QueryBox = GetBoxForFinderPhase(FinderPhase, Center, SearchRadius);

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
				if (OutCloseEntities.Num() >= MaxResults)
				{
					return;
				}
			}
		}
	}
}

bool GetClosestEnemy(const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FVector& Location, TArray<FMassNavigationObstacleItem, TFixedAllocator<10>>& CloseEntities, FMassEntityHandle& OutTargetEntity, const bool IsEntityOnTeam1, const uint8& FinderPhase)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_GetClosestEnemy);

	static const float SearchRadius = 5000.f; // TODO: don't hard-code
	FindCloseObstacles(Location, SearchRadius, AvoidanceObstacleGrid, CloseEntities, 10, FinderPhase);

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

void ProcessEntity(TQueue<FMassEntityHandle>& TargetFinderEntityQueue, FMassEntityHandle Entity, UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FTransformFragment& Location, FTargetEntityFragment& TargetEntityFragment, const bool IsEntityOnTeam1, TArray<FMassNavigationObstacleItem, TFixedAllocator<10>>& CloseEntities, const uint8& FinderPhase)
{
	FMassEntityHandle TargetEntity;
	auto bFoundTarget = GetClosestEnemy(Entity, EntitySubsystem, AvoidanceObstacleGrid, Location.GetTransform().GetTranslation(), CloseEntities, TargetEntity, IsEntityOnTeam1, FinderPhase);
	if (!bFoundTarget) {
		return;
	}

	TargetEntityFragment.Entity = TargetEntity;

	TargetFinderEntityQueue.Enqueue(Entity);
}

void UMassEnemyTargetFinderProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor);

	if (!NavigationSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem, &NavigationSubsystem = NavigationSubsystem, &FinderPhase = FinderPhase](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTeamMemberFragment> TeamMemberList = Context.GetFragmentView<FTeamMemberFragment>();
		const TArrayView<FTargetEntityFragment> TargetEntityList = Context.GetMutableFragmentView<FTargetEntityFragment>();

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

		const int32 NumJobs = 60; // TODO

		TQueue<FMassEntityHandle> TargetFinderEntityQueue;

		ParallelFor(NumJobs, [&](int32 JobIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(UMassEnemyTargetFinderProcessor_ParallelFor);
			TArray<FMassNavigationObstacleItem, TFixedAllocator<10>> CloseEntities;

			for (int32 EntityIndex = JobIndex; EntityIndex < NumEntities; EntityIndex += NumJobs)
			{
				ProcessEntity(TargetFinderEntityQueue, Context.GetEntity(EntityIndex), EntitySubsystem, AvoidanceObstacleGrid, LocationList[EntityIndex], TargetEntityList[EntityIndex], TeamMemberList[EntityIndex].IsOnTeam1, CloseEntities, FinderPhase);
			}
		});

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
	});

	FinderPhase = (FinderPhase + 1) % 4;
}
