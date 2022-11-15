// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectM : ModuleRules
{
	public ProjectM(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
			"Niagara",
			"StateTreeModule",
			"StructUtils",
			"UMG",
			"MotionWarping",
			"ContextualAnimation",
			"AnimToTexture",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"MassAIBehavior",
			"MassAIDebug",

			// AI/MassCrowd Plugin Modules
			"MassCrowd",

			// Runtime/MassEntity Plugin Modules
			"MassEntity",

			// Runtime/MassGameplay Plugin Modules
			"MassActors",
			"MassCommon",
			"MassGameplayDebug",
			"MassLOD",
			"MassMovement",
			"MassNavigation",
			"MassRepresentation",
			"MassReplication",
			"MassSpawner",
			"MassSimulation",
			"MassSignals",

			"Slate",
			"SlateCore",

			"NavigationSystem",
		});

		if (Target.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
		{
				PrivateDependencyModuleNames.Add("GameplayDebugger");
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
		}
		else
		{
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
		}
	}
}
