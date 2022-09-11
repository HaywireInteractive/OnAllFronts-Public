#pragma once

#include "MassProcessor.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassEntityTypes.h"
#include "MassNavigationSubsystem.h"
#include "MassProjectileDamageProcessor.generated.h"

class UMassNavigationSubsystem;

USTRUCT()
struct FMassProjectileWithDamageTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassProjectileDamagableSoldierTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct PROJECTM_API FMassPreviousLocationFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	FVector Location = FVector::ZeroVector;
};

USTRUCT()
struct PROJECTM_API FMassHealthFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	int16 Value = 100;
};

USTRUCT()
struct PROJECTM_API FProjectileDamageFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	int16 DamagePerHit = 10;
	
	UPROPERTY(EditAnywhere, Category = "")
	float Caliber;
};

USTRUCT()
struct PROJECTM_API FProjectileDamagableFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "")
	float MinCaliberForDamage;
};

USTRUCT()
struct PROJECTM_API FMinZParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement")
	float Value = 0.f;
};

USTRUCT()
struct PROJECTM_API FDebugParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool DrawLineTraces = false;
};

UCLASS(meta = (DisplayName = "ProjectileWithDamage"))
class PROJECTM_API UMassProjectileWithDamageTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "Damage", EditAnywhere)
	int16 DamagePerHit = 10;

	UPROPERTY(Category = "Damage", EditAnywhere)
	float Caliber = 5.0f;

	FMassMovementParameters Movement;

	UPROPERTY(Category = "Movement", EditAnywhere)
	float GravityMagnitude = 0.f;

	/** Minimum Z value for projectiles at which they get destroyed */
	UPROPERTY(Category = "Movement", EditAnywhere)
	FMinZParameters MinZ;

	UPROPERTY(Category = "Debug", EditAnywhere)
	FDebugParameters DebugParameters;
};

UCLASS(meta = (DisplayName = "ProjectileDamagable"))
class PROJECTM_API UMassProjectileDamagableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const override;

	UPROPERTY(Category = "", EditAnywhere)
	bool bIsSoldier = true;

	UPROPERTY(Category = "", EditAnywhere)
	float MinCaliberForDamage = 5.0f;
};

UCLASS()
class PROJECTM_API UMassProjectileDamageProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassProjectileDamageProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context) override;

private:
	TObjectPtr<UMassNavigationSubsystem> NavigationSubsystem;
	FMassEntityQuery EntityQuery;
};
