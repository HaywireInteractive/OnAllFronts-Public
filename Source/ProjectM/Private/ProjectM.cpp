// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectM.h"

#define LOCTEXT_NAMESPACE "FProjectMModule"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory_ProjectM.h"
#endif // WITH_GAMEPLAY_DEBUGGER

void FProjectMModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
  IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
  GameplayDebuggerModule.RegisterCategory("ProjectM", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ProjectM::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate, 1);
  GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FProjectMModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectMModule, ProjectM)
