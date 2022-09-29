// Fill out your copyright notice in the Description page of Project Settings.


#include "MassCollisionProcessor.h"

#include <MassMovementFragments.h>
#include <MassCommonFragments.h>
#include "MassEntityView.h"
#include <MassLODTypes.h>

static const uint32 GUMassCollisionProcessor_MaxClosestEntitiesToFind = 3;
typedef TArray<FMassNavigationObstacleItem, TFixedAllocator<GUMassCollisionProcessor_MaxClosestEntitiesToFind>> TCollisionItemArray;

//----------------------------------------------------------------------//
//  UMassCollisionTrait
//----------------------------------------------------------------------//
void UMassCollisionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	FCollisionCapsuleParametersFragment& CollisionCapsuleParametersFragment = BuildContext.AddFragment_GetRef<FCollisionCapsuleParametersFragment>();
	CollisionCapsuleParametersFragment.bIsCapsuleAlongForwardVector = bIsCapsuleAlongForwardVector;
	CollisionCapsuleParametersFragment.CapsuleRadius = CapsuleRadius;
	CollisionCapsuleParametersFragment.CapsuleLength = CapsuleLength;
	CollisionCapsuleParametersFragment.CapsuleCenterOffset = CapsuleCenterOffset;

	if (bEnableCollisionProcessor)
	{
		BuildContext.AddTag<FMassCollisionTag>();
	}
}

//----------------------------------------------------------------------//
//  UMassCollisionProcessor
//----------------------------------------------------------------------//
UMassCollisionProcessor::UMassCollisionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	
	// Needs to execute right before UMassApplyMovementProcessor. By copying the ExecutionOrder from that processor, we'll run right before it.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UMassCollisionProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FCollisionCapsuleParametersFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassCollisionTag>(EMassFragmentPresence::All);
}

void UMassCollisionProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

FCapsule MakeCapsuleForEntity(const FCollisionCapsuleParametersFragment& CollisionCapsuleParametersFragment, const FTransform& EntityTransform)
{
	FCapsule Capsule;
	const FVector& EntityLocation = EntityTransform.GetLocation();

	if (CollisionCapsuleParametersFragment.bIsCapsuleAlongForwardVector)
	{
		FVector Center = EntityLocation + CollisionCapsuleParametersFragment.CapsuleCenterOffset;
		FVector Forward = EntityTransform.GetRotation().GetForwardVector();

		Capsule.a = Center + Forward * (CollisionCapsuleParametersFragment.CapsuleLength / 2.f);
		Capsule.b = Center + Forward * (-CollisionCapsuleParametersFragment.CapsuleLength / 2.f);
		Capsule.r = CollisionCapsuleParametersFragment.CapsuleRadius;
	}
	else
	{
		Capsule.a = EntityLocation;
		Capsule.b = EntityLocation + FVector(0.f, 0.f, CollisionCapsuleParametersFragment.CapsuleLength);
		Capsule.r = CollisionCapsuleParametersFragment.CapsuleRadius;
	}

	return Capsule;
}

FCapsule MakeCapsuleForEntity(const FMassEntityView& EntityView)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("MakeCapsuleForEntity");

	FCollisionCapsuleParametersFragment* CollisionCapsuleParametersFragment = EntityView.GetFragmentDataPtr<FCollisionCapsuleParametersFragment>();
	FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>();
	if (!CollisionCapsuleParametersFragment || !TransformFragment)
	{
		UE_LOG(LogTemp, Error, TEXT("MakeCapsuleForEntity: Expected FCollisionCapsuleParametersFragment and FTransformFragment on Entity."));
		return FCapsule();
	}

	return MakeCapsuleForEntity(*CollisionCapsuleParametersFragment, TransformFragment->GetTransform());
}

float ClosestPtSegmentSegment(FVector p1, FVector q1, FVector p2, FVector q2,
	float& s, float& t, FVector& c1, FVector& c2)
{
	FVector d1 = q1 - p1; // Direction vector of segment S1
	FVector d2 = q2 - p2; // Direction vector of segment S2
	FVector r = p1 - p2;
	float a = FVector::DotProduct(d1, d1); // Squared length of segment S1, always nonnegative
	float e = FVector::DotProduct(d2, d2); // Squared length of segment S2, always nonnegative
	float f = FVector::DotProduct(d2, r);
	// Check if either or both segments degenerate into points
	if (a <= SMALL_NUMBER && e <= SMALL_NUMBER) {
		// Both segments degenerate into points
		s = t = 0.0f;
		c1 = p1;
		c2 = p2;
		return FVector::DotProduct(c1 - c2, c1 - c2);
	}
	if (a <= SMALL_NUMBER) {
		// First segment degenerates into a point
		s = 0.0f;
		t = f / e; // s = 0 => t = (b*s + f) / e = f / e
		t = FMath::Clamp(t, 0.0f, 1.0f);
	}
	else {
		float c = FVector::DotProduct(d1, r);
		if (e <= SMALL_NUMBER) {
			// Second segment degenerates into a point
			t = 0.0f;
			s = FMath::Clamp(-c / a, 0.0f, 1.0f); // t = 0 => s = (b*t - c) / a = -c / a
		}
		else {
			// The general nondegenerate case starts here
			float b = FVector::DotProduct(d1, d2);
			float denom = a * e - b * b; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary s (here 0)
			if (denom != 0.0f) {
				s = FMath::Clamp((b * f - c * e) / denom, 0.0f, 1.0f);
			}
			else s = 0.0f;
			// Compute point on L2 closest to S1(s) using
			// t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
			t = (b * s + f) / e;
			// If t in [0,1] done. Else clamp t, recompute s for the new value
			// of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
			// and clamp s to [0, 1]
			if (t < 0.0f) {
				t = 0.0f;
				s = FMath::Clamp(-c / a, 0.0f, 1.0f);
			}
			else if (t > 1.0f) {
				t = 1.0f;
				s = FMath::Clamp((b - c) / a, 0.0f, 1.0f);
			}
		}
	}
	c1 = p1 + d1 * s;
	c2 = p2 + d2 * t;
	return FVector::DotProduct(c1 - c2, c1 - c2);
}

bool TestCapsuleCapsule(FCapsule capsule1, FCapsule capsule2)
{
	// Compute (squared) distance between the inner structures of the capsules
	float s, t;
	FVector c1, c2;
	float dist2 = ClosestPtSegmentSegment(capsule1.a, capsule1.b,
		capsule2.a, capsule2.b, s, t, c1, c2);
	// If (squared) distance smaller than (squared) sum of radii, they collide
	float radius = capsule1.r + capsule2.r;
	return dist2 <= radius * radius;
}

FVector GetCapsuleCenter(const FCapsule& Capsule)
{
	return (Capsule.b - Capsule.a) / 2.f + Capsule.a;
}

float GetCapsuleHalfHeight(const FCapsule& Capsule)
{
	return (Capsule.b - Capsule.a).Size() / 2.f;
}

void DrawCapsule(const FCapsule& Capsule, const UWorld& World, const FLinearColor& Color, const bool bPersistentLines, float LifeTime)
{
	FQuat const CapsuleRot = FRotationMatrix::MakeFromZ(Capsule.b - Capsule.a).ToQuat();
	DrawDebugCapsule(&World, GetCapsuleCenter(Capsule), GetCapsuleHalfHeight(Capsule), Capsule.r, CapsuleRot, Color.ToFColor(true), bPersistentLines, LifeTime);
}

// TODO: DRY with other processors
static void FindCloseObstacles(const FVector& Center, const float SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
	TCollisionItemArray& OutCloseEntities, const FMassEntityHandle& EntityToIgnore, const UMassEntitySubsystem& EntitySubsystem)
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
				if (Items[Idx].ID.Entity != EntityToIgnore && EntitySubsystem.IsEntityValid(Items[Idx].ID.Entity))
				{
					OutCloseEntities.Add(Items[Idx].ID);
					if (OutCloseEntities.Num() >= GUMassCollisionProcessor_MaxClosestEntitiesToFind)
					{
						return;
					}
				}
			}
		}
	}
}

bool UMassCollisionProcessor_DrawCapsules = false;
FAutoConsoleVariableRef CVarUMassCollisionProcessor_DrawCapsules(TEXT("pm.UMassCollisionProcessor_DrawCapsules"), UMassCollisionProcessor_DrawCapsules, TEXT("UMassCollisionProcessor: Debug draw capsules used for collisions detection"));

void ProcessEntity(FMassExecutionContext& Context, FMassEntityHandle Entity, const FTransform& Transform, TCollisionItemArray& OutCloseEntities, const float& AgentRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, UMassEntitySubsystem& EntitySubsystem, const FCollisionCapsuleParametersFragment& CollisionCapsuleParametersFragment, FMassForceFragment& ForceFragment, FMassVelocityFragment& VelocityFragment)
{
	FCapsule EntityCapsule = MakeCapsuleForEntity(CollisionCapsuleParametersFragment, Transform);

	FindCloseObstacles(Transform.GetLocation(), AgentRadius * 2, AvoidanceObstacleGrid, OutCloseEntities, Entity, EntitySubsystem);

	for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : OutCloseEntities)
	{
		FMassEntityView OtherEntityView(EntitySubsystem, OtherEntity.Entity);
		FTransformFragment* OtherTransformFragment = OtherEntityView.GetFragmentDataPtr<FTransformFragment>();
		FCollisionCapsuleParametersFragment* OtherCollisionCapsuleParametersFragment = OtherEntityView.GetFragmentDataPtr<FCollisionCapsuleParametersFragment>();
		FMassForceFragment* OtherForceFragment = OtherEntityView.GetFragmentDataPtr<FMassForceFragment>();
		FMassVelocityFragment* OtherVelocityFragment = OtherEntityView.GetFragmentDataPtr<FMassVelocityFragment>();

		// We don't check OtherForceFragment for null because players don't have that fragment.
		if (!OtherTransformFragment || !OtherCollisionCapsuleParametersFragment || !OtherVelocityFragment)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MassCollisionProcessor] ProcessEntity: OtherEntity (idx=%d,sn=%d) does not have one of FTransformFragment, FCollisionCapsuleParametersFragment, FMassVelocityFragment."), OtherEntity.Entity.Index, OtherEntity.Entity.SerialNumber);
			continue;
		}

		const FTransform& OtherTransform = OtherTransformFragment->GetTransform();
		FCapsule OtherEntityCapsule = MakeCapsuleForEntity(*OtherCollisionCapsuleParametersFragment, OtherTransform);

		if (TestCapsuleCapsule(EntityCapsule, OtherEntityCapsule))
		{
			const auto& OtherLocation = OtherTransform.GetLocation();
			const auto NewForce = FMath::IsNearlyEqual(VelocityFragment.Value.Size(), 0.f) ? Transform.GetLocation() - OtherLocation : -VelocityFragment.Value;
			ForceFragment.Value = NewForce;
			//VelocityFragment.Value = FVector::ZeroVector;

			const auto NewOtherForce = FMath::IsNearlyEqual(OtherVelocityFragment->Value.Size(), 0.f) ? OtherLocation - Transform.GetLocation() : -OtherVelocityFragment->Value;
			if (OtherForceFragment)
			{
				OtherForceFragment->Value = NewOtherForce;
				//OtherVelocityFragment->Value = FVector::ZeroVector;
			}

			if (UMassCollisionProcessor_DrawCapsules)
			{
				DrawCapsule(EntityCapsule, *EntitySubsystem.GetWorld());
				DrawCapsule(OtherEntityCapsule, *EntitySubsystem.GetWorld(), FLinearColor::Yellow);

				DrawDebugDirectionalArrow(EntitySubsystem.GetWorld(), Transform.GetLocation(), Transform.GetLocation() + NewForce, 30.f, FColor::Red, true);
				DrawDebugDirectionalArrow(EntitySubsystem.GetWorld(), OtherLocation, OtherLocation + NewOtherForce, 30.f, FColor::Yellow, true);
			}
		}
	}
}

// TODO: To make this work, we probably need to use a queue for updating fragments.
bool UMassCollisionProcessor_UseParallelForEachEntityChunk = false;

void UMassCollisionProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMassCollisionProcessor");

	if (!NavigationSubsystem)
	{
		return;
	}

	auto ExecuteFunction = [&EntitySubsystem, &NavigationSubsystem = NavigationSubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TArrayView<FMassVelocityFragment> VelocityList = Context.GetMutableFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TConstArrayView<FCollisionCapsuleParametersFragment> CollisionCapsuleParametersList = Context.GetFragmentView<FCollisionCapsuleParametersFragment>();

		TCollisionItemArray CloseEntities;

		// Used for storing sorted list of nearest obstacles.
		struct FSortedObstacle
		{
			FVector LocationCached;
			FVector Forward;
			FMassNavigationObstacleItem ObstacleItem;
			float SqDist;
		};

		const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGridMutable();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			ProcessEntity(Context, Context.GetEntity(EntityIndex), TransformList[EntityIndex].GetTransform(), CloseEntities, RadiusList[EntityIndex].Radius, AvoidanceObstacleGrid, EntitySubsystem, CollisionCapsuleParametersList[EntityIndex], ForceList[EntityIndex], VelocityList[EntityIndex]);
		}
	};

	if (UMassCollisionProcessor_UseParallelForEachEntityChunk)
	{
		EntityQuery.ParallelForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}
	else
	{
		EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, ExecuteFunction);
	}
}
