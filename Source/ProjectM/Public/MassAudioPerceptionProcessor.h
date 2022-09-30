#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "MassAudioPerceptionProcessor.generated.h"

class UMassSoundPerceptionSubsystem;

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
	FMassEntityQuery EntityQuery;
	TObjectPtr<UMassSoundPerceptionSubsystem> SoundPerceptionSubsystem;
};
