// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavigationFragments.h"
#include "MassEntityTraitBase.h"
#include <MilitaryStructureSubsystem.h>

#include "MassMoveToCommandProcessor.generated.h"

class UMassMoveToCommandSubsystem;

USTRUCT()
struct FMassHasStashedMoveTargetTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassCommandableTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassFollowLeaderTag : public FMassTag
{
	GENERATED_BODY()
};

// This fragment should not be shared across entities because movement speed may get affected by damage in the future.
USTRUCT()
struct PROJECTM_API FMassCommandableMovementSpeedFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Category = "", EditAnywhere)
	float MovementSpeed;
};

USTRUCT()
struct PROJECTM_API FNavMeshParamsFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement")
	float NavMeshRadius = 50.f;
};

UCLASS(meta = (DisplayName = "Commandable"))
class PROJECTM_API UMassCommandableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "", EditAnywhere)
	float MovementSpeed = 500.f;

	UPROPERTY(Category = "", EditAnywhere)
	FNavMeshParamsFragment NavMeshParams;
};

USTRUCT()
struct PROJECTM_API FMassStashedMoveTargetFragment : public FMassMoveTargetFragment
{
	GENERATED_BODY()
};

struct FSquadMemberFollowData
{
	FMassEntityHandle Entity;
	uint8 SquadMemberIndex; // Index into GSquadMemberOffsetsMeters
};

USTRUCT()
struct PROJECTM_API FMassNavMeshMoveFragment : public FMassFragment
{
	GENERATED_BODY()

	FNavPathSharedPtr Path;
	int32 CurrentPathPointIndex = 0;
	TStaticArray<FSquadMemberFollowData, GNumSoldiersInSquad> SquadMembersFollowData;
	bool HasFollowData = false;
};

UCLASS()
class PROJECTM_API UMassMoveToCommandProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UMassMoveToCommandProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassMoveToCommandSubsystem> MoveToCommandSubsystem;
	FMassEntityQuery EntityQuery;
};
