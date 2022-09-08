// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CommanderCharacter.h"
#include "MassMoveToCommandSubsystem.h"
#include "MassFireProjectileTask.h"
#include "MassEntityQuery.h"
#include "MassProjectileDamageProcessor.h"
#include "MassCommonFragments.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassAgentComponent.h"
#include "MassEntityView.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawnLocationProcessor.h"
#include "MassAgentSubsystem.h"
#include <MilitaryStructureSubsystem.h>

//----------------------------------------------------------------------//
//  UMassPlayerControllableCharacterTrait
//----------------------------------------------------------------------//
void UMassPlayerControllableCharacterTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddTag<FMassPlayerControllableCharacterTag>();
}

//----------------------------------------------------------------------//
//  ACommanderCharacter
//----------------------------------------------------------------------//
ACommanderCharacter::ACommanderCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACommanderCharacter::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	check(World);
	MoveToCommandSystem = UWorld::GetSubsystem<UMassMoveToCommandSubsystem>(World);

	UMassAgentSubsystem* AgentSubsystem = UWorld::GetSubsystem<UMassAgentSubsystem>(GetWorld());
	check(AgentSubsystem);

	OnMassAgentComponentEntityAssociatedHandle = AgentSubsystem->GetOnMassAgentComponentEntityAssociated().AddLambda([this, AgentSubsystem](const UMassAgentComponent& AgentComponent)
	{
		if (&AgentComponent == Cast<UMassAgentComponent>(GetComponentByClass(UMassAgentComponent::StaticClass())))
		{
			AgentSubsystem->GetOnMassAgentComponentEntityAssociated().Remove(OnMassAgentComponentEntityAssociatedHandle);
			OnMassAgentComponentEntityAssociatedHandle.Reset();
			InitializeFromMassSoldierInternal();
		}
	});
}

void ACommanderCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnMassAgentComponentEntityAssociatedHandle.IsValid())
	{
		UMassAgentSubsystem* AgentSubsystem = UWorld::GetSubsystem<UMassAgentSubsystem>(GetWorld());
		check(AgentSubsystem);
		AgentSubsystem->GetOnMassAgentComponentEntityAssociated().Remove(OnMassAgentComponentEntityAssociatedHandle);
		OnMassAgentComponentEntityAssociatedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

UMilitaryUnit* ACommanderCharacter::GetMyMilitaryUnit() const
{
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);

	return MilitaryStructureSubsystem->GetUnitForEntity(GetMassEntityHandle());
}

void ACommanderCharacter::SetMoveToCommand(FVector2D CommandLocation) const
{
	UMilitaryUnit* MyMilitaryUnit = GetMyMilitaryUnit();

	if (!MyMilitaryUnit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot find military unit for player when attempting to set move to command."));
		return;
	}

	if (!MyMilitaryUnit->bIsCommander)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot set move to command from soldier who is not a commander."));
		return;
	}

	check(MyMilitaryUnit->Parent);
	// TODO: don't hard-code 20.f below
	MoveToCommandSystem->SetMoveToCommandTarget(MyMilitaryUnit->Parent, FVector(CommandLocation.X, CommandLocation.Y, 20.f), IsPlayerOnTeam1()); 
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

void ACommanderCharacter::ChangePlayerToAISoldier()
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	check(EntitySubsystem);

	const FMassEntityHandle& PlayerEntityHandle = GetMassEntityHandle();
	FMassEntityView PlayerEntityView(*EntitySubsystem, PlayerEntityHandle);
	FTransformFragment* PlayerEntityTransformFragment = PlayerEntityView.GetFragmentDataPtr<FTransformFragment>();
	FMassHealthFragment* PlayerEntityHealthFragment = PlayerEntityView.GetFragmentDataPtr<FMassHealthFragment>();

	check(PlayerEntityTransformFragment);
	check(PlayerEntityHealthFragment);

	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	check(SpawnerSystem);

	// TODO: A bit hacky to get first actor here.
	const FMassEntityTemplate* EntityTemplate = SoldierEntityConfig.GetOrCreateEntityTemplate(*World->GetLevel(0)->Actors[0], *SpawnerSystem); // TODO: passing SpawnerSystem is a hack
	check(EntityTemplate->IsValid());

	FMassEntitySpawnDataGeneratorResult Result;
	Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
	Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
	Result.NumEntities = 1;
	FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

	Transforms.Transforms.Reserve(1);
	FTransform& SpawnDataTransform = Transforms.Transforms.AddDefaulted_GetRef();
	SpawnDataTransform = PlayerEntityTransformFragment->GetTransform();

	TArray<FMassEntityHandle> SpawnedEntities;
	SpawnerSystem->SpawnEntities(EntityTemplate->GetTemplateID(), Result.NumEntities, Result.SpawnData, Result.SpawnDataProcessor, SpawnedEntities);

	FMassHealthFragment* SpawnedEntityHealthFragment = EntitySubsystem->GetFragmentDataPtr<FMassHealthFragment>(SpawnedEntities[0]);
	check(SpawnedEntityHealthFragment);
	SpawnedEntityHealthFragment->Value = PlayerEntityHealthFragment->Value;

	UMilitaryUnit* MyMilitaryUnit = GetMyMilitaryUnit();
	MyMilitaryUnit->bIsPlayer = false;

	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);
	MilitaryStructureSubsystem->BindUnitToMassEntity(MyMilitaryUnit, SpawnedEntities[0]);
}

void ACommanderCharacter::DidDie_Implementation()
{
}

bool ACommanderCharacter::InitializeFromMassSoldier(const int32 MassEntityIndex, const int32 MassEntitySerialNumber)
{
	FMassEntityHandle MassSoldierEntity(MassEntityIndex, MassEntitySerialNumber);

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	check(EntitySubsystem);

	if (!EntitySubsystem->IsEntityValid(MassSoldierEntity))
	{
		return false;
	}

	this->MassSoldierEntityToInitializeWith = MassSoldierEntity;
	return true;
}

void ACommanderCharacter::InitializeFromMassSoldierInternal()
{
	UMilitaryStructureSubsystem* MilitaryStructureSubsystem = UWorld::GetSubsystem<UMilitaryStructureSubsystem>(GetWorld());
	check(MilitaryStructureSubsystem);

	// If we don't have a soldier entity to initialize with, set it to team's highest commander.
	if (!MassSoldierEntityToInitializeWith.IsSet())
	{
		UMilitaryUnit* RootUnit = MilitaryStructureSubsystem->GetRootUnitForTeam(IsPlayerOnTeam1());
		if (!RootUnit)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot find root military unit for player's team on character initialization."));
			return;
		}

		UMilitaryUnit* TeamCommander = RootUnit->Commander;

		if (!TeamCommander)
		{
			UE_LOG(LogTemp, Warning, TEXT("Cannot find military unit for team commander on character initialization."));
			return;
		}
		MassSoldierEntityToInitializeWith = TeamCommander->GetMassEntityHandle();
	}

	if (!MassSoldierEntityToInitializeWith.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot find military unit for player on character initialization."));
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	check(EntitySubsystem);

	const FMassHealthFragment& SoldierHealthFragment = EntitySubsystem->GetFragmentDataChecked<FMassHealthFragment>(MassSoldierEntityToInitializeWith);
	int16 NewPlayerHealth = SoldierHealthFragment.Value;

	const FTransformFragment& SoldierTransformFragment = EntitySubsystem->GetFragmentDataChecked<FTransformFragment>(MassSoldierEntityToInitializeWith);
	FTransform NewPlayerTransform = SoldierTransformFragment.GetTransform();

	FMassEntityHandle PlayerEntityHandle = GetMassEntityHandle();

	UMilitaryUnit* SoldierMilitaryUnit = MilitaryStructureSubsystem->GetUnitForEntity(MassSoldierEntityToInitializeWith);
	SoldierMilitaryUnit->bIsPlayer = true;
	MilitaryStructureSubsystem->BindUnitToMassEntity(SoldierMilitaryUnit, PlayerEntityHandle);
	EntitySubsystem->DestroyEntity(MassSoldierEntityToInitializeWith);

	FMassHealthFragment* PlayerEntityHealthFragment = EntitySubsystem->GetFragmentDataPtr<FMassHealthFragment>(PlayerEntityHandle);
	check(PlayerEntityHealthFragment);
	
	PlayerEntityHealthFragment->Value = NewPlayerHealth;
	NewPlayerTransform.SetLocation(NewPlayerTransform.GetLocation() + FVector(0.f, 0.f, RootComponent->Bounds.BoxExtent.Z));
	SetActorTransform(NewPlayerTransform);
}

bool ACommanderCharacter::IsPlayerOnTeam1() const
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld());
	check(EntitySubsystem);

	FMassEntityView PlayerEntityView(*EntitySubsystem, GetMassEntityHandle());

	FTeamMemberFragment* PlayerEntityTeamMemberFragment = PlayerEntityView.GetFragmentDataPtr<FTeamMemberFragment>();
	check(PlayerEntityTeamMemberFragment);

	return PlayerEntityTeamMemberFragment->IsOnTeam1;
}

FMassEntityHandle ACommanderCharacter::GetMassEntityHandle() const
{
	UMassAgentComponent* AgentComponent = Cast<UMassAgentComponent>(GetComponentByClass(UMassAgentComponent::StaticClass()));
	check(AgentComponent);

	const FMassEntityHandle& AgentEntityHandle = AgentComponent->GetEntityHandle();
	check(AgentEntityHandle.IsValid());

	return AgentEntityHandle;
}

bool ACommanderCharacter::IsCommander() const
{
	UMilitaryUnit* MyMilitaryUnit = GetMyMilitaryUnit();

	if (!MyMilitaryUnit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot find military unit for player when attempting to check if commander."));
		return false;
	}

	return MyMilitaryUnit->bIsCommander;
}
