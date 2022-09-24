// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassRifle.h"
#include "MassFireProjectileTask.h"
#include "MassEnemyTargetFinderProcessor.h"

AMassRifle::AMassRifle()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMassRifle::SpawnProjectile(const FTransform SpawnTransform, const bool bIsPlayerTeam1) const
{
	const UWorld* World = GetWorld();
	const FVector InitialVelocity = SpawnTransform.GetRotation().Vector() * GetProjectileInitialXYVelocityMagnitude(true);
	::SpawnProjectile(World, SpawnTransform.GetLocation(), SpawnTransform.GetRotation(), InitialVelocity, ProjectileEntityConfig, bIsPlayerTeam1);
}
