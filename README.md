# ProjectR

Developed with Unreal Engine 5

# Development Environment Setup

1. [Build UE5 from source](https://docs.unrealengine.com/5.0/en-US/building-unreal-engine-from-source/). This is currently needed because it fixes a [bug in StateTree](https://forums.unrealengine.com/t/why-is-statetree-triggering-an-array-index-out-of-bounds-exception/617609). Ensure you have [this commit](https://github.com/EpicGames/UnrealEngine/commit/6178e39cc4f6c1bca9872d231d87086530c29dd6). It's in the [ue5-main branch](https://github.com/EpicGames/UnrealEngine/tree/ue5-main).
1. Clone this repo.
1. Open project in UE5.
1. The following content is large and unmodified from source, so it's Git ignored. Add it to the project manually:
    1. Third Person Content Pack: Content Drawer > Add > Add Feature or Content Pack > Blueprint > Third Person > Add to project. Note only the Content/Characters folder is ignored from this Content Pack, but adding it will still work correctly.
