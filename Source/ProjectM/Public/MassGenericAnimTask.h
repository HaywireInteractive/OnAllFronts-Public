// Mostly a copy of City Sample MassContextualAnimTask.h

#pragma once

#include "MassEntityTypes.h"
#include "MassStateTreeTypes.h"
#include "MassGenericAnimTask.generated.h"

class UAnimMontage;

class UMassSignalSubsystem;
struct FMassGenericMontageFragment; 
struct FTransformFragment;
struct FMassMoveTargetFragment;

USTRUCT()
struct FMassGenericAnimTaskInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category = Input, meta = (Optional))
	FMassEntityHandle TargetEntity;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0.0f;

	UPROPERTY()
	float ComputedDuration = 0.0f;

	/** Accumulated time used to stop task if a montage is set */
	UPROPERTY()
	float Time = 0.f;
};

USTRUCT(meta = (DisplayName = "Mass Generic Anim Task"))
struct FMassGenericAnimTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	FMassGenericAnimTask();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassGenericAnimTaskInstanceData::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FMassGenericMontageFragment, EStateTreeExternalDataRequirement::Optional> MontageRequestHandle; 
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
	TStateTreeExternalDataHandle<FMassMoveTargetFragment> MoveTargetHandle;
	
	TStateTreeInstanceDataPropertyHandle<FMassEntityHandle> TargetEntityHandle;
	TStateTreeInstanceDataPropertyHandle<float> DurationHandle;
	TStateTreeInstanceDataPropertyHandle<float> ComputedDurationHandle;
	TStateTreeInstanceDataPropertyHandle<float> TimeHandle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	class UContextualAnimSceneAsset* ContextualAnimAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FName AlignmentTrack;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FName InteractorRole;

	UPROPERTY(EditAnywhere, Category = Parameter)
	UAnimMontage* FallbackMontage = nullptr;
};
