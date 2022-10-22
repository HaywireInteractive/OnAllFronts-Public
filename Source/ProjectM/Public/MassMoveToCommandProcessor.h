// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassNavigationFragments.h"
#include "MassEntityTraitBase.h"
#include <MilitaryStructureSubsystem.h>

#include "MassMoveToCommandProcessor.generated.h"

class UMassMoveToCommandSubsystem;

bool IsSquadMember(const UMilitaryUnit* MilitaryUnit);

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

struct FNavigationAction
{
	FNavigationAction(const FVector TargetLocation, const FVector Forward, const EMassMovementAction Action = EMassMovementAction::Move, const bool bShouldFollowLeader = false)
		: TargetLocation(TargetLocation), Forward(Forward), Action(Action), bShouldFollowLeader(bShouldFollowLeader)
	{
	}
	FVector TargetLocation;
	FVector Forward;
	EMassMovementAction Action;
	bool bShouldFollowLeader;
};

struct FNavigationActionList
{
	FNavigationActionList(const TArray<FNavigationAction> Actions)
		: Actions(Actions)
	{
	}
	FNavigationActionList() = default;
	const TArray<FNavigationAction> Actions;
};

typedef TSharedPtr<FNavigationActionList, ESPMode::ThreadSafe> FNavActionListSharedPtr;

USTRUCT()
struct PROJECTM_API FMassNavMeshMoveFragment : public FMassFragment
{
	GENERATED_BODY()

	bool IsSquadMember()
	{
		return SquadMemberIndex >= 0;
	}

	void Reset()
	{
		ActionList = MakeShareable(new FNavigationActionList());
		CurrentActionIndex = 0;
		ActionsRemaining = -1;
		SquadMemberIndex = -1;
		bIsWaitingOnSquadMates = false;
	}

	FNavActionListSharedPtr ActionList;
	int32 CurrentActionIndex = 0; // This gets incremented when completing the next action AND all squad members have completed that action as well.
	int32 ActionsRemaining = -1; // This gets decremented when completing the next action BEFORE all squad members have completed that action as well.
	int8 SquadMemberIndex = -1;
	bool bIsWaitingOnSquadMates = false;
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
