// Fill out your copyright notice in the Description page of Project Settings.


#include "MassSimpleUpdateISMProcessor.h"

bool UMassSimpleUpdateISMProcessor_SkipRendering = false;
FAutoConsoleVariableRef CVarUMassSimpleUpdateISMProcessor_SkipRendering(TEXT("pm.UMassSimpleUpdateISMProcessor_SkipRendering"), UMassSimpleUpdateISMProcessor_SkipRendering, TEXT("UMassSimpleUpdateISMProcessor: Skip Rendering"));

//----------------------------------------------------------------------//
//  UMassSimpleUpdateISMTrait
//----------------------------------------------------------------------//
void UMassSimpleUpdateISMTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
  if (!UMassSimpleUpdateISMProcessor_SkipRendering)
  {
    BuildContext.AddTag<FMassSimpleUpdateISMTag>();
  }
}

//----------------------------------------------------------------------//
//  UMassSimpleUpdateISMProcessor
//----------------------------------------------------------------------//
UMassSimpleUpdateISMProcessor::UMassSimpleUpdateISMProcessor()
{
  bAutoRegisterWithProcessingPhases = true;
}

void UMassSimpleUpdateISMProcessor::ConfigureQueries()
{
  Super::ConfigureQueries();

  EntityQuery.AddTagRequirement<FMassSimpleUpdateISMTag>(EMassFragmentPresence::All);
}