// Mostly a copy of City Sample MassCrowdAnimationProcessor.cpp

#include "MassGenericAnimationProcessor.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
//#include "AnimToTextureInstancePlaybackHelpers.h"
#include "MassVisualizationComponent.h"
#include "MassRepresentationFragments.h"
//#include "ContextualAnimSceneAsset.h"
//#include "AnimToTextureDataAsset.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassLookAtFragments.h"
#include "MassLODFragments.h"
#include "MassRepresentationTypes.h"
#include "MotionWarpingComponent.h"
//#include "Animation/MassCrowdAnimInstance.h"
//#include "Animation/MassPlayerAnimInstance.h"
#include "MassEntityView.h"
#include "MassAIBehaviorTypes.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "MassMovementFragments.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "MassCrowdRepresentationSubsystem.h"

//void FMassGenericMontageFragment::Request(const UE::CrowdInteractionAnim::FRequest& InRequest)
//{
//	InteractionRequest = InRequest;
//	SkippedTime = 0.0f;
//	MontageInstance.Initialize(InRequest.QueryResult.Animation.Get(), InteractionRequest.QueryResult.AnimStartTime);
//}
//
//void FMassGenericMontageFragment::Clear()
//{
//	*this = FMassGenericMontageFragment();
//}

UMassGenericAnimationProcessor::UMassGenericAnimationProcessor()
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);

	bRequiresGameThreadExecution = true;
}

void UMassGenericAnimationProcessor::UpdateAnimationFragmentData(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context, float GlobalTime, TArray<FMassEntityHandle, TInlineAllocator<32>>& ActorEntities)
{
	TArrayView<FGenericAnimationFragment> AnimationDataList = Context.GetMutableFragmentView<FGenericAnimationFragment>();
	//TConstArrayView<FMassGenericMontageFragment> MontageDataList = Context.GetFragmentView<FMassGenericMontageFragment>();
	TConstArrayView<FMassRepresentationFragment> VisualizationList = Context.GetFragmentView<FMassRepresentationFragment>();
	TConstArrayView<FMassActorFragment> ActorInfoList = Context.GetFragmentView<FMassActorFragment>();

	const int32 NumEntities = Context.GetNumEntities();
	for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	{
		FGenericAnimationFragment& AnimationData = AnimationDataList[EntityIdx];
		const FMassRepresentationFragment& Visualization = VisualizationList[EntityIdx];
		const FMassActorFragment& ActorFragment = ActorInfoList[EntityIdx];

		if (!ActorFragment.IsOwnedByMass())
		{
			continue;
		}

		const bool bWasActor = (Visualization.PrevRepresentation == EMassRepresentationType::HighResSpawnedActor) || (Visualization.PrevRepresentation == EMassRepresentationType::LowResSpawnedActor);
		const bool bIsActor = (Visualization.CurrentRepresentation == EMassRepresentationType::HighResSpawnedActor) || (Visualization.CurrentRepresentation == EMassRepresentationType::LowResSpawnedActor);
		AnimationData.bSwappedThisFrame = (bWasActor != bIsActor);

		//if (!MontageDataList.IsEmpty() && MontageDataList[EntityIdx].MontageInstance.SequenceChangedThisFrame())
		//{
		//	AnimationData.GlobalStartTime = GlobalTime - MontageDataList[EntityIdx].MontageInstance.GetPositionInSection();
		//}

		switch (Visualization.CurrentRepresentation)
		{
		case EMassRepresentationType::LowResSpawnedActor:
		case EMassRepresentationType::HighResSpawnedActor:
		{
			FMassEntityHandle Entity = Context.GetEntity(EntityIdx);
			ActorEntities.Add(Entity);
			break;
		}
		default:
			break;
		}
	}
}

void UMassGenericAnimationProcessor::UpdateVertexAnimationState(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context, float GlobalTime)
{
	const int32 NumEntities = Context.GetNumEntities();
	TArrayView<FGenericAnimationFragment> AnimationDataList = Context.GetMutableFragmentView<FGenericAnimationFragment>();
	//TConstArrayView<FMassGenericMontageFragment> MontageDataList = Context.GetFragmentView<FMassGenericMontageFragment>();
	TConstArrayView<FMassRepresentationFragment> VisualizationList = Context.GetFragmentView<FMassRepresentationFragment>();
	TConstArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetFragmentView<FMassRepresentationLODFragment>();
	TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();

	for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	{
		FGenericAnimationFragment& AnimationData = AnimationDataList[EntityIdx];

		const FMassRepresentationFragment& Visualization = VisualizationList[EntityIdx];
		const FMassVelocityFragment& Velocity = VelocityList[EntityIdx];

		// Need current anim state to update for skeletal meshes to do a smooth blend between poses
		if (Visualization.CurrentRepresentation != EMassRepresentationType::None)
		{
			int32 StateIndex = 0;

			FMassEntityHandle Entity = Context.GetEntity(EntityIdx);
			//const UAnimSequence* Sequence = MontageDataList.IsEmpty() ? nullptr : MontageDataList[EntityIdx].MontageInstance.GetSequence();
			//if (Sequence)
			//{
			//	StateIndex = AnimationData.AnimToTextureData.IsValid() ? AnimationData.AnimToTextureData->GetIndexFromAnimSequence(Sequence) : 0;
			//}
			//else
			//{
				// @todo: Make a better way to map desired anim states here. Currently the anim texture index to access is hard-coded.
				const float VelocitySizeSq = Velocity.Value.SizeSquared();
				const bool bIsWalking = Velocity.Value.SizeSquared() > MoveThresholdSq;
				if(bIsWalking)
				{
					StateIndex = 1;
					const float AuthoredAnimSpeed = 140.0f;
					const float PrevPlayRate = AnimationData.PlayRate;
					AnimationData.PlayRate = FMath::Clamp(FMath::Sqrt(VelocitySizeSq / (AuthoredAnimSpeed * AuthoredAnimSpeed)), 0.8f, 2.0f);

					// Need to conserve current frame on a playrate switch so (GlobalTime - Offset1) * Playrate1 == (GlobalTime - Offset2) * Playrate2
					AnimationData.GlobalStartTime = GlobalTime - PrevPlayRate * (GlobalTime - AnimationData.GlobalStartTime) / AnimationData.PlayRate;
				}
				else
				{
					AnimationData.PlayRate = 1.0f;
					StateIndex = 0;
				}
			//}
			AnimationData.AnimationStateIndex = StateIndex;
		}
	}
}

void UMassGenericAnimationProcessor::ConfigureQueries()
{
	AnimationEntityQuery_Conditional.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	AnimationEntityQuery_Conditional.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	AnimationEntityQuery_Conditional.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	AnimationEntityQuery_Conditional.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	AnimationEntityQuery_Conditional.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	AnimationEntityQuery_Conditional.AddRequirement<FMassLookAtFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	AnimationEntityQuery_Conditional.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	AnimationEntityQuery_Conditional.AddRequirement<FGenericAnimationFragment>(EMassFragmentAccess::ReadWrite);
	//AnimationEntityQuery_Conditional.AddRequirement<FMassGenericMontageFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	AnimationEntityQuery_Conditional.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
	AnimationEntityQuery_Conditional.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);

	//MontageEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	//MontageEntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	//MontageEntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	//MontageEntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	//MontageEntityQuery.AddRequirement<FGenericAnimationFragment>(EMassFragmentAccess::ReadOnly);
	//MontageEntityQuery.AddRequirement<FMassGenericMontageFragment>(EMassFragmentAccess::ReadWrite);
	//MontageEntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);

	//MontageEntityQuery_Conditional = MontageEntityQuery;
	//MontageEntityQuery_Conditional.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
}

void UMassGenericAnimationProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	World = Owner.GetWorld();
	check(World);
}

void UMassGenericAnimationProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	check(World);

	QUICK_SCOPE_CYCLE_COUNTER(UMassGenericAnimationProcessor_Run);

	const float GlobalTime = World->GetTimeSeconds();

	TArray<FMassEntityHandle, TInlineAllocator<32>> ActorEntities;
	
	//{
	//	QUICK_SCOPE_CYCLE_COUNTER(UMassGenericAnimationProcessor_UpdateMontage);
	//	MontageEntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this, GlobalTime, &ActorEntities, &EntitySubsystem](FMassExecutionContext& Context)
	//		{
	//			const int32 NumEntities = Context.GetNumEntities();
	//			TArrayView<FMassGenericMontageFragment> MontageDataList = Context.GetMutableFragmentView<FMassGenericMontageFragment>();
	//			if (!FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk(Context))
	//			{
	//				for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	//				{
	//					// If we are not updating animation, we still need to accumulate skipped time to fixup animation on the next update.
	//					FMassGenericMontageFragment& MontageData = MontageDataList[EntityIdx];

	//					MontageData.SkippedTime += Context.GetDeltaTimeSeconds();
	//				}
	//			}
	//			else
	//			{
	//				TConstArrayView<FMassRepresentationFragment> VisualizationList = Context.GetFragmentView<FMassRepresentationFragment>();
	//				for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	//				{
	//					const FMassRepresentationFragment& Visualization = VisualizationList[EntityIdx];
	//					if (Visualization.CurrentRepresentation == EMassRepresentationType::None)
	//					{
	//						continue;
	//					}

	//					FMassGenericMontageFragment& MontageFragment = MontageDataList[EntityIdx];

	//					const float MontagePositionPreAdvance = MontageFragment.MontageInstance.GetPosition();
	//					const float MontageLength = MontageFragment.MontageInstance.GetLength();
	//					float AdjustedDeltaTime = Context.GetDeltaTimeSeconds() + MontageFragment.SkippedTime;
	//					const float AdjustedPositionPostAdvance = MontagePositionPreAdvance + AdjustedDeltaTime;
	//					if (AdjustedPositionPostAdvance > MontageLength)
	//					{
	//						// If we've skipped over the remaining duration of the montage clear our fragment
	//						MontageFragment.Clear();
	//						Context.Defer().PushCommand(FCommandRemoveFragment(Context.GetEntity(EntityIdx), FMassGenericMontageFragment::StaticStruct()));
	//					}
	//					else
	//					{
	//						MontageFragment.RootMotionParams.Clear();

	//						UE::VertexAnimation::FLightWeightMontageExtractionSettings ExtractionSettings;

	//						if (MontageFragment.InteractionRequest.AlignmentTrack != NAME_None && MontageFragment.SkippedTime > 0.0f)
	//						{
	//							const UContextualAnimSceneAsset* ContextualAnimAsset = MontageFragment.InteractionRequest.ContextualAnimAsset.Get();
	//							if (ContextualAnimAsset)
	//							{
	//								FContextualAnimQueryResult& QueryResult = MontageFragment.InteractionRequest.QueryResult;

	//								const FContextualAnimData* AnimData = ContextualAnimAsset->GetAnimDataForRoleAtIndex(MontageFragment.InteractionRequest.InteractorRole, QueryResult.DataIndex);

	//								const float WarpDuration = AnimData ? AnimData->GetSyncTimeForWarpSection(0) : 0.f;

	//								const float WarpDurationSkippedDelta = WarpDuration - MontagePositionPreAdvance;
	//								if (MontageFragment.SkippedTime > WarpDurationSkippedDelta)
	//								{
	//									// If we skipped past the warp, don't extract root motion for that portion, because we want to snap to the warp target before applying root motion.
	//									ExtractionSettings.bExtractRootMotion = false;
	//									MontageFragment.MontageInstance.Advance(WarpDurationSkippedDelta, GlobalTime, MontageFragment.RootMotionParams, ExtractionSettings);

	//									// Remaining time delta should not include warp duration we skipped
	//									AdjustedDeltaTime -= WarpDurationSkippedDelta;
	//								}
	//							}
	//						}

	//						ExtractionSettings.bExtractRootMotion = true;
	//						MontageFragment.MontageInstance.Advance(AdjustedDeltaTime, GlobalTime, MontageFragment.RootMotionParams, ExtractionSettings);
	//					}
	//				}
	//			}
	//		});
	//}

	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassGenericAnimationProcessor_UpdateAnimationFragmentData);
		AnimationEntityQuery_Conditional.ForEachEntityChunk(EntitySubsystem, Context, [this, GlobalTime, &ActorEntities, &EntitySubsystem](FMassExecutionContext& Context)
			{
				UMassGenericAnimationProcessor::UpdateAnimationFragmentData(EntitySubsystem, Context, GlobalTime, ActorEntities);
			});
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(UMassGenericAnimationProcessor_UpdateVertexAnimationState);
		AnimationEntityQuery_Conditional.ForEachEntityChunk(EntitySubsystem, Context, [this, GlobalTime, &EntitySubsystem](FMassExecutionContext& Context)
			{
				UMassGenericAnimationProcessor::UpdateVertexAnimationState(EntitySubsystem, Context, GlobalTime);
			});
	}

	//{
	//	QUICK_SCOPE_CYCLE_COUNTER(UMassGenericAnimationProcessor_ConsumeRootMotion);
	//	MontageEntityQuery_Conditional.ForEachEntityChunk(EntitySubsystem, Context, [this, &EntitySubsystem](FMassExecutionContext& Context)
	//		{
	//			TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
	//			TConstArrayView<FMassRepresentationFragment> VisualizationList = Context.GetFragmentView<FMassRepresentationFragment>();
	//			TConstArrayView<FGenericAnimationFragment> AnimationDataList = Context.GetFragmentView<FGenericAnimationFragment>();
	//			TArrayView<FMassGenericMontageFragment> MontageDataList = Context.GetMutableFragmentView<FMassGenericMontageFragment>();

	//			const int32 NumEntities = Context.GetNumEntities();
	//			for (int32 EntityIdx = 0; EntityIdx < NumEntities; EntityIdx++)
	//			{
	//				const FMassRepresentationFragment& Visualization = VisualizationList[EntityIdx];
	//				if (Visualization.CurrentRepresentation == EMassRepresentationType::None)
	//				{
	//					continue;
	//				}

	//				const FGenericAnimationFragment& AnimationData = AnimationDataList[EntityIdx];
	//				FTransformFragment& TransformFragment = TransformList[EntityIdx];
	//				FMassGenericMontageFragment& MontageFragment = MontageDataList[EntityIdx];

	//				const UContextualAnimSceneAsset* ContextualAnimAsset = MontageFragment.InteractionRequest.ContextualAnimAsset.Get();
	//				if (MontageFragment.InteractionRequest.AlignmentTrack != NAME_None && MontageFragment.MontageInstance.IsValid() && ContextualAnimAsset)
	//				{
	//					FContextualAnimQueryResult& QueryResult = MontageFragment.InteractionRequest.QueryResult;

	//					const FContextualAnimData* AnimData = ContextualAnimAsset->GetAnimDataForRoleAtIndex(MontageFragment.InteractionRequest.InteractorRole, QueryResult.DataIndex);

	//					const float WarpDuration = AnimData ? AnimData->GetSyncTimeForWarpSection(0) : 0.f;
	//					const float MontagePosition = MontageFragment.MontageInstance.GetPosition();

	//					FVector TargetLocation;
	//					FQuat TargetRotation;

	//					const FTransform& PrevTransform = TransformFragment.GetTransform();
	//					FQuat PrevRot = PrevTransform.GetRotation();
	//					FVector PrevLoc = PrevTransform.GetTranslation();

	//					// Simple lerp towards interaction sync point
	//					UE::CrowdInteractionAnim::FMotionWarpingScratch& Scratch = MontageFragment.MotionWarpingScratch;

	//					if (MontagePosition < WarpDuration)
	//					{
	//						if (Scratch.Duration < 0.0f)
	//						{
	//							Scratch.InitialLocation = PrevLoc;
	//							Scratch.InitialRotation = PrevRot;
	//							Scratch.TimeRemaining = WarpDuration - MontagePosition;
	//							Scratch.Duration = Scratch.TimeRemaining;
	//						}
	//						Scratch.TimeRemaining -= Context.GetDeltaTimeSeconds();

	//						const FTransform& SyncTransform = QueryResult.SyncTransform;

	//						const float Alpha = FMath::Clamp((Scratch.Duration - Scratch.TimeRemaining) / Scratch.Duration, 0.0f, 1.0f);
	//						TargetLocation = FMath::Lerp(Scratch.InitialLocation, SyncTransform.GetLocation(), Alpha);

	//						TargetRotation = FQuat::Slerp(Scratch.InitialRotation, SyncTransform.GetRotation(), FMath::Pow(Alpha, 1.5f));
	//						TargetRotation.Normalize();
	//					}
	//					// Apply root motion
	//					else
	//					{
	//						if (MontagePosition - MontageFragment.SkippedTime < WarpDuration)
	//						{
	//							// If we skipped past the warp duration, snap to our sync point before applying root motion
	//							const FTransform& SyncTransform = QueryResult.SyncTransform;
	//							PrevLoc = SyncTransform.GetLocation();
	//							PrevRot = SyncTransform.GetRotation();
	//						}

	//						Scratch.Duration = -1.0f;

	//						const FTransform& RootMotionTransform = MontageFragment.RootMotionParams.GetRootMotionTransform();

	//						const FQuat ComponentRot = PrevRot * FQuat(FVector::UpVector, -90.0f);
	//						TargetLocation = PrevLoc + ComponentRot.RotateVector(RootMotionTransform.GetTranslation());
	//						TargetRotation = RootMotionTransform.GetRotation() * PrevRot;
	//					}

	//					MontageFragment.SkippedTime = 0.0f;
	//					TransformFragment.GetMutableTransform().SetLocation(TargetLocation);
	//					TransformFragment.GetMutableTransform().SetRotation(TargetRotation);
	//				}
	//			}
	//		});
	//}
}

class UAnimInstance* UMassGenericAnimationProcessor::GetAnimInstanceFromActor(const AActor* Actor)
{
	const USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		SkeletalMeshComponent = Character->GetMesh();
	}
	else if (Actor)
	{
		SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
	}

	if (SkeletalMeshComponent)
	{
		return SkeletalMeshComponent->GetAnimInstance();
	}

	return nullptr;
}

UGenericAnimationFragmentInitializer::UGenericAnimationFragmentInitializer()
{
	ObservedType = FGenericAnimationFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
}

void UGenericAnimationFragmentInitializer::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FGenericAnimationFragment>(EMassFragmentAccess::ReadWrite);
}

void UGenericAnimationFragmentInitializer::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	UMassCrowdRepresentationSubsystem* RepresentationSubsystem = UWorld::GetSubsystem<UMassCrowdRepresentationSubsystem>(EntitySubsystem.GetWorld());

	// Fallback random if we were unable to set up using the presets
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&, this, RepresentationSubsystem](FMassExecutionContext& Context)
	{
		const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		const TArrayView<FGenericAnimationFragment> AnimationDataList = Context.GetMutableFragmentView<FGenericAnimationFragment>();
		const int32 NumEntities = Context.GetNumEntities();
		for (int32 i = 0; i < NumEntities; ++i)
		{
			ACommanderCharacter* CommanderCharacter = nullptr;
			UAnimToTextureDataAsset* AnimToTextureDataAsset = nullptr;
			TSubclassOf<AActor> TemplateActorClass = RepresentationSubsystem->GetTemplateActorClass(RepresentationList[i].HighResTemplateActorIndex);
			if (TemplateActorClass)
			{
				CommanderCharacter = Cast<ACommanderCharacter>(TemplateActorClass->GetDefaultObject());
				if (CommanderCharacter)
				{
					AnimToTextureDataAsset = CommanderCharacter->AnimToTextureDataAsset.Get();
				}
			}

			if (RepresentationSubsystem)
			{
				if (AnimToTextureDataAsset)
				{
					ensureMsgf(AnimToTextureDataAsset->GetStaticMesh(), TEXT("%s is missing static mesh %s"), *AnimToTextureDataAsset->GetName(), *AnimToTextureDataAsset->StaticMesh.ToString());
					AnimationDataList[i].AnimToTextureData = AnimToTextureDataAsset;
				}
			}
		}
	});
};