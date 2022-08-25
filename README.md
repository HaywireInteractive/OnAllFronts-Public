# ProjectM

## What is this?

ProjectM is a proof of concept [MilSim](https://en.wikipedia.org/wiki/MilSim) FPS game developed with [Unreal Engine 5](https://www.unrealengine.com/en-US/unreal-engine-5), leveraging the new [MassEntity](https://docs.unrealengine.com/5.0/en-US/overview-of-mass-entity-in-unreal-engine/) ([ECS](https://en.wikipedia.org/wiki/Entity_component_system)) framework to have a very large number of entities not seen in traditional FPS games (tens or hundreds of thousands). Think [Squad](https://joinsquad.com/) meets [Foxhole](https://www.foxholegame.com/) meets [PlanetSide 2](https://www.planetside2.com/home) mixed in with massive number of AI controlled soldiers.

The project is open sourced to make it easy for others to contribute and to help others learn the MassEntity system which is not very well-documented/supported yet.

## Latest Demo

10,000 soliders with one team attacking and one defending:
https://youtu.be/Cg0k9XhYcQA

## Motivation

In today's "MilSim" games players frequently end up doing things or seeing this that would not happen in an actual MilSim. Examples:
- Soldiers magically spawn onto the battlefield
- Areas of interest are captured by standing near them
- Areas that should be defended have little or no one defending them (because it's boring)
- Soldiers being revived who should be dead
- Soliders being healed much faster than they should be able to

ProjectM aims to eliminate these unrealistic occurrences while keeping the game fun. It mainly does this by leveraging AI to do the not-so-fun stuff. Examples:
- Moving soldiers to the frontlines
- Defending all areas of interest, even those with little activity
- Logistics
- Vehicle repairs
- Medical evacuation (medevac)

Human players will never spawn soliders onto the battlefield. Instead they will take control of an AI-controller soldier already on the battlefield. Each match will last days/weeks, until all soldiers of one team are dead or main objectives have been accomplished.

# Phases

Currently this is a hobby project, so it's unclear how far it'll go. Therefore, the project is broken into multiple phases.

## Phase 1: Single-player

Same as multiplayer below, except only one human-controller soldier.

## Phase 2: Multiplayer MMO

All players in the game will be in a single battle at a time. Because of the scale being attempted, a single server will likely not be able to handle hosting this game. Instead it will require multiple servers communicating with each other.

The multiplayer phase of the game is split into two sub-phases below.

### Phase 2A: Server switching between zones

The map will be split up into zones. When a player reaches the border between zones, they load into a different server that manages that area of the map.

### Phase 2B: Server meshing across zones

Seamless switching between servers and players at the borders of servers will talk to all nearby servers to have a seamless experience across zones.

## Possible Additional Features

These ideas below would be explored in one of the phases above.

1. Railroad system for logistics. Railroads can be damaged and repaired.
1. Pontoon bridges which can be built for river crossings. Bridges can be destroyed.
1. Building destruction

# Development Environment Setup

1. Install [Git for Windows](https://gitforwindows.org/). Use the default options.
1. Install Unreal Engine 5.0.3 from [Epic Games Launcher](https://store.epicgames.com/en-US/download).
1. Create a new project using the [City Sample project](https://www.unrealengine.com/marketplace/en-US/product/city-sample) from UE Marketplace.
1. In PowerShell:
    1. cd to City Sample project folder
    1. `cd Plugins`
    1. `git clone https://github.com/LeroyTechnologies/ProjectM.git`
1. Install Visual Studio 2022.
1. Generate VS project from right clicking CitySample.uproject in City Sample project folder.
1. Open CitySample.sln.
1. In order to get Mass ParallelForEachEntityChunk to actually parallelize, it requires passing argument to editor on launch:
    1. In VS Solution Explorer, right click City Sample project > Properties.
    1. Debugging > Command Arguments > Add "-ParallelMassQueries=1" to the end.
1. Run project from VS.
