# ProjectR

## What is this?

ProjectR is a proof of concept [MilSim](https://en.wikipedia.org/wiki/MilSim) FPS game developed with [Unreal Engine 5](https://www.unrealengine.com/en-US/unreal-engine-5), leveraging the new [MassEntity](https://docs.unrealengine.com/5.0/en-US/overview-of-mass-entity-in-unreal-engine/) ([ECS](https://en.wikipedia.org/wiki/Entity_component_system)) framework to have a very large number of entities not seen in traditional FPS games (tens or hundreds of thousands). Think [Squad](https://joinsquad.com/) meets [Foxhole](https://www.foxholegame.com/) meets [PlanetSide 2](https://www.planetside2.com/home) mixed in with massive number of AI controlled soldiers.

The project is open sourced to make it easy for others to contribute and to help others learn the MassEntity system which is not very well-documented/supported yet.

## Motivation

In today's "MilSim" games players frequently end up doing things or seeing this that would not happen in an actual MilSim. Examples:
- Soldiers magically spawn onto the battlefield
- Areas of interest are captured by standing near them
- Areas that should be defended have little or no one defending them (because it's boring)
- Soldiers being revived who should be dead
- Soliders being healed much faster than they should be able to

ProjectR aims to eliminate these unrealistic occurrences while keeping the game fun. It mainly does this by leveraging AI to do the not-so-fun stuff. Examples:
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

1. [Build UE5 from source](https://docs.unrealengine.com/5.0/en-US/building-unreal-engine-from-source/). This is currently needed because it fixes a [bug in StateTree](https://forums.unrealengine.com/t/why-is-statetree-triggering-an-array-index-out-of-bounds-exception/617609).
    1. Ensure you have [this commit](https://github.com/EpicGames/UnrealEngine/commit/6178e39cc4f6c1bca9872d231d87086530c29dd6). It's in the [ue5-main branch](https://github.com/EpicGames/UnrealEngine/tree/ue5-main).
    1. Ensure you are using Visual Studio 2022.
1. Clone this repo.
1. Open project in UE5.
1. The following content is large and unmodified from source, so it's Git ignored. Add it to the project manually:
    1. Third Person Content Pack: Content Drawer > Add > Add Feature or Content Pack > Blueprint > Third Person > Add to project. Note only the Content/Characters folder is ignored from this Content Pack, but adding it will still work correctly.
