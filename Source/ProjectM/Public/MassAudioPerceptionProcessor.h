#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "MassAudioPerceptionProcessor.generated.h"

class UMassSoundPerceptionSubsystem;

struct FSoundTraceData
{
	FSoundTraceData() = default;
	FSoundTraceData(const FMassEntityHandle& Entity, const FVector& TraceStart, const FVector& TraceEnd)
		: Entity(Entity), TraceStart(TraceStart), TraceEnd(TraceEnd)
	{

	}
	FMassEntityHandle Entity;
	FVector TraceStart;
	FVector TraceEnd;
};

UCLASS()
class PROJECTM_API UMassAudioPerceptionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassAudioPerceptionProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery PreLineTracesEntityQuery;
	FMassEntityQuery PostLineTracesEntityQuery;
	TObjectPtr<UMassSoundPerceptionSubsystem> SoundPerceptionSubsystem;
	TQueue<FSoundTraceData, EQueueMode::Mpsc> SoundTraceQueue;
};
