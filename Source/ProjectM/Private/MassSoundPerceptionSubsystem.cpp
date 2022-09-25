#include "MassSoundPerceptionSubsystem.h"

#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"

static int32 GMassSoundPerceptionSubsystemCounter = 0;
static const float GSecondsTilSoundPerceptionDestruction = 2.f;
static const float GUMassSoundPerceptionSubsystem_GridCellSize = 100000.f; // TODO: value here may not be optimal for performance.

UMassSoundPerceptionSubsystem::UMassSoundPerceptionSubsystem()
	: SoundPerceptionGridForTeam1(GUMassSoundPerceptionSubsystem_GridCellSize), SoundPerceptionGridForTeam2(GUMassSoundPerceptionSubsystem_GridCellSize)
{
}

void UMassSoundPerceptionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassSimulationSubsystem>();
}

void UMassSoundPerceptionSubsystem::Tick(float DeltaTime)
{
	TArray<uint32> ItemsToRemove;
	for (const auto& Item : SoundPerceptionGridForTeam1.GetItems())
	{
		float& TimeLeftTilDestruction = IdsToMetaDataForTeam1[Item.ID].TimeLeftTilDestruction;
		TimeLeftTilDestruction -= DeltaTime;
		if (TimeLeftTilDestruction <= 0.f)
		{
			ItemsToRemove.Add(Item.ID);
		}
	}
	for (const int32& ItemID : ItemsToRemove)
	{
		SoundPerceptionGridForTeam1.Remove(ItemID, IdsToMetaDataForTeam1[ItemID].CellLocation);
		IdsToMetaDataForTeam1.Remove(ItemID);
	}

	ItemsToRemove.Reset();

	for (const auto& Item : SoundPerceptionGridForTeam2.GetItems())
	{
		float& TimeLeftTilDestruction = IdsToMetaDataForTeam2[Item.ID].TimeLeftTilDestruction;
		TimeLeftTilDestruction -= DeltaTime;
		if (TimeLeftTilDestruction <= 0.f)
		{
			ItemsToRemove.Add(Item.ID);
		}
	}
	for (const int32& ItemID : ItemsToRemove)
	{
		SoundPerceptionGridForTeam2.Remove(ItemID, IdsToMetaDataForTeam2[ItemID].CellLocation);
		IdsToMetaDataForTeam2.Remove(ItemID);
	}
}

bool UMassSoundPerceptionSubsystem_DrawOnAddSoundPerception = false;
FAutoConsoleVariableRef CVarUMassSoundPerceptionSubsystem_DrawOnAddSoundPerception(TEXT("pm.UMassSoundPerceptionSubsystem_DrawOnAddSoundPerception"), UMassSoundPerceptionSubsystem_DrawOnAddSoundPerception, TEXT("UMassSoundPerceptionSubsystem: Draw On AddSoundPerception"));

void UMassSoundPerceptionSubsystem::AddSoundPerception(FVector Location, const bool& bIsSourceFromTeam1, const bool SkipDebugDraw)
{
	auto& SoundPerceptionGrid = bIsSourceFromTeam1 ? SoundPerceptionGridForTeam1 : SoundPerceptionGridForTeam2;
	uint32 ItemID = GMassSoundPerceptionSubsystemCounter++;
	static const float Extent = 3.f;
	const FBox Bounds(Location - FVector(Extent, Extent, 0.f), Location + FVector(Extent, Extent, 0.f));
	const FSoundPerceptionHashGrid2D::FCellLocation& CellLocation = SoundPerceptionGrid.Add(ItemID, Bounds);


	FMassSoundPerceptionItemMetaData ItemMetaData(GSecondsTilSoundPerceptionDestruction, CellLocation, Location);
	auto& IdsToMetaData = bIsSourceFromTeam1 ? IdsToMetaDataForTeam1 : IdsToMetaDataForTeam2;
	IdsToMetaData.Add(ItemID, ItemMetaData);

	if (UMassSoundPerceptionSubsystem_DrawOnAddSoundPerception && !SkipDebugDraw)
	{
		::DrawDebugSphere(GetWorld(), Location, 200.f, 10, bIsSourceFromTeam1 ? FColor::Red : FColor::Blue, false, GSecondsTilSoundPerceptionDestruction);
	}
}

// TODO: This isn't on by default yet because it makes the AI do pretty dumb stuff like turn away from enemies if there's an explosion behind them. Need to improve.
bool UMassSoundPerceptionSubsystem_AddEnvironmentImpactSounds = false;
FAutoConsoleVariableRef CVar_UMassSoundPerceptionSubsystem_AddEnvironmentImpactSounds(TEXT("pm.UMassSoundPerceptionSubsystem_AddEnvironmentImpactSounds"), UMassSoundPerceptionSubsystem_AddEnvironmentImpactSounds, TEXT("UMassSoundPerceptionSubsystem_AddEnvironmentImpactSounds"));

void UMassSoundPerceptionSubsystem::AddSoundPerception(FVector Location)
{
	if (!UMassSoundPerceptionSubsystem_AddEnvironmentImpactSounds)
	{
		return;
	}

	AddSoundPerception(Location, true, true);
	AddSoundPerception(Location, false, true);

	if (UMassSoundPerceptionSubsystem_DrawOnAddSoundPerception)
	{
		::DrawDebugSphere(GetWorld(), Location, 200.f, 10, FColor::White, false, GSecondsTilSoundPerceptionDestruction);
	}
}

TStatId UMassSoundPerceptionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMassSoundPerceptionSubsystem, STATGROUP_Tickables);
}

bool UMassSoundPerceptionSubsystem::HasSoundAtLocation(FVector Location, FVector& OutSoundSource, const bool& bFilterToTeam1)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassSoundPerceptionSubsystem_HasSoundAtLocation);

	auto& SoundPerceptionGrid = bFilterToTeam1 ? SoundPerceptionGridForTeam1 : SoundPerceptionGridForTeam2;
	TArray<FSoundPerceptionHashGrid2D::ItemIDType> NearbySounds;
	static const float QueryRadius = GUMassSoundPerceptionSubsystem_GridCellSize / 2.f;
	const FVector Extent(QueryRadius, QueryRadius, QueryRadius);
	const FBox QueryBox = FBox(Location - Extent, Location + Extent);

	SoundPerceptionGrid.Query(QueryBox, NearbySounds);

	if (NearbySounds.Num() > 0)
	{
		auto& IdsToMetaData = bFilterToTeam1 ? IdsToMetaDataForTeam1 : IdsToMetaDataForTeam2;
		// TODO: OK to always pick first sounds here? No guarantee it's the closest sound.
		OutSoundSource = IdsToMetaData[NearbySounds[0]].SoundSource;
	}

	return NearbySounds.Num() > 0;
}
