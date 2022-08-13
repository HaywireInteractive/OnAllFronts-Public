// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CommanderCharacter.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassFireProjectileTask.h"

// Sets default values
ACommanderCharacter::ACommanderCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACommanderCharacter::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	check(World);
	MoveToCommandSystem = UWorld::GetSubsystem<UMassMoveToCommandSubsystem>(World);
}

// Called every frame
void ACommanderCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ACommanderCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ACommanderCharacter::SetMoveToCommand() const
{
	MoveToCommandSystem->SetMoveToCommandTarget(FVector(0.f, 0.f, 20.f), false); // TODO
}

void ACommanderCharacter::SpawnProjectile() const
{
	// TODO: don't hard-code
	static const float ForwardVectorMagnitude = 300.f;
	static const FVector ProjectileLocationOffset = FVector(0.f, 0.f, 150.f);
	static const float InitialVelocityMagnitude = 4000.0f;

	const UWorld* World = GetWorld();
	const FVector& ActorForward = GetActorForwardVector();
	const FVector ActorFeetLocation = GetActorLocation() - FVector(0.f, 0.f, GetRootComponent()->Bounds.BoxExtent.Z);
	const FVector SpawnLocation = ActorFeetLocation + ActorForward * ForwardVectorMagnitude + ProjectileLocationOffset;
	const FVector InitialVelocity = ActorForward * InitialVelocityMagnitude;
	::SpawnProjectile(World, SpawnLocation, GetActorQuat(), InitialVelocity, ProjectileEntityConfig);
}