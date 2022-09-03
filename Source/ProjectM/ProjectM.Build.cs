// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectM : ModuleRules
{
	public ProjectM(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
            "MassZoneGraphNavigation",
            "Niagara",
			"StateTreeModule",
			"StructUtils",
			"UMG",
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

            "ZoneGraph",
        });
	}
}
