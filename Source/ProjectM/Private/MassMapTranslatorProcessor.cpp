// Fill out your copyright notice in the Description page of Project Settings.

#include "MassMapTranslatorProcessor.h"
#include "MassCommonFragments.h"

void UMassMapDisplayableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, UWorld& World) const
{
	BuildContext.AddTag<FMassMapDisplayableTag>();
}
