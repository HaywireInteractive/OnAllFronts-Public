// Copyright (c) 2022 Leroy Technologies. Licensed under MIT License.


#include "MassAgentOrientNoCharSyncTrait.h"
#include "Translators/MassSceneComponentLocationTranslator.h"
#include "MassCommonTypes.h"
#include "Components/SceneComponent.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplateRegistry.h"

//----------------------------------------------------------------------//
//  UMassAgentOrientNoCharSyncTrait
//----------------------------------------------------------------------//
void UMassAgentOrientNoCharSyncTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FMassSceneComponentWrapperFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::ActorToMass))
	{
		BuildContext.AddTranslator<UMassSceneComponentOrientationToMassTranslator>();
	}

	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::MassToActor))
	{
		BuildContext.AddTranslator<UMassSceneComponentOrientationToActorTranslator>();
	}
}

//----------------------------------------------------------------------//
//  UMassSceneComponentOrientationToMassTranslator
//----------------------------------------------------------------------//
UMassSceneComponentOrientationToMassTranslator::UMassSceneComponentOrientationToMassTranslator()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	RequiredTags.Add<FMassSceneComponentOrientationCopyToMassTag>();
}

void UMassSceneComponentOrientationToMassTranslator::ConfigureQueries()
{
	AddRequiredTagsToQuery(EntityQuery);
	EntityQuery.AddRequirement<FMassSceneComponentWrapperFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void UMassSceneComponentOrientationToMassTranslator::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this](FMassExecutionContext& Context)
		{
			const TConstArrayView<FMassSceneComponentWrapperFragment> ComponentList = Context.GetFragmentView<FMassSceneComponentWrapperFragment>();
			const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();

			const int32 NumEntities = Context.GetNumEntities();
			for (int32 i = 0; i < NumEntities; ++i)
			{
				if (const USceneComponent* AsComponent = ComponentList[i].Component.Get())
				{
					TransformList[i].GetMutableTransform().SetRotation(AsComponent->GetComponentTransform().GetRotation());
				}
			}
		});
}

//----------------------------------------------------------------------//
//  UMassSceneComponentOrientationToActorTranslator
//----------------------------------------------------------------------//
UMassSceneComponentOrientationToActorTranslator::UMassSceneComponentOrientationToActorTranslator()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	RequiredTags.Add<FMassSceneComponentOrientationCopyToActorTag>();
	bRequiresGameThreadExecution = true;
}

void UMassSceneComponentOrientationToActorTranslator::ConfigureQueries()
{
	AddRequiredTagsToQuery(EntityQuery);
	EntityQuery.AddRequirement<FMassSceneComponentWrapperFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RequireMutatingWorldAccess(); // due to mutating World by setting actor's transform
}

void UMassSceneComponentOrientationToActorTranslator::Execute(UMassEntitySubsystem& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [this](FMassExecutionContext& Context)
		{
			const TConstArrayView<FMassSceneComponentWrapperFragment> ComponentList = Context.GetFragmentView<FMassSceneComponentWrapperFragment>();
			const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();

			const int32 NumEntities = Context.GetNumEntities();
			for (int32 i = 0; i < NumEntities; ++i)
			{
				if (USceneComponent* AsComponent = ComponentList[i].Component.Get())
				{
					AsComponent->SetWorldRotation(TransformList[i].GetTransform().GetRotation());
				}
			}
		});
}