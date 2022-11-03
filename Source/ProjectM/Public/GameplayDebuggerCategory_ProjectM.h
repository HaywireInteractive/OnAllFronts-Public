// Adapted from GameplayDebuggerCategory_Mass.h.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class APlayerController;
class AActor;
struct FMassNavMeshMoveFragment;

class FGameplayDebuggerCategory_ProjectM : public FGameplayDebuggerCategory
{
public:
  FGameplayDebuggerCategory_ProjectM();
  void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
  void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

  static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	void DrawTargetEntityLocations(const TArray<FVector>& TargetEntityLocations, const FColor& Color, const FVector& EntityLocation);
	void CollectDataForNavMeshMoveProcessor(const APlayerController* OwnerPC, const FVector& ViewLocation, const FVector& ViewDirection);
	void DrawEntityInfo(const FMassNavMeshMoveFragment& NavMeshMoveFragment, const FTransform& Transform, const float MinViewDirDot, const FVector& ViewLocation, const FVector& ViewDirection, const float MaxViewDistance, const struct FMassMoveTargetFragment& MoveTargetFragment, const float AgentRadius, const FMassEntityHandle& Entity, const UWorld* World);

	struct FEntityDescription
	{
		FEntityDescription() = default;
		FEntityDescription(const float InScore, const FVector& InLocation, const FString& InDescription) : Score(InScore), Location(InLocation), Description(InDescription) {}

		float Score = 0.0f;
		FVector Location = FVector::ZeroVector;
		FString Description;
	};
	TArray<FEntityDescription> NearEntityDescriptions;
};

#endif // WITH_GAMEPLAY_DEBUGGER