#include "GameplayDebuggerCategory_ProjectM.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "MassAudioPerceptionProcessor.h"
#include <MassEnemyTargetFinderProcessor.h>

FGameplayDebuggerCategory_ProjectM::FGameplayDebuggerCategory_ProjectM()
{
  bShowOnlyWithDebugActor = false;
}

void FGameplayDebuggerCategory_ProjectM::DrawTargetEntityLocations(const TArray<FVector>& TargetEntityLocations, const FColor& Color, const FVector& EntityLocation)
{
	const FVector ZOffset(0.f, 0.f, 200.f);
	for (const FVector& TargetEntityLocation : TargetEntityLocations)
	{
		AddShape(FGameplayDebuggerShape::MakeArrow(EntityLocation + ZOffset, TargetEntityLocation + ZOffset, 10.0f, 2.0f, Color));
		AddShape(FGameplayDebuggerShape::MakeCylinder(TargetEntityLocation + ZOffset / 2.f, 50.f, 100.0f, Color));
	}
}

void FGameplayDebuggerCategory_ProjectM::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
  if (!OwnerPC)
  {
		return;
  }

	const FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (!DebugEntityData.IsEntitySearching)
	{
		return;
	}

	AddShape(FGameplayDebuggerShape::MakeBox(DebugEntityData.SearchCenter, DebugEntityData.SearchExtent, FColor::Purple));

	DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToSameTeam, FColor::Red, DebugEntityData.EntityLocation);
	DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToOtherEntityBlocking, FColor::Yellow, DebugEntityData.EntityLocation);
	DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToImpenetrable, FColor::Orange, DebugEntityData.EntityLocation);
	DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToOutOfRange, FColor::Blue, DebugEntityData.EntityLocation);
	DrawTargetEntityLocations(DebugEntityData.TargetEntitiesCulledDueToNoLineOfSight, FColor::Black, DebugEntityData.EntityLocation);

	if (DebugEntityData.HasTargetEntity)
	{
		TArray<FVector> TargetEntityLocationArray;
		TargetEntityLocationArray.Add(DebugEntityData.TargetEntityLocation);
		DrawTargetEntityLocations(TargetEntityLocationArray, FColor::Green, DebugEntityData.EntityLocation);
	}
}

void FGameplayDebuggerCategory_ProjectM::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	const FDebugEntityData& DebugEntityData = UMassEnemyTargetFinderProcessor_DebugEntityData;
	if (!DebugEntityData.IsEntitySearching)
	{
		return;
	}

	const FVector2D EntityScreenLocation = CanvasContext.ProjectLocation(DebugEntityData.EntityLocation);
	CanvasContext.PrintAt(EntityScreenLocation.X, EntityScreenLocation.Y, FColor::Purple, 1.f, FString::Printf(TEXT("{Purple}Number of close entities: %d"), DebugEntityData.NumCloseEntities));

	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ProjectM::MakeInstance()
{
  return MakeShareable(new FGameplayDebuggerCategory_ProjectM());
}

#endif // WITH_GAMEPLAY_DEBUGGER