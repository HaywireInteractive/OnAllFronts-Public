// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassPlayerSubsystem.h"

#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassSimulationSubsystem.h"

void UMassPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSimulationSubsystem>();

	AgentSubsystem = Collection.InitializeDependency<UMassAgentSubsystem>();
	check(AgentSubsystem);

	AgentSubsystem->GetOnMassAgentComponentEntityAssociated().AddLambda([this](const UMassAgentComponent& AgentComponent)
	{
		EntityToActorMap.Add(AgentComponent.GetEntityHandle(), AgentComponent.GetOwner());
	});

	AgentSubsystem->GetOnMassAgentComponentEntityDetaching().AddLambda([this](const UMassAgentComponent& AgentComponent)
	{
			EntityToActorMap.Remove(AgentComponent.GetEntityHandle());
	});
}

void UMassPlayerSubsystem::Deinitialize()
{
	check(AgentSubsystem);
	AgentSubsystem->GetOnMassAgentComponentEntityAssociated().RemoveAll(this);
	AgentSubsystem->GetOnMassAgentComponentEntityDetaching().RemoveAll(this);

	Super::Deinitialize();
}

AActor* UMassPlayerSubsystem::GetActorForEntity(const FMassEntityHandle Entity)
{
	AActor** resultPtr = EntityToActorMap.Find(Entity);
	if (resultPtr)
	{
		return *resultPtr;
	}	else {
		return nullptr;
	}
}
