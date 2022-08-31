// Fill out your copyright notice in the Description page of Project Settings.


#include "MassSimpleUpdateISMProcessor.h"

//----------------------------------------------------------------------//
//  UMassSimpleUpdateISMTrait
//----------------------------------------------------------------------//
void UMassSimpleUpdateISMTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
  BuildContext.AddTag<FMassSimpleUpdateISMTag>();
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