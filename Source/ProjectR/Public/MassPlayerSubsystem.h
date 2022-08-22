// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassPlayerSubsystem.generated.h"

class UMassAgentSubsystem;

UCLASS()
class PROJECTR_API UMassPlayerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	AActor* GetActorForEntity(const FMassEntityHandle Entity);

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY()
	UMassAgentSubsystem* AgentSubsystem;

	UPROPERTY()
	TMap<FMassEntityHandle, AActor*> EntityToActorMap;
};
