#include "MassTargetGridProcessors.h"

#include "MassCommonUtils.h"
#include "MassCommandBuffer.h"
#include "MassCommonFragments.h"
#include "MassSimulationLOD.h"
#include "MassMovementTypes.h"
#include "MassMovementFragments.h"
#include "MassEntityView.h"
#include "Engine/World.h"


//----------------------------------------------------------------------//
//  UMassTargetGridProcessor
//----------------------------------------------------------------------//
UMassTargetGridProcessor::UMassTargetGridProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
}

void UMassTargetGridProcessor::ConfigureQueries()
{
	FMassEntityQuery BaseEntityQuery;
	BaseEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BaseEntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	BaseEntityQuery.AddRequirement<FMassTargetGridCellLocationFragment>(EMassFragmentAccess::ReadWrite);

	AddToGridEntityQuery = BaseEntityQuery;
	AddToGridEntityQuery.AddTagRequirement<FMassInTargetGridTag>(EMassFragmentPresence::None);

	UpdateGridEntityQuery = BaseEntityQuery;
	UpdateGridEntityQuery.AddTagRequirement<FMassInTargetGridTag>(EMassFragmentPresence::All);
}

void UMassTargetGridProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	TargetFinderSubsystem = UWorld::GetSubsystem<UMassTargetFinderSubsystem>(Owner.GetWorld());
}

void UMassTargetGridProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	if (!TargetFinderSubsystem)
	{
		return;
	}

	// can't be ParallelFor due to Move() not being thread-safe
	AddToGridEntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		TConstArrayView<FAgentRadiusFragment> RadiiList = Context.GetFragmentView<FAgentRadiusFragment>();
		TArrayView<FMassTargetGridCellLocationFragment> TargetGridCellLocationList = Context.GetMutableFragmentView<FMassTargetGridCellLocationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			// Add to the grid
			const FVector NewPos = LocationList[EntityIndex].GetTransform().GetLocation();
			const float Radius = RadiiList[EntityIndex].Radius;

			const FMassEntityHandle& TargetEntity = Context.GetEntity(EntityIndex);
			
			const FBox NewBounds(NewPos - FVector(Radius, Radius, 0.f), NewPos + FVector(Radius, Radius, 0.f));
			TargetGridCellLocationList[EntityIndex].CellLoc = TargetFinderSubsystem->GetTargetGridMutable().Add(TargetEntity, NewBounds);

			Context.Defer().AddTag<FMassInTargetGridTag>(TargetEntity);
		}
	});

	UpdateGridEntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		TConstArrayView<FAgentRadiusFragment> RadiiList = Context.GetFragmentView<FAgentRadiusFragment>();
		TArrayView<FMassTargetGridCellLocationFragment> TargetGridCellLocationList = Context.GetMutableFragmentView<FMassTargetGridCellLocationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			// Update position in grid
			const FVector NewPos = LocationList[EntityIndex].GetTransform().GetLocation();
			const float Radius = RadiiList[EntityIndex].Radius;
			const FMassEntityHandle& TargetEntity = Context.GetEntity(EntityIndex);

			const FBox NewBounds(NewPos - FVector(Radius, Radius, 0.f), NewPos + FVector(Radius, Radius, 0.f));
			TargetGridCellLocationList[EntityIndex].CellLoc = TargetFinderSubsystem->GetTargetGridMutable().Move(TargetEntity, TargetGridCellLocationList[EntityIndex].CellLoc, NewBounds);
		}
	});
}

//----------------------------------------------------------------------//
//  UMassTargetRemoverProcessor
//----------------------------------------------------------------------//
UMassTargetRemoverProcessor::UMassTargetRemoverProcessor()
{
	ObservedType = FMassTargetGridCellLocationFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::All);
}

void UMassTargetRemoverProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassTargetGridCellLocationFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassTargetRemoverProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);
	TargetFinderSubsystem = UWorld::GetSubsystem<UMassTargetFinderSubsystem>(Owner.GetWorld());
}

void UMassTargetRemoverProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	if (!TargetFinderSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TArrayView<FMassTargetGridCellLocationFragment> TargetGridCellLocationList = Context.GetMutableFragmentView<FMassTargetGridCellLocationFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle& TargetEntity = Context.GetEntity(i);
			TargetFinderSubsystem->GetTargetGridMutable().Remove(TargetEntity, TargetGridCellLocationList[i].CellLoc);
		}
	});
}
