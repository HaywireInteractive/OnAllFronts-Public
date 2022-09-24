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
	FCapsule(FVector InA, FVector InB, float InR) : a(InA), b(InB), r(InR) {}
	FCapsule() {}
	FVector a; // One edge of Capsule.
	FVector b; // Other edge of Capsule.
	float r; // Radius
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
FCapsule MakeCapsuleForEntity(const FMassEntityView& EntityView);

// Returns true if capsules collide.
bool TestCapsuleCapsule(FCapsule capsule1, FCapsule capsule2);

void DrawCapsule(const FCapsule& Capsule, const UWorld& World, const FLinearColor& Color = FLinearColor::Red, const bool bPersistentLines = true, float LifeTime = -1.f);

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
