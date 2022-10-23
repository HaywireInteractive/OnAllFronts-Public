# ProjectM

## Latest Demo

2,000 soldier and tank city battle:

[![2,000 soldier and tank city battle](https://img.youtube.com/vi/NDfSpPRgOHg/0.jpg)](https://youtu.be/NDfSpPRgOHg)

https://youtu.be/NDfSpPRgOHg

## What is this?

ProjectM is a proof of concept [MilSim](https://en.wikipedia.org/wiki/MilSim) FPS / RTS game developed with [Unreal Engine 5](https://www.unrealengine.com/en-US/unreal-engine-5), leveraging the new [MassEntity](https://docs.unrealengine.com/5.0/en-US/overview-of-mass-entity-in-unreal-engine/) ([ECS](https://en.wikipedia.org/wiki/Entity_component_system)) framework to have a very large number of entities not seen in traditional FPS games (tens or hundreds of thousands). Think [Squad](https://joinsquad.com/) meets [Foxhole](https://www.foxholegame.com/) meets [PlanetSide 2](https://www.planetside2.com/home) mixed in with massive number of AI controlled soldiers.

The project is open sourced to make it easy for others to contribute and to help others learn the MassEntity system which is not very well-documented/supported yet.

## Motivation

In today's MilSim games players frequently end up doing things or seeing this that would not happen in an actual MilSim. Examples:
- Soldiers magically spawn onto the battlefield
- Areas of interest are captured by standing near them
- Areas that should be defended have little or no one defending them (because it's boring)
- Soldiers being revived who should be dead
- Soldiers being healed much faster than they should be able to

ProjectM aims to eliminate these unrealistic occurrences while keeping the game fun. It mainly does this by leveraging AI to do the not-so-fun stuff. Examples:
- Moving soldiers to the frontlines
- Defending all areas of interest, even those with little activity
- Logistics
- Vehicle repairs
- Medical evacuation (medevac)

Human players will never spawn soldiers onto the battlefield. Instead they will take control of an AI-controller soldier already on the battlefield. Each match will last days/weeks, until all soldiers of one team are dead or main objectives have been accomplished.

# Phases

Currently this is a hobby project, so it's unclear how far it'll go. Therefore, the project is broken into multiple phases. For current status, see [Project Tracker](https://github.com/users/LeroyTechnologies/projects/1).

## Phase 1: Demo
- Single player: One human player and thousands of AI soldiers (no vehicles)
- Single map

## Phase 2: Early Access

- Multiplayer FPS game with per-server battles
- Human and AI soldiers only (no vehicles)
- Single map

## Phase 3: Release

- Ground vehicles
- Commander assets (e.g. artillery)
- More maps

## Phase 4: Multiplayer MMO

All players in the game will be in a single battle at a time, across multiple servers.

This phase is split into two sub-phases below.

### Phase 4A: Server switching between zones

The map will be split up into zones. When a player reaches the border between zones, they load into a different server that manages that area of the map.

### Phase 4B: Server meshing across zones

Seamless switching between servers and players at the borders of servers will talk to all nearby servers to have a seamless experience across zones.

## End Game

The ultimate goal is to recreate a war at a scale such as in this video (with the teams a bit more balanced): https://youtu.be/RSqKx3FG0Lw

## Possible Features

These ideas below would be explored in one of the phases above.

1. Railroad system for logistics. Railroads can be damaged and repaired.
1. Oil pipelines for transfering oil to vehicles quicker.
1. Base building (FOBs and COPs)
1. Pontoon bridges which can be built for river crossings. Bridges can be destroyed.
1. Building destruction
1. Naval warfare
1. Helicopters
1. Airplanes (not player controlled initially, only AI)

# FAQ

- Where can I ask questions or get involved in the project? Email leroytech231+gh@gmail.com.


# Development Environment Setup

1. Install [Git for Windows](https://gitforwindows.org/). Use the default options.
1. Install Unreal Engine 5.0.3 from [Epic Games Launcher](https://store.epicgames.com/en-US/download).
1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/).
    1. Follow steps [here](https://docs.unrealengine.com/5.0/en-US/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine/#optionsforanewvisualstudioinstallation) for which options to choose when installing.
    1. If you don't have it installed, install latest .NET Core 3.1: https://dotnet.microsoft.com/en-us/download/dotnet?cid=getdotnetcore
1. Create a new project using one of the two options below:
    1. If you want to use the [City Sample project](https://www.unrealengine.com/marketplace/en-US/product/city-sample) for developing in a large level, get the project from UE Marketplace and then create a project using that template.
    1. If you want to quickly develop in a smaller project, in UE create a new empty C++ project.
1. If there is no .sln in the project folder, generate VS project from right clicking .uproject in project folder.
1. Open .sln in Visual Studio if it isn't already open.
1. Visual Studio > Tools > Options
    1. Text Editor > C/C++ > Tabs > select Keep tabs
    1. Text Editor > Advanced > uncheck "Use adaptive formatting"
1. If project isn't open yet in UE, run the project from VS.
1. Enable the required plugins in UE project if they are not already:
    1. Edit > Plugins
    1. Search for "mass"
    1. Check MassAI, MassCrowd, MassEntity, MassGameplay. Answer Yes if it asks if you're sure.
1. Quit UE.
1. In PowerShell:
    1. `cd` to project folder. This is the folder that has the .uproject file.
    1. If `Plugins` folder does not exist, `mkdir Plugins`.
    1. `cd Plugins`
    1. `git clone https://github.com/LeroyTechnologies/ProjectM.git`
	1. If in City Sample: `rmdir -recurse AnimToTexture`
	1. `git clone --branch projectm https://github.com/LeroyTechnologies/AnimToTexture.git`
1. Right click the .uproject file again and re-generate the solution to get the new files from the Plugins folder to show in VS.
1. In order to get Mass ParallelForEachEntityChunk to actually parallelize, it requires passing argument to editor on launch:
    1. In VS Solution Explorer, right click the project under Games folder > Properties.
    1. Debugging > Command Arguments > Add "-ParallelMassQueries=1" to the end.
1. Rerun project from VS.
1. At this point you should confirm that the ProjectM folder shows up in your Content Drawer under Plugins. If you don't see Plugins, click Settings button on top right of Content Drawer > Show Plugin Content.
1. Add Content Packs from Content Drawer > Add > Add Feature or Content Pack > Blueprint
    1. First Person > Add to project.
    1. Third Person > Add to project.
1. Add Unreal Engine Marketplace free content to project. Note that some of this content hasn't been migrated to UE5 yet, so when you try to add it from the Epic Games Launcher, check "Show all projects" after clicking "Add to Project". Then in the "Select Version" dropdown select the newest version in the list.
    1. [Military Weapons Silver](https://www.unrealengine.com/marketplace/en-US/product/military-weapons-silver)
        1. Then open M_WeaponMaster_01 > Used with Instanced Static Meshes > Apply > Save
    1. [M1A1 Abrams Tank](https://www.unrealengine.com/marketplace/en-US/product/m1a1-abrams-tank)
    1. [Realistic Starter VFX Pack Vol 2](https://www.unrealengine.com/marketplace/en-US/product/realistic-starter-vfx-pack-vol)
    1. [Animation Starter Pack](https://www.unrealengine.com/marketplace/en-US/product/animation-starter-pack)
1. Edit > Project Settings
    1. Engine - Input > Bindings > Action Mappings
        1. Add "MoveToCommand", 1 key
        1. Add "Spectate", F key
        1. Add "Respawn", K key
        1. Add "Map", M key
        1. Add "Shoot", Left Mouse Button
        1. Add "Aim", Right Mouse Button
        1. Add "Crouch", Left Ctrl key
        1. Add "Sprint", Left Shift key
        1. Add "Reload", R key
        1. Add "Prone", Z key
    1. Engine - Mass
		1. Search for "MassUpdateISMProcessor" > Mass > Module Settings > Mass Entity > Processor CDOs > Index (for MassUpdateISMProcessor) > Auto Register with Processing Phases > uncheck. Note this will already be unchecked in the City Sample project.
		1. Search for "MassGenericUpdateISMVertexAnimationProcessor" > Mass > Module Settings > Mass Entity > Processor CDOs > Index (for MassGenericUpdateISMVertexAnimationProcessor) > Auto Register with Processing Phases > check
    1. Engine - Navigation System > Agents > Supported Agents > Add 2 elements:
        1. Name: Soldier, Nav Agent Radius: 50.0, Nav Agent Height: 200.0
        1. Name: Tank, Nav Agent Radius: 500.0, Nav Agent Height: 200.0
1. Now in Content Drawer open "Plugins/ProjectM Content/Playgrounds/Maps/L_Small" level and use PIE to test out the project.
    1. You might have to build NavMesh paths via menu for move commands to work: Build > Build Paths.
1. For a larger amount of soldiers see L_MediumWithTanks or L_Large levels.

## To add soldiers to City Sample Level
1. Duplicate Small_City_LVL and call it Small_City_ProjectM_LVL
1. Open Small_City_ProjectM_LVL
1. Delete Actors
    1. Mass Spawners: BP_MassTraffic* and BP_MassCrowdSpawner
    1. BP_CitySampleWorldInfo
    1. BP_Nightmode
    1. SmartObjectCollection
1. Open L_Template
1. Copy PM_* actors
1. Paste in Small_City_ProjectM_LVL. Ensure locations of all actors were preserved.
1. From menu: Build > Build ZoneGraph
1. Select each PM_MilitaryUnitMassSpawner* actor > Details > World Partition > Is Spatially Loaded > uncheck
1. World Settings > GameMode Override > BP_FirstPersonGameModeCommander
1. We need to make collisions loaded all the time because AI soldiers rely on them to determine if they have line of sight to enemies:
    1. Load all cells in World Partition
    1. While we've got all cells loaded, build NavMesh from menu: Build > Build Paths
    1. In Outliner search for "coll"
    1. Click on any actor in the Outliner and select all (Ctrl+A)
    1. Details > World Partition > Is Spatially Loaded > uncheck
    1. World Partition: Unload all cells and load any if desired
