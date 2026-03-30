# Phase 5: Player Files, Player Mesh, Attacker vs Goalie

## Overview

Three goals:
1. Extract player code into `Player.h` / `Player.cpp`
2. Add a kinematic Player Mesh body for puck deflection
3. Create attacker + goalie with different parameters

## Before starting: Fix existing hover values

Your current PuckSim.cpp hover PID uses .cs defaults, not prefab values. These need correcting
regardless of the refactor:

| Parameter | Current (wrong) | Correct (prefab) |
|-----------|-----------------|-------------------|
| Target Distance | 1.0 | **1.2** |
| Derivative Gain | 15 | **8** |
| Raycast Distance | 1.45 | **1.3** |
| Raycast Offset Y | 1.0 | 1.0 (correct) |
| Max Force | 40 | 40 (correct) |
| P Gain | 100 | 100 (correct) |

You'll fix these as part of the refactor below, not as a separate step.

---

## Step 1: Create `Player.h`

Create `Source/Player.h`. This file defines two things: the parameters struct and the runtime
player struct. Here's what goes in it and why.

### 1a: Include guard and forward declarations

You need Jolt's `BodyID` type and `Vec3`/`RVec3`, but you don't need the full physics system
headers. Forward-declare what you can, include what you must. At minimum you'll need:

```
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
```

You'll also need to forward-declare `PhysicsSystem` and `BodyInterface` (or include their
headers) since the function declarations reference them.

### 1b: `PlayerParams` struct

This holds the configuration values. Most are shared between roles — only 8 differ.
Structure it like this:

```
struct PlayerParams {
    // --- Speed caps (DIFFER between roles) ---
    float maxForwardSpeed;
    float maxForwardSprintSpeed;
    float maxBackwardSpeed;
    float maxBackwardSprintSpeed;

    // --- Gameplay (DIFFER between roles) ---
    bool  canDash;
    float slideDrag;
    float tackleSpeedThreshold;
    float tackleForceThreshold;

    // --- Shared values (identical in both prefabs) ---
    float mass;                 // 90
    float gravityMultiplier;    // 2.0
    float hoverTargetDistance;  // 1.2
    float hoverRaycastDistance; // 1.3
    float hoverPGain;           // 100
    float hoverDGain;           // 8
    float hoverMaxForce;        // 40
};
```

Then define two `constexpr` (or `const`) instances — `ATTACKER_PARAMS` and `GOALIE_PARAMS`.

The 8 values that differ:

| Parameter             | Attacker | Goalie |
|-----------------------|----------|--------|
| maxForwardSpeed       | 7.5      | 5.0    |
| maxForwardSprintSpeed | 8.75     | 6.25   |
| maxBackwardSpeed      | 7.5      | 5.0    |
| maxBackwardSprintSpeed| 8.5      | 6.0    |
| canDash               | false    | true   |
| slideDrag             | 0.1      | 1.0    |
| tackleSpeedThreshold  | 7.6      | 5.1    |
| tackleForceThreshold  | 7.0      | 5.0    |

All shared values are the same for both — just fill in the numbers from the table above.

### 1c: `Player` struct

This holds the runtime state for one player instance:

```
struct Player {
    BodyID bodyId;          // Dynamic body on PLAYER layer
    BodyID meshId;          // Kinematic body on PLAYER_MESH layer
    float  prevHoverDist;   // PID derivative state (previous frame's distance)
    const PlayerParams* params;
};
```

### 1d: Function declarations

Declare four functions:

- `Player CreatePlayer(BodyInterface& bi, const PlayerParams& params, RVec3 startPos);`
  — Creates both the dynamic body and kinematic mesh body, returns a populated Player struct.

- `void DestroyPlayer(BodyInterface& bi, Player& player);`
  — RemoveBody + DestroyBody for both bodyId and meshId.

- `void UpdatePlayerHover(BodyInterface& bi, PhysicsSystem& ps, Player& player, float dt);`
  — Raycast, PID, apply hover force. Uses params for gains/distances.

- `void SyncPlayerMesh(BodyInterface& bi, const Player& player, float dt);`
  — MoveKinematic the mesh to the body's current position/rotation.

---

## Step 2: Create `Player.cpp`

Create `Source/Player.cpp`. This implements the four functions.

### 2a: Includes

Include `Player.h` plus the Jolt headers you need for the implementations:
- Shape headers: `CapsuleShape.h`, `RotatedTranslatedShape.h`
- Body: `BodyCreationSettings.h`, `BodyInterface.h` (if not already via forward decl)
- Raycast: `RayCast.h`, `CastResult.h`
- Physics system: `PhysicsSystem.h` (for `GetNarrowPhaseQuery`)
- Collision filtering: `ObjectLayerFilter.h` (for the ice filter)

### 2b: Ice layer filter

Move the `IceLayerFilter` class from PuckSim.cpp into Player.cpp. It's only used by the hover
raycast, so it belongs here. You can put it in an anonymous namespace or make it `static` since
it's an implementation detail.

### 2c: `CreatePlayer` implementation

This function does what lines 425–440 of your current PuckSim.cpp do, plus creates the mesh body.

**Dynamic body (PLAYER layer):**
- Shape: `CapsuleShape(0.45f, 0.3f)` wrapped in `RotatedTranslatedShape` with offset `(0, 1.25, 0)`
- Position: `startPos`
- Motion type: Dynamic
- Layer: `Layers::PLAYER`
- Gravity factor: `params.gravityMultiplier` (2.0)
- Friction: 0, Restitution: 0.2
- Linear/Angular damping: 0
- Mass: `params.mass` (90), override = CalculateInertia
- AllowSleeping: false

**Kinematic mesh body (PLAYER_MESH layer):**
- Shape: Same `CapsuleShape(0.45f, 0.3f)` + `RotatedTranslatedShape` offset `(0, 1.25, 0)`
  (you can create a second shape, or share the ShapeRefC — Jolt shapes are ref-counted and immutable)
- Position: same `startPos`
- Motion type: **Kinematic**
- Layer: `Layers::PLAYER_MESH`
- Friction: 0, Restitution: 0.2

Both created with `CreateAndAddBody(..., EActivation::Activate)`.

Initialize `prevHoverDist` to 0 and `params` to point at the passed-in params.

Return the populated `Player` struct.

### 2d: `DestroyPlayer` implementation

Call `RemoveBody` then `DestroyBody` for both `player.bodyId` and `player.meshId`.

### 2e: `UpdatePlayerHover` implementation

This is a direct extraction of lines 484–506 from your current PuckSim.cpp, but using the
player's params instead of hardcoded values. Key changes:

- Raycast origin: `bi.GetPosition(player.bodyId) + Vec3(0, 1.0f, 0)` (the 1.0 is the raycast offset — same for both roles)
- Raycast direction: `Vec3(0, -params.hoverRaycastDistance, 0)` → **1.3** (not 1.45)
- Default distance (miss): `params.hoverRaycastDistance` → **1.3**
- Hit distance: `hit.mFraction * params.hoverRaycastDistance`
- Error: `params.hoverTargetDistance - currentDistance` → target is **1.2** (not 1.0)
- P term: `params.hoverPGain * error` → **100**
- D term: `params.hoverDGain * (-derivative)` → **8** (not 15)
- Clamp: `[0, params.hoverMaxForce]` → **40**
- Force: `Vec3(0, clampedOutput * params.mass, 0)` (ForceMode.Acceleration → multiply by mass for Jolt's AddForce)

Use the ice layer filter for the raycast (same as current code).

Update `player.prevHoverDist = currentDistance` at the end.

### 2f: `SyncPlayerMesh` implementation

One call:
```
bi.MoveKinematic(player.meshId,
    bi.GetPosition(player.bodyId),
    bi.GetRotation(player.bodyId),
    dt);
```

**Why MoveKinematic, not SetPosition:** MoveKinematic calculates and sets the kinematic body's
velocity so it arrives at the target over the timestep. When the puck collides with a moving
player mesh, Jolt uses that velocity to compute a realistic impulse. SetPosition teleports with
zero velocity — the puck would bounce off as if the player were stationary.

---

## Step 3: Update `PuckSim.cpp`

### 3a: Include Player.h

Add `#include "Player.h"` near the top. Remove any code that moved to Player.cpp
(the `IceLayerFilter` class).

### 3b: Replace player creation

Delete the current player creation block (lines 424–440: the capsule shape, player_settings,
player_id). Replace with two calls:

```
Player attacker = CreatePlayer(body_interface, ATTACKER_PARAMS, RVec3(5.0_r, 0.5_r, 0.0_r));
Player goalie   = CreatePlayer(body_interface, GOALIE_PARAMS,   RVec3(-5.0_r, 0.5_r, 0.0_r));
```

### 3c: Replace hover logic in the loop

Delete the hover block (lines 466–520: prevDistance, ice_filter, raycast, PID, force, printing).
Replace with:

```
// Update hover + sync mesh for both players
UpdatePlayerHover(body_interface, physics_system, attacker, cDeltaTime);
UpdatePlayerHover(body_interface, physics_system, goalie, cDeltaTime);
SyncPlayerMesh(body_interface, attacker, cDeltaTime);
SyncPlayerMesh(body_interface, goalie, cDeltaTime);
```

For diagnostic printing every 50 steps, you can keep a similar block but print both players'
positions/velocities. Reference `attacker.bodyId` and `goalie.bodyId`.

### 3d: Sync Puck Trigger

The existing puck trigger sync (`SetPosition` of puck_trigger to puck position) stays in
PuckSim.cpp — it's not player-related.

### 3e: Replace cleanup

Delete the old `body_interface.RemoveBody(player_id)` / `DestroyBody(player_id)` lines.
Replace with:

```
DestroyPlayer(body_interface, attacker);
DestroyPlayer(body_interface, goalie);
```

### 3f: Test puck deflection

Set the puck velocity to aim at the attacker:

```
body_interface.SetLinearVelocity(puck_id, Vec3(10.0f, 0.0f, 0.0f));
```

The puck should hit the attacker's mesh (at X=5) and deflect with restitution 0.2. It should
NOT collide with the player's dynamic body (PLAYER layer has no puck collision).

---

## Step 4: Update CMakeLists.txt

Add `Source/Player.cpp` and `Source/Player.h` to your target's source list. The exact line
depends on your current CMakeLists.txt structure — it's likely an `add_executable` or
`target_sources` call.

---

## What to expect

- Both players hover at ~1.2m above ice (hover target distance)
- Hover PID with D=8 will be slightly less damped than the old D=15 — expect a bit more
  oscillation initially, but it should still converge
- Puck hits the attacker's player mesh and deflects with restitution 0.2
- Puck does NOT collide with the player capsule (PLAYER layer has no Puck in the collision matrix)
- Both hover PIDs converge independently with identical gains

## Gotchas

- **MoveKinematic, not SetPosition** — SetPosition gives zero velocity to the kinematic body.
  Puck would bounce off as if player were stationary.
- **PLAYER_MESH layer must have Puck collision enabled** — already in your collision matrix.
- **Each Player struct has its own PID state** — `prevHoverDist` is per-instance, no sharing.
- **Kinematic bodies must be activated** — use `EActivation::Activate` when creating mesh bodies.
- **Player Mesh restitution is 0.2** — from the Player Mesh physics material.
- **Shape sharing** — You can share one `ShapeRefC` between the dynamic body and the kinematic
  mesh since they use identical geometry. Jolt shapes are immutable and ref-counted.
- **Params pointer lifetime** — If you define `ATTACKER_PARAMS` / `GOALIE_PARAMS` as file-scope
  constants, they outlive everything. The `Player` struct stores a `const PlayerParams*` so
  it never copies — just make sure the pointed-to params don't go out of scope.
