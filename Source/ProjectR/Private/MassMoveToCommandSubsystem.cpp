// Fill out your copyright notice in the Description page of Project Settings.


#include "MassMoveToCommandSubsystem.h"

#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassComponentHitTypes.h"

const FVector* UMassMoveToCommandSubsystem::GetLastMoveToCommandTarget() const
{
  return MoveToCommandTarget.IsZero() ? nullptr : &MoveToCommandTarget;
}

void UMassMoveToCommandSubsystem::SetMoveToCommandTarget(const FVector target)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("UMassMoveToCommandSubsystem::SetMoveToCommandTarget, num entities: %d"), Entities.Num()));
	MoveToCommandTarget = target;

	// TODO: We are relying on fact that HitReceived triggers a state update in state tree, but this could lead to issues in the future if that behavior changes.
	SignalSubsystem->SignalEntities(UE::Mass::Signals::HitReceived, Entities.Array());
}

void UMassMoveToCommandSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SignalSubsystem = Collection.InitializeDependency<UMassSignalSubsystem>();
	checkfSlow(SignalSubsystem != nullptr, TEXT("MassSignalSubsystem is required"));

	AgentSubsystem = Collection.InitializeDependency<UMassAgentSubsystem>();
	checkfSlow(AgentSubsystem != nullptr, TEXT("MassAgentSubsystem is required"));

	AgentSubsystem->GetOnMassAgentComponentEntityAssociated().AddLambda([this](const UMassAgentComponent& AgentComponent)
		{
			RegisterEntity(AgentComponent.GetEntityHandle());
		});

	AgentSubsystem->GetOnMassAgentComponentEntityDetaching().AddLambda([this](const UMassAgentComponent& AgentComponent)
		{
			UnregisterEntity(AgentComponent.GetEntityHandle());
		});
}

void UMassMoveToCommandSubsystem::RegisterEntity(const FMassEntityHandle Entity)
{
	Entities.Add(Entity);
}

void UMassMoveToCommandSubsystem::UnregisterEntity(const FMassEntityHandle Entity)
{
	Entities.Remove(Entity);
}