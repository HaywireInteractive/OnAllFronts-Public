// Mostly copied from CitySample MassCrowdUpdateISMVertexAnimationProcessor.cpp 

#include "MassGenericUpdateISMVertexAnimationProcessor.h"
#include "MassVisualizationComponent.h"
#include "MassRepresentationSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassRepresentationFragments.h"
#include "MassCommonFragments.h"
#include "MassLODFragments.h"
#include "AnimToTextureInstancePlaybackHelpers.h"
#include "MassCommonTypes.h"

//----------------------------------------------------------------------//
//  UMassGenericUpdateISMVertexAnimationProcessor
//----------------------------------------------------------------------//
UMassGenericUpdateISMVertexAnimationProcessor::UMassGenericUpdateISMVertexAnimationProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Tasks);
}

void UMassGenericUpdateISMVertexAnimationProcessor::ConfigureQueries()
{
	Super::ConfigureQueries();

	EntityQuery.AddRequirement<FGenericAnimationFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassGenericUpdateISMVertexAnimationProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedStaticMeshInfoArrayView ISMInfo = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

		TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
		TArrayView<FGenericAnimationFragment> AnimationDataList = Context.GetMutableFragmentView<FGenericAnimationFragment>();

		const int32 NumEntities = Context.GetNumEntities();
		for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
		{
			const FMassEntityHandle Entity = Context.GetEntity(EntityIdx);
			const FTransformFragment& TransformFragment = TransformList[EntityIdx];
			const FMassRepresentationLODFragment& RepresentationLOD = RepresentationLODList[EntityIdx];
			FMassRepresentationFragment& Representation = RepresentationList[EntityIdx];
			FGenericAnimationFragment& AnimationData = AnimationDataList[EntityIdx];

			if (Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
			{
				UpdateISMTransform(GetTypeHash(Context.GetEntity(EntityIdx)), ISMInfo[Representation.StaticMeshDescIndex], TransformFragment.GetTransform(), Representation.PrevTransform, RepresentationLOD.LODSignificance, Representation.PrevLODSignificance);
				UpdateISMVertexAnimation(ISMInfo[Representation.StaticMeshDescIndex], AnimationData, RepresentationLOD.LODSignificance, Representation.PrevLODSignificance);
			}
			Representation.PrevTransform = TransformFragment.GetTransform();
			Representation.PrevLODSignificance = RepresentationLOD.LODSignificance;
		}
	});
}

void UMassGenericUpdateISMVertexAnimationProcessor::UpdateISMVertexAnimation(FMassInstancedStaticMeshInfo& ISMInfo, FGenericAnimationFragment& AnimationData, const float LODSignificance, const float PrevLODSignificance, const int32 NumFloatsToPad /*= 0*/)
{
	FAnimToTextureInstancePlaybackData InstanceData;
	UAnimToTextureInstancePlaybackLibrary::AnimStateFromDataAsset(AnimationData.AnimToTextureData.Get(), AnimationData.AnimationStateIndex, InstanceData.CurrentState);
	InstanceData.CurrentState.GlobalStartTime = AnimationData.GlobalStartTime;
	InstanceData.CurrentState.PlayRate = AnimationData.PlayRate;
	ISMInfo.AddBatchedCustomData<FAnimToTextureInstancePlaybackData>(InstanceData, LODSignificance, PrevLODSignificance, NumFloatsToPad);
}
