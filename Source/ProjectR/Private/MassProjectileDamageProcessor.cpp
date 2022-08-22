#include "MassProjectileDamageProcessor.h"

#include "MassEntityView.h"
#include "MassLODTypes.h"
#include "MassCommonFragments.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MassPlayerSubsystem.h"
#include "Character/CommanderCharacter.h"

//----------------------------------------------------------------------//
//  UMassProjectileWithDamageTrait
//----------------------------------------------------------------------//
void UMassProjectileWithDamageTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassPreviousLocationFragment>();

	FProjectileDamageFragment& ProjectileDamageTemplate = BuildContext.AddFragment_GetRef<FProjectileDamageFragment>();
	ProjectileDamageTemplate.DamagePerHit = DamagePerHit;

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
//  UMassProjectileDamagableTrait
//----------------------------------------------------------------------//
void UMassProjectileDamagableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FMassProjectileDamagableTag>();
}

//----------------------------------------------------------------------//
//  UMassProjectileDamageProcessor
//----------------------------------------------------------------------//
UMassProjectileDamageProcessor::UMassProjectileDamageProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
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

static void FindCloseObstacles(const FVector& Center, const float SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
	TArray<FMassNavigationObstacleItem, TFixedAllocator<2>>& OutCloseEntities, const int32 MaxResults)
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
				if (OutCloseEntities.Num() >= MaxResults)
				{
					return;
				}
			}
		}
	}
}

void UMassProjectileDamageProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

// Returns true if found another entity.
bool GetClosestEntity(const FMassEntityHandle& Entity, UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FVector& Location, const float Radius, TArray<FMassNavigationObstacleItem, TFixedAllocator<2>>& CloseEntities, FMassEntityHandle& OutOtherEntity)
{
	FindCloseObstacles(Location, Radius, AvoidanceObstacleGrid, CloseEntities, 2);

	for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
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
		OutOtherEntity = OtherEntity.Entity;
		return true;
	}

	OutOtherEntity = UMassEntitySubsystem::InvalidEntity;
	return false;
}

bool DidCollideViaLineTrace(const UWorld &World, const FVector& StartLocation, const FVector &EndLocation, const bool& DrawLineTraces)
{
	FHitResult Result;
	const TArray<AActor*> ActorsToIgnore;
	EDrawDebugTrace::Type DrawDebugTraceType = DrawLineTraces ? EDrawDebugTrace::Type::Persistent : EDrawDebugTrace::Type::None;
	bool bSuccess = UKismetSystemLibrary::LineTraceSingle(World.GetLevel(0)->Actors[0], StartLocation, EndLocation, TraceTypeQuery1, false, ActorsToIgnore, DrawDebugTraceType, Result, false);
	return bSuccess;
}

struct FCapsule
{
	FVector a;
	FVector b;
	float r;
};

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
	if (a <= UE_SMALL_NUMBER && e <= UE_SMALL_NUMBER) {
		// Both segments degenerate into points
		s = t = 0.0f;
		c1 = p1;
		c2 = p2;
		return FVector::DotProduct(c1 - c2, c1 - c2);
	}
	if (a <= UE_SMALL_NUMBER) {
		// First segment degenerates into a point
		s = 0.0f;
		t = f / e; // s = 0 => t = (b*s + f) / e = f / e
		t = FMath::Clamp(t, 0.0f, 1.0f);
	}
	else {
		float c = FVector::DotProduct(d1, r);
		if (e <= UE_SMALL_NUMBER) {
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

void DrawCapsule(const FCapsule& Capsule, const UWorld& World, const FLinearColor &Color = FLinearColor::Red)
{
	FQuat const CapsuleRot = FRotationMatrix::MakeFromZ(Capsule.b - Capsule.a).ToQuat();
	DrawDebugCapsule(&World, GetCapsuleCenter(Capsule), GetCapsuleHalfHeight(Capsule), Capsule.r, CapsuleRot, Color.ToFColor(true), true);
}

bool DidCollideWithEntity(const FVector& StartLocation, const FVector& EndLocation, const float Radius, FTransformFragment* OtherTransformFragment, const bool& DrawCapsules, const UWorld& World)
{
	if (!OtherTransformFragment)
	{
		return false;
	}

	FVector OtherEntityLocation = OtherTransformFragment->GetTransform().GetLocation();

	FCapsule ProjectileCapsule;
	ProjectileCapsule.a = StartLocation;
	ProjectileCapsule.b = EndLocation;
	ProjectileCapsule.r = Radius;

	FCapsule OtherEntityCapsule;
	OtherEntityCapsule.a = OtherEntityLocation;
	static const float EntityHeight = 200.0f; // TODO: don't hard-code; add new fragment for this?
	static const float EntityRadius = 40.0f; // TODO: don't hard-code; read from other entity's AgentRadius?
	OtherEntityCapsule.b = OtherEntityLocation + FVector(0.f, 0.f, EntityHeight);
	OtherEntityCapsule.r = EntityRadius;

	if (DrawCapsules)
	{
		DrawCapsule(ProjectileCapsule, World);
		DrawCapsule(OtherEntityCapsule, World, FLinearColor::White);
	}

	return TestCapsuleCapsule(ProjectileCapsule, OtherEntityCapsule);
}

void ProcessProjectileDamageEntity(FMassExecutionContext& Context, FMassEntityHandle Entity, UMassEntitySubsystem& EntitySubsystem, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid, const FTransformFragment& Location, const FAgentRadiusFragment& Radius, const int16 DamagePerHit, TArray<FMassNavigationObstacleItem, TFixedAllocator<2>>& CloseEntities, const FMassPreviousLocationFragment& PreviousLocationFragment, const bool& DrawLineTraces)
{
	// If collide via line trace, we hit the environment, so destroy projectile.
	const FVector& CurrentLocation = Location.GetTransform().GetLocation();
	if (DidCollideViaLineTrace(*EntitySubsystem.GetWorld(), PreviousLocationFragment.Location, CurrentLocation, DrawLineTraces))
	{
		Context.Defer().DestroyEntity(Entity);
		return;
	}

	FMassEntityHandle OtherEntity;
	bool bHasCloseEntity = GetClosestEntity(Entity, EntitySubsystem, AvoidanceObstacleGrid, Location.GetTransform().GetTranslation(), Radius.Radius, CloseEntities, OtherEntity);
	if (!bHasCloseEntity) {
		return;
	}

	FMassEntityView OtherEntityView(EntitySubsystem, OtherEntity);
	FTransformFragment* OtherTransformFragment = OtherEntityView.GetFragmentDataPtr<FTransformFragment>();

	if (!DidCollideWithEntity(PreviousLocationFragment.Location, CurrentLocation, Radius.Radius, OtherTransformFragment, DrawLineTraces, *EntitySubsystem.GetWorld()))
	{
		return;
	}

	Context.Defer().DestroyEntity(Entity);

	FMassHealthFragment* OtherHealthFragment = OtherEntityView.GetFragmentDataPtr<FMassHealthFragment>();
	if (!OtherHealthFragment)
	{
		return;
	}

	OtherHealthFragment->Value -= DamagePerHit;

	// Handle health reaching 0.
	if (OtherHealthFragment->Value <= 0)
	{
		bool bHasPlayerTag = OtherEntityView.HasTag<FMassPlayerControllableCharacterTag>();
		if (!bHasPlayerTag)
		{
			Context.Defer().DestroyEntity(OtherEntity);
		} else {
			UMassPlayerSubsystem* PlayerSubsystem = UWorld::GetSubsystem<UMassPlayerSubsystem>(EntitySubsystem.GetWorld());
			check(PlayerSubsystem);
			AActor *otherActor = PlayerSubsystem->GetActorForEntity(OtherEntity);
			check(otherActor);
			ACommanderCharacter* Character = Cast<ACommanderCharacter>(otherActor);
			check(Character);
			AsyncTask(ENamedThreads::GameThread, [Character]()
			{
				Character->Respawn(true);
			});
		}
	}
}

void UMassProjectileDamageProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassProjectileDamageProcessor);

	if (!NavigationSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&EntitySubsystem, &NavigationSubsystem = NavigationSubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FProjectileDamageFragment> ProjectileDamageList = Context.GetFragmentView<FProjectileDamageFragment>();
		const TArrayView<FMassPreviousLocationFragment> PreviousLocationList = Context.GetMutableFragmentView<FMassPreviousLocationFragment>();
		const FDebugParameters& DebugParameters = Context.GetConstSharedFragment<FDebugParameters>();

		// Arrays used to store close obstacles
		TArray<FMassNavigationObstacleItem, TFixedAllocator<2>> CloseEntities;

		// Used for storing sorted list or nearest obstacles.
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
			ProcessProjectileDamageEntity(Context, Context.GetEntity(EntityIndex), EntitySubsystem, AvoidanceObstacleGrid, LocationList[EntityIndex], RadiusList[EntityIndex], ProjectileDamageList[EntityIndex].DamagePerHit, CloseEntities, PreviousLocationList[EntityIndex], DebugParameters.DrawLineTraces);
			PreviousLocationList[EntityIndex].Location = LocationList[EntityIndex].GetTransform().GetLocation();
		}
	});
}
