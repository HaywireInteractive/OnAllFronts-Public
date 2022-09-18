// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityTypes.h"
#include "MassNavigationSubsystem.h"

#include "MassCollisionProcessor.generated.h"

USTRUCT()
struct FMassCollisionTag : public FMassTag
{
	GENERATED_BODY()
};

struct FCapsule
{
	FVector a;
	FVector b;
	float r;
};

USTRUCT()
struct PROJECTM_API FCollisionCapsuleParametersFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Category = "", EditAnywhere)
	bool bIsCapsuleAlongForwardVector;

	UPROPERTY(Category = "", EditAnywhere)
	float CapsuleRadius;

	UPROPERTY(Category = "", EditAnywhere)
	float CapsuleLength;

	UPROPERTY(Category = "", EditAnywhere)
	FVector CapsuleCenterOffset;
};

FCapsule MakeCapsuleForEntity(const FCollisionCapsuleParametersFragment& CollisionCapsuleParametersFragment, const FTransform& EntityTransform);

// Returns true if capsules collide.
bool TestCapsuleCapsule(FCapsule capsule1, FCapsule capsule2);

void DrawCapsule(const FCapsule& Capsule, const UWorld& World, const FLinearColor& Color = FLinearColor::Red);

UCLASS(meta = (DisplayName = "Collision"))
class PROJECTM_API UMassCollisionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "", EditAnywhere)
	bool bIsCapsuleAlongForwardVector;

	UPROPERTY(Category = "", EditAnywhere)
	float CapsuleRadius;

	UPROPERTY(Category = "", EditAnywhere)
	float CapsuleLength;

	UPROPERTY(Category = "", EditAnywhere)
	FVector CapsuleCenterOffset;

	UPROPERTY(Category = "", EditAnywhere)
	bool bEnableCollisionProcessor;
};

UCLASS()
class PROJECTM_API UMassCollisionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassCollisionProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};
