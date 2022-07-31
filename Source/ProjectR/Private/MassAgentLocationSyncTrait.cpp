// Fill out your copyright notice in the Description page of Project Settings.


#include "MassAgentLocationSyncTrait.h"
#include "MassVisualizer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassSimulationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassActorSubsystem.h"

//----------------------------------------------------------------------//
//  UMassProjectileTrait
//----------------------------------------------------------------------//
void UMassProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddTag<FMassProjectileTag>();
}

//----------------------------------------------------------------------//
//  UMassProjectileUpdateCollisionTrait
//----------------------------------------------------------------------//
void UMassProjectileUpdateCollisionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddTag<FMassProjectileUpdateCollisionTag>();
}

//----------------------------------------------------------------------//
//  UMassAgentLocationSyncTrait
//----------------------------------------------------------------------//
void UMassAgentLocationSyncTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FTransformFragment>();

	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::ActorToMass))
	{
		// TODO
	}

	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::MassToActor))
	{
		BuildContext.AddTranslator<ULocationToActorTranslator>();
	}
}

//----------------------------------------------------------------------//
//  ULocationToActorTranslator
//----------------------------------------------------------------------//
ULocationToActorTranslator::ULocationToActorTranslator()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	RequiredTags.Add<FMassLocationCopyToActorTag>();
}

void ULocationToActorTranslator::ConfigureQueries()
{
	AddRequiredTagsToQuery(EntityQuery);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void ULocationToActorTranslator::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this](FMassExecutionContext& Context)
		{
			const TArrayView<FMassActorFragment> ActorList = Context.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

			const int32 NumEntities = Context.GetNumEntities();

			for (int32 EntityIdx = 0; EntityIdx < NumEntities; ++EntityIdx)
			{
				FMassActorFragment& ActorInfo = ActorList[EntityIdx];
				const FTransformFragment& TransformFragment = TransformList[EntityIdx];
				AActor* actor = ActorInfo.GetMutable();
				if (actor) {
					actor->SetActorLocation(TransformFragment.GetTransform().GetLocation());
				}
			}
		});
}

//----------------------------------------------------------------------//
//  UMassProjectileUpdateISMProcessor
//----------------------------------------------------------------------//
UMassProjectileUpdateISMProcessor::UMassProjectileUpdateISMProcessor()
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
}

void UMassProjectileUpdateISMProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();

	EntityQuery.AddTagRequirement<FMassProjectileTag>(EMassFragmentPresence::All);
}

//----------------------------------------------------------------------//
//  UMassProjectileUpdateISMCollisionsProcessor
//----------------------------------------------------------------------//
UMassProjectileUpdateISMCollisionsProcessor::UMassProjectileUpdateISMCollisionsProcessor()
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
}

void UMassProjectileUpdateISMCollisionsProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();

	EntityQuery.AddTagRequirement<FMassProjectileUpdateCollisionTag>(EMassFragmentPresence::All);
}

void UMassProjectileUpdateISMCollisionsProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	AMassVisualizer* Visualizer = nullptr;

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&Visualizer](FMassExecutionContext& Context)
	{
		if (Visualizer)
		{
			return;
		}

		UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedStaticMeshInfoArrayView ISMInfo = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

		if (UMassProjectileRepresentationSubsystem* ProjectileRepresentationSubsystem = dynamic_cast<UMassProjectileRepresentationSubsystem*>(RepresentationSubsystem))
		{
			Visualizer = ProjectileRepresentationSubsystem->GetVisualizer();
		}
	});

	if (Visualizer)
	{
		const TArray<USceneComponent*>& childComponents = Visualizer->GetRootComponent()->GetAttachChildren();
		for (USceneComponent* component : childComponents)
		{
			UInstancedStaticMeshComponent* ismComponent = static_cast<UInstancedStaticMeshComponent*>(component);

			// Workaround issue with ISMs transformed with UpdateInstances where collisions only update by disabling and enabling collision
			ismComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ismComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
}

//----------------------------------------------------------------------//
//  UMassProjectileRepresentationSubsystem
//----------------------------------------------------------------------//
AMassVisualizer* UMassProjectileRepresentationSubsystem::GetVisualizer()
{
	return Visualizer;
}

void UMassProjectileRepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		UMassSimulationSubsystem* SimSystem = World->GetSubsystem<UMassSimulationSubsystem>();
		check(SimSystem);
		SimSystem->GetOnProcessingPhaseStarted(EMassProcessingPhase::PrePhysics).AddUObject(this, &UMassProjectileRepresentationSubsystem::OnPrePhysicsProcessingPhaseStarted, EMassProcessingPhase::PrePhysics);
	}
}

void UMassProjectileRepresentationSubsystem::OnPrePhysicsProcessingPhaseStarted(const float DeltaSeconds, const EMassProcessingPhase Phase)
{
	if (!bNeedUpdateCollisions) {
		return;
	}

	const TArray<USceneComponent*>& childComponents = Visualizer->GetRootComponent()->GetAttachChildren();
	for (USceneComponent* component : childComponents)
	{
		bNeedUpdateCollisions = false;
		UInstancedStaticMeshComponent* ismComponent = static_cast<UInstancedStaticMeshComponent*>(component);
		ismComponent->SetCollisionProfileName(TEXT("OverlapAll"));
		ismComponent->SetGenerateOverlapEvents(true);
	}
}

//----------------------------------------------------------------------//
//  UMassProjectileVisualizationTrait
//----------------------------------------------------------------------//
UMassProjectileVisualizationTrait::UMassProjectileVisualizationTrait()
{
	Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Low] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Off] = EMassRepresentationType::StaticMeshInstance;
	Params.bKeepLowResActors = false;
	Params.bKeepActorExtraFrame = false;
	Params.bSpreadFirstVisualizationUpdate = false;
	Params.WorldPartitionGridNameContainingCollision = NAME_None;
	Params.NotVisibleUpdateRate = 0.5f;

	LODParams.BaseLODDistance[EMassLOD::High] = 0.f;
	LODParams.BaseLODDistance[EMassLOD::Medium] = 0.f;
	LODParams.BaseLODDistance[EMassLOD::Low] = 3000.f;
	LODParams.BaseLODDistance[EMassLOD::Off] = 6000.f;

	LODParams.VisibleLODDistance[EMassLOD::High] = 0.f;
	LODParams.VisibleLODDistance[EMassLOD::Medium] = 0.f;
	LODParams.VisibleLODDistance[EMassLOD::Low] = 6000.f;
	LODParams.VisibleLODDistance[EMassLOD::Off] = 50000.f;

	LODParams.LODMaxCount[EMassLOD::High] = 10;
	LODParams.LODMaxCount[EMassLOD::Medium] = 20;
	LODParams.LODMaxCount[EMassLOD::Low] = 500;
	LODParams.LODMaxCount[EMassLOD::Off] = TNumericLimits<int32>::Max();

	LODParams.BufferHysteresisOnDistancePercentage = 20.0f;
	LODParams.DistanceToFrustum = 0.0f;
	LODParams.DistanceToFrustumHysteresis = 0.0f;

	LODParams.FilterTag = FMassProjectileVisualizationTag::StaticStruct();
}

void UMassProjectileVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);
	BuildContext.AddTag<FMassProjectileVisualizationTag>();
}

//----------------------------------------------------------------------//
// UMassProjectileVisualizationProcessor
//----------------------------------------------------------------------//
UMassProjectileVisualizationProcessor::UMassProjectileVisualizationProcessor()
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);

	bAutoRegisterWithProcessingPhases = true;

	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);

	bRequiresGameThreadExecution = true;
}

void UMassProjectileVisualizationProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();
	EntityQuery.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
}

//----------------------------------------------------------------------//
// UMassProjectileVisualizationLODProcessor
//----------------------------------------------------------------------//
UMassProjectileVisualizationLODProcessor::UMassProjectileVisualizationLODProcessor()
{
	bAutoRegisterWithProcessingPhases = true;

	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);

	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::LOD;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LODCollector);
}

void UMassProjectileVisualizationLODProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();

	CloseEntityQuery.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
	CloseEntityAdjustDistanceQuery.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
	FarEntityQuery.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
	DebugEntityQuery.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);

	FilterTag = FMassProjectileVisualizationTag::StaticStruct();
}

void UMassProjectileVisualizationLODProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ProjectileVisualizationLOD"))

	Super::Execute(EntitySubsystem, Context);
}

//----------------------------------------------------------------------//
// UMassProjectileLODCollectorProcessor
//----------------------------------------------------------------------//
UMassProjectileLODCollectorProcessor::UMassProjectileLODCollectorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
}

void UMassProjectileLODCollectorProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();

	EntityQuery_VisibleRangeAndOnLOD.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
	EntityQuery_VisibleRangeOnly.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
	EntityQuery_OnLODOnly.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
	EntityQuery_NotVisibleRangeAndOffLOD.AddTagRequirement<FMassProjectileVisualizationTag>(EMassFragmentPresence::All);
}
