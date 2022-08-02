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
struct FMassProjectileDamagableTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct PROJECTR_API FMassHealthFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	int16 Value = 100;
};

USTRUCT()
struct PROJECTR_API FProjectileDamageFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "")
	int16 DamagePerHit = 10;
};

USTRUCT()
struct PROJECTR_API FMinZParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement")
	float Value = 0.f;
};

UCLASS(meta = (DisplayName = "ProjectileWithDamage"))
class PROJECTR_API UMassProjectileWithDamageTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category = "Damage", EditAnywhere)
	int16 DamagePerHit = 10;

	FMassMovementParameters Movement;

	UPROPERTY(Category = "Movement", EditAnywhere)
	float GravityMagnitude = 0.f;

	/** Minimum Z value for projectiles at which they get destroyed */
	UPROPERTY(Category = "Movement", EditAnywhere)
	FMinZParameters MinZ;
};

UCLASS(meta = (DisplayName = "ProjectileDamagable"))
class PROJECTR_API UMassProjectileDamagableTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS()
class PROJECTR_API UMassProjectileDamageProcessor : public UMassProcessor
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
