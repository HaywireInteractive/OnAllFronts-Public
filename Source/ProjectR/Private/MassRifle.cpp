// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassRifle.h"
#include "MassFireProjectileTask.h"

AMassRifle::AMassRifle()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMassRifle::SpawnProjectile(const FTransform SpawnTransform) const
{
	// TODO: don't hard-code
	static const float InitialVelocityMagnitude = 4000.0f;

	const UWorld* World = GetWorld();
	const FVector InitialVelocity = SpawnTransform.GetRotation().Vector() * InitialVelocityMagnitude;
	::SpawnProjectile(World, SpawnTransform.GetLocation(), SpawnTransform.GetRotation(), InitialVelocity, ProjectileEntityConfig);
}
