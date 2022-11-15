// Mostly a copy of City Sample MassCrowdAnimationProcessor.h

#pragma once

#include "MassObserverProcessor.h"
#include "MassRepresentationTypes.h"
#include "LightweightMontageInstance.h"
#include "ContextualAnimSceneAsset.h"

#include "MassGenericAnimationProcessor.generated.h"

class UAnimToTextureDataAsset;
struct FMassActorFragment;

namespace UE::CrowdInteractionAnim
{
	struct FRequest
	{
		TWeakObjectPtr<class UContextualAnimSceneAsset> ContextualAnimAsset = nullptr;
		FContextualAnimQueryResult QueryResult = FContextualAnimQueryResult();
		FName InteractorRole;
		FName AlignmentTrack;
	};

	struct FMotionWarpingScratch
	{
		float TimeRemaining = -1.0f;
		float Duration = -1.0f;
		FVector InitialLocation = FVector::ZeroVector;
		FQuat InitialRotation = FQuat::Identity;
	};
} // namespace UE::CrowdInteractionAnim

USTRUCT()
struct PROJECTM_API FMassGenericMontageFragment : public FMassFragment
{
	GENERATED_BODY()

	UE::VertexAnimation::FLightweightMontageInstance MontageInstance = UE::VertexAnimation::FLightweightMontageInstance();
	UE::CrowdInteractionAnim::FRequest InteractionRequest = UE::CrowdInteractionAnim::FRequest();
	UE::CrowdInteractionAnim::FMotionWarpingScratch MotionWarpingScratch = UE::CrowdInteractionAnim::FMotionWarpingScratch();
	FRootMotionMovementParams RootMotionParams = FRootMotionMovementParams();
	float SkippedTime = 0.0f;

	void Request(const UE::CrowdInteractionAnim::FRequest& InRequest);
	void Clear();
};

USTRUCT()
struct PROJECTM_API FGenericAnimationFragment : public FMassFragment
{
	GENERATED_BODY()

	TWeakObjectPtr<UAnimToTextureDataAsset> AnimToTextureData;
	float GlobalStartTime = 0.0f;
	float PlayRate = 1.0f;
	int32 AnimationStateIndex = 0;
	bool bSwappedThisFrame = false;
};

UCLASS()
class PROJECTM_API UMassGenericAnimationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassGenericAnimationProcessor();

	UPROPERTY(EditAnywhere, Category="Animation", meta=(ClampMin=0.0, UIMin=0.0))
	float MoveThresholdSq = 750.0f;

private:
	void UpdateAnimationFragmentData(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context, float GlobalTime, TArray<FMassEntityHandle, TInlineAllocator<32>>& ActorEntities);
	void UpdateVertexAnimationState(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context, float GlobalTime);
	void UpdateSkeletalAnimation(UMassEntitySubsystem& EntitySubsystem, float GlobalTime, TArrayView<FMassEntityHandle> ActorEntities);

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

	static class UAnimInstance* GetAnimInstanceFromActor(const class AActor* Actor);

	UPROPERTY(Transient)
	UWorld* World = nullptr;

	FMassEntityQuery AnimationEntityQuery_Conditional;
	FMassEntityQuery MontageEntityQuery;
	FMassEntityQuery MontageEntityQuery_Conditional;
};

// Adapted from CitySample UCitySampleCrowdVisualizationFragmentInitializer.
UCLASS()
class PROJECTM_API UGenericAnimationFragmentInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UGenericAnimationFragmentInitializer();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

protected:
	FMassEntityQuery EntityQuery;
	UWorld* World = nullptr;
};