# Phase 4: Player Body with Hover PID

## Overview

Add a single attacker player that hovers above the ice using a PID controller. The player is a capsule with custom gravity (2x), center of mass at (0, 1.25, 0), and force-based movement with manual drag.

This is the most complex phase so far. Take it step by step.

## Step 1: Add new includes

```cpp
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>  // already have this
```

You'll also need for raycasting:
```cpp
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerFilter.h>
#include <Jolt/Physics/Collision/ObjectLayerFilter.h>
```

## Step 2: Create the player body

The player capsule in Unity has radius 0.3 and height 1.5. In Jolt, `CapsuleShape` takes `(halfHeight, radius)` where halfHeight is the cylindrical part only:

```
halfHeight = (1.5 - 2 * 0.3) / 2 = 0.45
radius = 0.3
```

The center of mass is at (0, 1.25, 0) in the prefab. Use `OffsetCenterOfMassShape` to shift it:

```
CapsuleShape* capsule = new CapsuleShape(0.45f, 0.3f);
OffsetCenterOfMassShapeSettings com_offset(Vec3(0, 1.25f, 0), capsule);
ShapeSettings::ShapeResult player_shape_result = com_offset.Create();
ShapeRefC player_shape = player_shape_result.Get();
```

Body settings:

```
BodyCreationSettings:
  - shape: the OffsetCenterOfMassShape from above
  - position: RVec3(0, 2, 0)    (start above ice, hover will settle it)
  - rotation: Quat::sIdentity()
  - motionType: EMotionType::Dynamic
  - objectLayer: Layers::PLAYER
  - mGravityFactor: 2.0f
  - mFriction: 0.0f
  - mRestitution: 0.2f           (Player Body material)
  - mLinearDamping: 0.0f         (drag handled manually)
  - mAngularDamping: 0.0f        (drag handled manually)
  - mOverrideMassProperties: EOverrideMassProperties::CalculateInertia
  - mMassPropertiesOverride.mMass: 90.0f
  - mAllowSleeping: false        (player should never sleep)
```

## Step 3: Understand the gravity setup

The game does something specific with gravity:

```csharp
// Cancel Unity's default gravity
Rigidbody.AddForce(Vector3.up * -Physics.gravity.y, ForceMode.Acceleration);
// Re-apply at 2x
Rigidbody.AddForce(Vector3.down * -Physics.gravity.y * gravityMultiplier, ForceMode.Acceleration);
```

Net effect: gravity is `2 * 9.81 = 19.62 m/s²` downward. In Jolt, `mGravityFactor = 2.0f` does this directly — no manual force needed.

## Step 4: Implement the Hover PID

This runs every tick, before the physics update.

### 4a: Raycast down to find distance to ice

```
Origin: player position + (0, 1, 0)     (the raycastOffset from prefab)
Direction: down (0, -1, 0)
Max distance: 1.45
Filter: ICE layer only
```

In Jolt, use `PhysicsSystem::GetNarrowPhaseQuery().CastRay(...)`. The ray origin and direction use Jolt's `RRayCast` type.

If the ray hits, `currentDistance = hit.mFraction * 1.45`. If it misses, `currentDistance = 1.45` (and `IsGrounded = false`).

### 4b: PID calculation

The game's PID controller uses **derivative-on-measurement** (not derivative-on-error). This is important — it avoids derivative kick when the target changes.

```
error = targetDistance - currentDistance
P_term = proportionalGain * error
D_term = derivativeGain * (-valueDerivative)    // derivative-on-measurement
output = P_term + D_term                         // I_term is 0
```

Where:
- `targetDistance = 1.0` (the default TargetDistance, which becomes `hoverDistance * Balance` = `1.2 * 1.0` when fully balanced — but start with 1.0 for now and refine later)
- `proportionalGain = 100`
- `derivativeGain = 15`
- `valueDerivative = (currentDistance - lastCurrentDistance) / dt`
- `derivativeSmoothing = 1.0` (no smoothing)

### 4c: Apply the hover force

```
force = clamp(output, 0, 40)    // never pushes downward, max 40
```

Apply using Jolt equivalent of `ForceMode.Acceleration` — this means mass-independent. In Jolt, `Body::AddForce` applies force (F = ma), so to get acceleration you multiply by mass:

```cpp
body_interface.AddForce(player_id, Vec3(0, force * 90.0f, 0));
```

Wait — actually, `ForceMode.Acceleration` in Unity means "treat the value as acceleration, ignore mass." So `AddForce(40, Acceleration)` on a 90kg body is equivalent to `AddForce(40 * 90 = 3600, Force)`. In Jolt, `AddForce` always takes a force (Newtons), so yes, multiply by mass.

### 4d: PID state

You need to track between ticks:
- `float prevDistance = 0.0f;` (initialize to 0)

## Step 5: Implement basic movement (optional for this phase)

Movement only runs when `IsGrounded` (raycast hit something). For Phase 4, you can apply a constant forward input to test, or skip movement entirely and just verify hover works.

If you want to test movement:

```
Each tick (when grounded):
  1. Pick a movement direction (e.g., Vec3(1, 0, 0) to move in +X)
  2. Get horizontal speed: sqrt(vx² + vz²)
  3. If speed < maxForwardSpeed (7.5):
       Apply acceleration: AddForce(direction * forwardAcceleration * mass)
       (forwardAcceleration = 2.0, ForceMode.Acceleration -> multiply by mass 90)
  4. Apply drag: velocity *= (1 - drag * dt)
       drag = 0.025, dt = 0.02
       so multiplier = 1 - 0.025 * 0.02 = 0.9995
```

Important: the game applies drag by directly scaling the velocity vector each tick. In Jolt you'd do:
```cpp
Vec3 vel = body_interface.GetLinearVelocity(player_id);
vel *= (1.0f - 0.025f * cDeltaTime);
body_interface.SetLinearVelocity(player_id, vel);
```

## Step 6: Update the simulation loop

The loop now needs to:
1. Sync puck trigger to puck (already done)
2. Do hover raycast
3. Calculate PID
4. Apply hover force
5. Optionally apply movement + drag
6. `physics_system.Update(...)`
7. Print player position/velocity along with puck data

Change the loop termination — it currently runs until the puck sleeps. Since the player has `mAllowSleeping = false`, the loop won't terminate based on the player. Keep the puck sleep check, or switch to a fixed number of steps (e.g., 500 steps = 10 seconds).

## Step 7: Print player state

Every 50 steps, also print:
- Player Y position (should stabilize around hover height)
- Player Y velocity (should converge to ~0)
- Current hover distance from raycast

## What to expect

- Player spawns at Y=2, begins falling under 2x gravity
- Hover PID kicks in when raycast detects ice below
- Y position oscillates and converges to approximately Y = center_of_mass_height + hover_offset
- The exact hover height depends on the interaction between the capsule shape, center of mass offset, and PID target
- Y velocity should dampen to near-zero within a few seconds
- If movement is enabled, horizontal speed should ramp toward 7.5 then plateau due to drag

## Gotchas

- **CapsuleShape(halfHeight, radius)** — halfHeight is the cylindrical part only (0.45), NOT total half-height
- **OffsetCenterOfMassShape** wraps the capsule — create the capsule first, then wrap it
- **ForceMode.Acceleration → multiply by mass in Jolt.** Every `AddForce(..., Acceleration)` in the game becomes `AddForce(value * mass)` in Jolt.
- **Raycast must filter to ICE layer only.** Use an `ObjectLayerFilter` that only accepts `Layers::ICE`.
- **PID uses derivative-on-measurement**, not derivative-on-error. Use `-valueDerivative` not `errorDerivative`.
- **Initialize prevDistance to 0.** The first tick will have a large derivative — this is fine, the PID settles quickly.
- **Hover force is clamped to [0, 40]** — it never pushes the player downward. If the player is too high, only gravity pulls them back down.
- **Drag is manual**, not Jolt's built-in damping. Set `mLinearDamping = 0` and `mAngularDamping = 0` on the player body.
- **Player should never sleep** — set `mAllowSleeping = false`.
