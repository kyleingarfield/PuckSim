# Phase 2: Object Layers and Collision Matrix

## Step 1: Add new includes

```cpp
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
```

## Step 2: Replace the Layers namespace

Replace the existing 2-layer namespace with all 20 game layers:

```cpp
namespace Layers
{
    static constexpr ObjectLayer DEFAULT = 0;
    static constexpr ObjectLayer TRANSPARENT_FX = 1;
    static constexpr ObjectLayer IGNORE_RAYCAST = 2;
    static constexpr ObjectLayer LEVEL = 3;
    static constexpr ObjectLayer WATER = 4;
    static constexpr ObjectLayer UI = 5;
    static constexpr ObjectLayer STICK = 6;
    static constexpr ObjectLayer PUCK = 7;
    static constexpr ObjectLayer PLAYER = 8;
    static constexpr ObjectLayer PLAYER_MESH = 9;
    static constexpr ObjectLayer POST_PROCESSING = 10;
    static constexpr ObjectLayer GOAL_POST = 11;
    static constexpr ObjectLayer BOARDS = 12;
    static constexpr ObjectLayer ICE = 13;
    static constexpr ObjectLayer GOAL_NET = 14;
    static constexpr ObjectLayer GOAL_TRIGGER = 15;
    static constexpr ObjectLayer PLAYER_COLLIDER = 16;
    static constexpr ObjectLayer NET_CLOTH_COLLIDER = 17;
    static constexpr ObjectLayer GOAL_FRAME = 18;
    static constexpr ObjectLayer PUCK_TRIGGER = 19;
    static constexpr ObjectLayer NUM_LAYERS = 20;
};
```

## Step 3: Replace BroadPhaseLayers namespace

```cpp
namespace BroadPhaseLayers
{
    static constexpr BroadPhaseLayer STATIC(0);
    static constexpr BroadPhaseLayer DYNAMIC(1);
    static constexpr BroadPhaseLayer SENSOR(2);
    static constexpr uint NUM_LAYERS(3);
};
```

## Step 4: Delete the three hand-written filter classes

Delete these entire classes:
- `ObjectLayerPairFilterImpl`
- `BPLayerInterfaceImpl`
- `ObjectVsBroadPhaseLayerFilterImpl`

They are replaced by Jolt's table-based versions in the next step.

## Step 5: Create table-based filters in main()

Replace the old filter object declarations with these. Put them in the same spot (before `PhysicsSystem::Init`).

### Broadphase layer mapping

```cpp
BroadPhaseLayerInterfaceTable broad_phase_layer_interface(Layers::NUM_LAYERS, BroadPhaseLayers::NUM_LAYERS);

// Static geometry
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::ICE, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::BOARDS, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_POST, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_FRAME, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_NET, BroadPhaseLayers::STATIC);

// Dynamic objects
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PUCK, BroadPhaseLayers::DYNAMIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PLAYER, BroadPhaseLayers::DYNAMIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PLAYER_MESH, BroadPhaseLayers::DYNAMIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::STICK, BroadPhaseLayers::DYNAMIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PLAYER_COLLIDER, BroadPhaseLayers::DYNAMIC);

// Sensors
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_TRIGGER, BroadPhaseLayers::SENSOR);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PUCK_TRIGGER, BroadPhaseLayers::SENSOR);

// Unused layers (map to static — they have no collision pairs anyway)
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::DEFAULT, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::TRANSPARENT_FX, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::IGNORE_RAYCAST, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::LEVEL, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::WATER, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::UI, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::POST_PROCESSING, BroadPhaseLayers::STATIC);
broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::NET_CLOTH_COLLIDER, BroadPhaseLayers::STATIC);
```

### Object layer pair filter (collision matrix)

```cpp
ObjectLayerPairFilterTable object_vs_object_layer_filter(Layers::NUM_LAYERS);

// Stick collisions
object_vs_object_layer_filter.EnableCollision(Layers::STICK, Layers::STICK);
object_vs_object_layer_filter.EnableCollision(Layers::STICK, Layers::PUCK);

// Puck collisions
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::PUCK);
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::PLAYER_MESH);
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::GOAL_POST);
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::BOARDS);
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::ICE);
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::GOAL_NET);
object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::GOAL_FRAME);

// Player collisions
object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::PLAYER);
object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::GOAL_POST);
object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::BOARDS);
object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::ICE);
object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::GOAL_NET);
object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::PLAYER_COLLIDER);

// Goal detection
object_vs_object_layer_filter.EnableCollision(Layers::GOAL_TRIGGER, Layers::PUCK_TRIGGER);
```

### Broadphase vs object filter (auto-derived)

```cpp
ObjectVsBroadPhaseLayerFilterTable object_vs_broadphase_layer_filter(
    broad_phase_layer_interface, BroadPhaseLayers::NUM_LAYERS,
    object_vs_object_layer_filter, Layers::NUM_LAYERS);
```

## Step 6: Update body layer assignments

Change your existing body creation code:

| Body | Old Layer | New Layer |
|------|-----------|-----------|
| Ice | `Layers::NON_MOVING` | `Layers::ICE` |
| All 4 walls | `Layers::NON_MOVING` | `Layers::BOARDS` |
| Puck | `Layers::MOVING` | `Layers::PUCK` |

## Step 7: Add self-test (temporary)

After `PhysicsSystem::Init`, before creating any bodies, add:

```cpp
cout << "Collision matrix self-test:" << endl;
const char* layer_names[] = {
    "Default", "TransparentFX", "IgnoreRaycast", "Level", "Water", "UI",
    "Stick", "Puck", "Player", "PlayerMesh", "PostProcessing", "GoalPost",
    "Boards", "Ice", "GoalNet", "GoalTrigger", "PlayerCollider",
    "NetClothCollider", "GoalFrame", "PuckTrigger"
};
for (uint i = 0; i < Layers::NUM_LAYERS; i++)
    for (uint j = i; j < Layers::NUM_LAYERS; j++)
        if (object_vs_object_layer_filter.ShouldCollide(i, j))
            cout << "  " << layer_names[i] << " <-> " << layer_names[j] << endl;
```

## Expected self-test output

```
Collision matrix self-test:
  Stick <-> Stick
  Stick <-> Puck
  Puck <-> Puck
  Puck <-> PlayerMesh
  Puck <-> GoalPost
  Puck <-> Boards
  Puck <-> Ice
  Puck <-> GoalNet
  Puck <-> GoalFrame
  Player <-> Player
  Player <-> GoalPost
  Player <-> Boards
  Player <-> Ice
  Player <-> GoalNet
  Player <-> PlayerCollider
  GoalTrigger <-> PuckTrigger
```

That's 16 pairs. After the self-test, the puck simulation should run identically to Phase 1.

## After verification

Remove the self-test print loop once you've confirmed the pairs are correct — you won't need it anymore.
