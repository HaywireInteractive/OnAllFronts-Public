// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "MassStateTreeTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassFireProjectileTask.generated.h"

class UMassSignalSubsystem;
struct FTransformFragment;
struct FTeamMemberFragment;

void SpawnProjectile(const UWorld* World, const FVector& SpawnLocation, const FQuat& SpawnRotation, const FVector& InitialVelocity, const FMassEntityConfig& EntityConfig, const bool& bIsProjectileFromTeam1);

USTRUCT()
struct PROJECTM_API FMassFireProjectileTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassEntityConfig EntityConfig;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float InitialVelocity = 100.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float ForwardVectorMagnitude = 100.f;

	// TODO: DRY this somehow. Soldier/vehicle Entity's weapon should have info where it should spawn from instead of needing this.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool IsFromSoldier = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float WeaponCoolDownSeconds = 1.f;

	UPROPERTY(VisibleAnywhere, Category = Parameter)
	float LastWeaponFireTimeSeconds = -1.f;
};

USTRUCT(meta = (DisplayName = "Fire Projectile"))
struct PROJECTM_API FMassFireProjectileTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FMassFireProjectileTaskInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const EStateTreeStateChangeType ChangeType, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FTransformFragment> EntityTransformHandle;
	TStateTreeExternalDataHandle<FTeamMemberFragment> TeamMemberHandle;

	TStateTreeInstanceDataPropertyHandle<FMassEntityConfig> EntityConfigHandle;
	TStateTreeInstanceDataPropertyHandle<float> InitialVelocityHandle;
	TStateTreeInstanceDataPropertyHandle<float> ForwardVectorMagnitudeHandle;
	TStateTreeInstanceDataPropertyHandle<bool> IsFromSoldierHandle;
	TStateTreeInstanceDataPropertyHandle<float> WeaponCoolDownSecondsHandle;
	TStateTreeInstanceDataPropertyHandle<float> LastWeaponFireTimeSecondsHandle;
};
