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

bool IsTransformFacingDirection(const FTransform& Transform, const FVector& TargetDirection, float* OutCurrentHeadingRadians, float* OutDesiredHeadingRadians, float* OutDeltaAngleRadians, float* OutAbsDeltaAngleRadians)
{
	const FVector CurrentForward = Transform.GetRotation().GetForwardVector();

	// These are in range of (-PI, PI].
	const float& CurrentHeadingRadians = UE::MassNavigation::GetYawFromDirection(CurrentForward);
	const float& DesiredHeadingRadians = UE::MassNavigation::GetYawFromDirection(TargetDirection);

	// In Range [-PI, PI].
	const float& DeltaAngleRadians = (FMath::FindDeltaAngleRadians(CurrentHeadingRadians, DesiredHeadingRadians));
	// In Range [0, PI].
	const float& AbsDeltaAngleRadians = FMath::Abs(DeltaAngleRadians);

	if (OutCurrentHeadingRadians)
	{
		*OutCurrentHeadingRadians = CurrentHeadingRadians;
	}
	if (OutDesiredHeadingRadians)
	{
		*OutDesiredHeadingRadians = DesiredHeadingRadians;
	}
	if (OutDeltaAngleRadians)
	{
		*OutDeltaAngleRadians = DeltaAngleRadians;
	}
	if (OutAbsDeltaAngleRadians)
	{
		*OutAbsDeltaAngleRadians = AbsDeltaAngleRadians;
	}

	return FMath::IsNearlyEqual(AbsDeltaAngleRadians, 0.f, 0.01f); // TODO: Is this a good tolerance?
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
		// These are in range of (-PI, PI].
		float OutCurrentHeadingRadians;
		float OutDesiredHeadingRadians;

		// In Range [-PI, PI].
		float OutTotalDeltaAngleRadians;

		// In Range [0, PI].
		float OutAbsTotalDeltaAngleRadians;

		const bool& bIsAtDesiredHeading = IsTransformFacingDirection(Transform, MoveTarget.Forward, &OutCurrentHeadingRadians, &OutDesiredHeadingRadians, &OutTotalDeltaAngleRadians, &OutAbsTotalDeltaAngleRadians);
		if (bIsAtDesiredHeading)
		{
			return;
		}

		float NewHeadingRadians;
		float DeltaHeadingRadians = FMath::DegreesToRadians(OrientationParams.TurningSpeed) * DeltaTime;
		if (OutAbsTotalDeltaAngleRadians <= DeltaHeadingRadians)
		{
			NewHeadingRadians = OutDesiredHeadingRadians;
		}
		else
		{
			NewHeadingRadians = OutCurrentHeadingRadians + (OutTotalDeltaAngleRadians > 0.f ? DeltaHeadingRadians : -DeltaHeadingRadians);
		}

		FQuat Rotation(FVector::UpVector, NewHeadingRadians);
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
