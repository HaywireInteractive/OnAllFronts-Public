#pragma once

#include "MassEntityTypes.h"
#include "HierarchicalHashGrid2D.h"
#include "Subsystems/WorldSubsystem.h"

#include "MassSoundPerceptionSubsystem.generated.h"

// TODO: values of 2,4 here may not be optimal for performance.
typedef THierarchicalHashGrid2D<2, 4> FSoundPerceptionHashGrid2D;	// 2 levels of hierarchy, 4 ratio between levels

struct FMassSoundPerceptionItemMetaData
{
	FMassSoundPerceptionItemMetaData(float InTimeLeftTilDestruction, const FSoundPerceptionHashGrid2D::FCellLocation InCellLocation, const FVector InSoundSource)
		: TimeLeftTilDestruction(InTimeLeftTilDestruction), CellLocation(InCellLocation), SoundSource(InSoundSource) {}
	float TimeLeftTilDestruction;
	const FSoundPerceptionHashGrid2D::FCellLocation CellLocation;
	const FVector SoundSource;
};

UCLASS()
class PROJECTM_API UMassSoundPerceptionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UMassSoundPerceptionSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void AddSoundPerception(FVector Location, const bool& bIsSourceFromTeam1);
	bool HasSoundAtLocation(FVector Location, FVector& OutSoundSource, const bool& bFilterToTeam1);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	FSoundPerceptionHashGrid2D SoundPerceptionGridForTeam1;
	FSoundPerceptionHashGrid2D SoundPerceptionGridForTeam2;
	TMap<uint32, FMassSoundPerceptionItemMetaData> IdsToMetaDataForTeam1;
	TMap<uint32, FMassSoundPerceptionItemMetaData> IdsToMetaDataForTeam2;
};
