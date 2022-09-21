// Fill out your copyright notice in the Description page of Project Settings.


#include "MassTrackedVehicleOrientationProcessor.h"

void UMassTrackedVehicleOrientationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(&World);
	check(EntitySubsystem);

	BuildContext.AddFragment<FMassMoveTargetFragment>();
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddTag<FMassTrackedVehicleOrientationTag>();

	const FConstSharedStruct OrientationFragment = EntitySubsystem->GetOrCreateConstSharedFragment(UE::StructUtils::GetStructCrc32(FConstStructView::Make(Orientation)), Orientation);
	BuildContext.AddConstSharedFragment(OrientationFragment);
}

UMassTrackedVehicleOrientationProcessor::UMassTrackedVehicleOrientationProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void UMassTrackedVehicleOrientationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassTrackedVehicleOrientationParameters>(EMassFragmentPresence::All);
}

struct FProcessEntityContext
{
	FProcessEntityContext(const FMassMoveTargetFragment& InMoveTarget, FTransform& InTransform, const FMassTrackedVehicleOrientationParameters& InOrientationParams, const float& InDeltaTime, const FMassEntityHandle& InEntity) : MoveTarget(InMoveTarget), Transform(InTransform), OrientationParams(InOrientationParams), DeltaTime(InDeltaTime), Entity(InEntity) {}

	const FMassMoveTargetFragment& MoveTarget;
	FTransform& Transform;
	const FMassTrackedVehicleOrientationParameters& OrientationParams;
	const float& DeltaTime;
	const FMassEntityHandle& Entity;

	void ProcessEntity()
	{
		const FVector CurrentForward = Transform.GetRotation().GetForwardVector();

		const float& CurrentHeading = UE::MassNavigation::GetYawFromDirection(CurrentForward);
		const float& DesiredHeading = UE::MassNavigation::GetYawFromDirection(MoveTarget.Forward);

		const bool& bIsAtDesiredHeading = FMath::IsNearlyEqual(CurrentHeading, DesiredHeading);
		if (UE::Mass::Debug::IsDebuggingEntity(Entity, nullptr))
		{
			UE_LOG(LogTemp, Warning, TEXT("UMassTrackedVehicleOrientationProcessor: Entity sn=%d, CurrentHeading=%.2f , DesiredHeading=%.2f, IsAtDesiredHeading=%d"), Entity.SerialNumber, CurrentHeading, DesiredHeading, bIsAtDesiredHeading);
		}

		if (bIsAtDesiredHeading)
		{
			return;
		}

		float NewHeading;
		float DeltaHeading = FMath::DegreesToRadians(OrientationParams.TurningSpeed) * DeltaTime;
		if (FMath::Abs(CurrentHeading - DesiredHeading) <= DeltaHeading)
		{
			NewHeading = DesiredHeading;
		}
		else
		{
			DeltaHeading = DesiredHeading > CurrentHeading ? DeltaHeading : -DeltaHeading;
			NewHeading = CurrentHeading + DeltaHeading;
		}

		FQuat Rotation(FVector::UpVector, NewHeading);
		Transform.SetRotation(Rotation);
	}
};

void UMassTrackedVehicleOrientationProcessor::Execute(UMassEntitySubsystem& EntitySubsystem,
	FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassTrackedVehicleOrientationProcessor_Execute);

	// Clamp max delta time to avoid large values during initialization.
	const float DeltaTime = FMath::Min(0.1f, Context.GetDeltaTimeSeconds());

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [DeltaTime](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const FMassTrackedVehicleOrientationParameters& OrientationParams = Context.GetConstSharedFragment<FMassTrackedVehicleOrientationParameters>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FProcessEntityContext(MoveTargetList[EntityIndex], LocationList[EntityIndex].GetMutableTransform(), OrientationParams, DeltaTime, Context.GetEntity(EntityIndex)).ProcessEntity();
		}
	});
}
