// Fill out your copyright notice in the Description page of Project Settings.

#include "MassMapTranslatorProcessor.h"
#include "MassEnemyTargetFinderProcessor.h"
#include "MassTrackTargetProcessor.h"
#include "MassCommonFragments.h"
#include "MassMoveToCommandProcessor.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeTypes.h"

//----------------------------------------------------------------------//
//  UMassMapDisplayableTrait
//----------------------------------------------------------------------//
void UMassMapDisplayableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddFragment<FTargetEntityFragment>();
	BuildContext.AddFragment<FMapLocationFragment>();
	BuildContext.AddTag<FMassMapDisplayableTag>();
}

//----------------------------------------------------------------------//
//  UMassMapTranslatorProcessor
//----------------------------------------------------------------------//
UMassMapTranslatorProcessor::UMassMapTranslatorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMassMapTranslatorProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMapLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassMapDisplayableTag>(EMassFragmentPresence::All);
}

void ProcessEntity(const FVector& EntityLocation, FMapLocationFragment& MapLocationFragment)
{
	static const float MinWorldX = -1000.f; // TODO
	static const float MaxWorldX = 1000.f; // TODO
	static const float MinWorldY = -1000.f; // TODO
	static const float MaxWorldY = 1000.f; // TODO

	static const float MinMapX = 0.f; // TODO
	static const float MaxMapX = 1920.f; // TODO
	static const float MinMapY = 0.f; // TODO
	static const float MaxMapY = 1080.f; // TODO

	static const float WorldXSize = MaxWorldX - MinWorldX;
	static const float WorldYSize = MaxWorldY - MinWorldY;

	static const float MapXSize = MaxMapX - MinMapX;
	static const float MapYSize = MaxMapY - MinMapY;

	float XOffsetPercent = (EntityLocation.X - MinWorldX) / WorldXSize;
	float YOffsetPercent = (EntityLocation.Y - MinWorldY) / WorldYSize;

	FVector2D MapLocation;
	MapLocation.X = MinMapX + XOffsetPercent / MapXSize;
	MapLocation.Y = MinMapY + YOffsetPercent / MapYSize;

	MapLocationFragment.MapLocation = MapLocation;
}

void UMassMapTranslatorProcessor::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassMapTranslatorProcessor);

	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
		const TArrayView<FMapLocationFragment> MapLocationList = Context.GetMutableFragmentView<FMapLocationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			ProcessEntity(TransformList[EntityIndex].GetTransform().GetLocation(), MapLocationList[EntityIndex]);
		}
	});
}
