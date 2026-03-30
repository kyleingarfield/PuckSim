# Phase 6: Stick Body and PID Control

## Overview

The stick is the most physics-complex object in Puck. In the game, each player owns one stick
that floats freely as a dynamic rigidbody. Two PID controllers apply forces at the shaft handle
and blade handle to drive the stick toward target positions. The player body's velocity couples
into the stick, and the stick's angular velocity couples back into the player as torque.

This phase implements the stick's physics body and PID force system. The StickPositioner (which
converts player input into target positions via raycasts) is **not** part of this phase — for RL,
the agent will supply target positions directly.

**Three goals:**
1. Create `Stick.h` / `Stick.cpp` following the Player pattern
2. Implement dual PID controllers that drive the stick via forces at handle points
3. Implement player-stick velocity coupling (both directions)

---

## How the Stick Works in the Game

Understanding the full system before you simplify anything:

1. **StickPositioner** (on the player) takes joystick input, fires raycasts against the rink
   surface, and computes two world-space target positions: `ShaftTargetPosition` and
   `BladeTargetPosition`. This is the **input system** — we skip it for RL.

2. **Stick.FixedUpdate** (on the stick body) runs every physics tick:
   a. Reads shaft/blade target positions from StickPositioner
   b. Feeds them into two Vector3 PID controllers alongside the current handle positions
   c. Applies velocity transfer: player body velocity at each handle point -> stick force
   d. Applies PID output: force at each handle point -> stick force
   e. Zeroes the stick's local Z angular velocity (prevents roll)
   f. Snaps the stick's Z euler angle to 0 via MoveRotation
   g. Transfers a scaled portion of the stick's angular velocity back to the player as torque

3. **Collisions**: Stick only collides with Stick and Puck (already in your collision matrix).
   Stick-to-stick collisions modulate the blade PID's proportional gain.

---

## Prefab Value Corrections

The `physics-values.md` StickPositioner section has been updated with prefab-authoritative values.
Key corrections from .cs defaults:

| Parameter | .cs Default | Prefab (authoritative) |
|-----------|-------------|----------------------|
| StickPositioner P Gain | 0.75 | **75** |
| StickPositioner I Gain | 5 | **500** |
| StickPositioner I Saturation | 5 | **100** |
| StickPositioner Output Min | -15 | **-750** |
| StickPositioner Output Max | 15 | **750** |
| Soft Collision Force | 1 | **20** |
| Stick Derivative Smoothing | 0.1 | **1** (both shaft and blade) |
| Stick Linear Vel Transfer Mult | 0.25 | **20** |
| Stick Angular Vel Transfer Mult | 0.25 | **0.3** |
| Stick Bounce Combine | Average | **Multiply** |
| Stick Shaft Bounce Combine | Average | **Multiply** |

These are already corrected in `physics-values.md`. Listed here so you know the scale of
the difference — the .cs defaults are wildly wrong for several values.

---

## Step 1: Create `Stick.h`

Create `Source/Stick.h`. Follow the same pattern as Player.h.

### 1a: Include guard and forward declarations

Same pattern as Player.h:
```
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
```

Forward-declare `BodyInterface` and `PhysicsSystem`.

### 1b: `StickParams` struct

The stick has almost no attacker/goalie differences in its own rigidbody settings — both use
identical mass, drag, PID gains, etc. The **only** difference is the blade handle position.
Everything else is shared.

```
struct StickParams {
    // --- Handle positions (local to stick body) ---
    // Shaft handle is the same for both roles
    // Blade handle differs: attacker (0, 0.118, 1.328) vs goalie (0, 0.0902, 1.3366)
    Vec3 shaftHandleLocalPos;    // (0, -0.006, -0.992)
    Vec3 bladeHandleLocalPos;    // differs by role

    // --- Rigidbody ---
    float mass;                  // 1.0

    // --- Shaft Handle PID ---
    float shaftPGain;            // 500
    float shaftIGain;            // 0
    float shaftISaturation;      // 0
    float shaftDGain;            // 20
    float shaftDSmoothing;       // 1
    float minShaftPGainMult;     // 0.25

    // --- Blade Handle PID ---
    float bladePGain;            // 500
    float bladeIGain;            // 0
    float bladeISaturation;      // 0
    float bladeDGain;            // 20
    float bladeDSmoothing;       // 1

    // --- Velocity coupling ---
    float linearVelTransferMult;      // 20
    float angularVelTransferMult;     // 0.3
    bool  transferAngularVelocity;    // true
};
```

Then define two const instances:

```
extern const StickParams ATTACKER_STICK_PARAMS;
extern const StickParams GOALIE_STICK_PARAMS;
```

The values (all from prefab):

| Field | Attacker | Goalie |
|-------|----------|--------|
| shaftHandleLocalPos | (0, -0.006, -0.992) | (0, -0.006, -0.992) |
| bladeHandleLocalPos | (0, 0.118, 1.328) | (0, 0.0902, 1.3366) |
| mass | 1 | 1 |
| shaftPGain | 500 | 500 |
| shaftIGain | 0 | 0 |
| shaftISaturation | 0 | 0 |
| shaftDGain | 20 | 20 |
| shaftDSmoothing | 1 | 1 |
| minShaftPGainMult | 0.25 | 0.25 |
| bladePGain | 500 | 500 |
| bladeIGain | 0 | 0 |
| bladeISaturation | 0 | 0 |
| bladeDGain | 20 | 20 |
| bladeDSmoothing | 1 | 1 |
| linearVelTransferMult | 20 | 20 |
| angularVelTransferMult | 0.3 | 0.3 |
| transferAngularVelocity | true | true |

### 1c: Vector3PID struct

You need a 3-axis PID controller. The game uses `Vector3PIDController` which runs independent
PID on each axis. Define a small struct for this:

```
struct Vector3PID {
    Vec3 integral;       // accumulated integral term
    Vec3 prevError;      // previous error (for derivative)
    Vec3 prevDerivative; // smoothed derivative
};
```

The game's PID Update logic (per axis) is:
1. `error = target - current`
2. `integral += error * dt`, clamped to [-saturation, +saturation] per axis
3. `rawDerivative = (error - prevError) / dt`
4. `derivative = lerp(prevDerivative, rawDerivative, 1 - smoothing)`
   (smoothing=1 means derivative never changes from initial 0; smoothing=0 means no filtering)
   Wait — actually check the game's formula carefully. With `derivativeSmoothing = 1` (the
   prefab value), this makes the smoothed derivative = `prevDerivative * 1 + rawDerivative * 0`.
   That means the derivative term is effectively **zeroed** since prevDerivative starts at 0.
   Combined with I gains of 0, the stick PID is essentially **P-only** control.
5. `output = P * error + I * integral + D * derivative`

This is a critical insight: **the stick PID is effectively proportional-only in practice.**
The D gain (20) exists in the parameters, but derivativeSmoothing=1 kills it entirely.
The I gain is 0. So the effective force is just `P_gain * error * dt`.

You still need to implement the full PID (the struct, the update function) so that if you
later want to test other gain values you can. But knowing it's P-only helps you verify
correctness — the stick should just spring toward its targets.

### 1d: `Stick` struct

```
struct Stick {
    BodyID bodyId;
    const StickParams* params;
    float  length;              // distance between shaft and blade handles

    // PID state
    Vector3PID shaftPID;
    Vector3PID bladePID;

    // Stick-to-stick collision gain modulation
    float bladeGainMultiplier;  // reset to 1.0 each tick
};
```

### 1e: Function declarations

```
Stick CreateStick(BodyInterface& bi, const StickParams& params, RVec3 startPos);

void DestroyStick(BodyInterface& bi, Stick& stick);

void UpdateStick(BodyInterface& bi, Stick& stick,
                 Vec3 shaftTarget, Vec3 bladeTarget,
                 BodyID playerBodyId, float dt);
```

**Why targets are passed in:** In the game, the StickPositioner computes targets from input +
raycasts. For RL, the agent supplies targets directly. Passing them as arguments keeps the stick
physics clean and decoupled from input processing.

**Why playerBodyId is passed in:** The stick needs the player body's velocity for coupling.
Rather than storing a pointer to the Player struct, just pass the body ID. The stick reads
the player's point velocity and later applies torque back to the player.

---

## Step 2: Create `Stick.cpp`

### 2a: Includes

Include `Stick.h`, `Layers.h`, and the Jolt headers you need:
- `CapsuleShape.h` (shaft approximation)
- `BoxShape.h` (blade approximation)
- `CompoundShapeSettings.h` or `StaticCompoundShape.h` (combining shaft + blade)
- `BodyCreationSettings.h`, `BodyInterface.h`

### 2b: Define the two param instances

Fill in the values from the table in Step 1b. Only the blade handle positions differ.

### 2c: Stick shape

The game uses convex mesh colliders for the stick blade and shaft. You don't have the meshes,
so approximate with primitives. This is a reasonable trade-off for RL training:

**Option A — Single capsule (simplest):**
One capsule spanning the stick's full length. Fast, good enough for initial testing.
The stick is roughly 2.32m long (shaft-to-blade handle distance). A capsule with
half-height ~1.1 and radius ~0.03 placed at the stick's midpoint would approximate it.

**Option B — Compound shape (more accurate):**
A `StaticCompoundShape` with:
- Shaft: Capsule, half-height ~0.9, radius ~0.02, centered near shaft handle
- Blade: Box, roughly 0.15 x 0.02 x 0.10, positioned near blade handle

Start with **Option A** for Phase 6. You can refine the shape later once basic PID control works.

### 2d: `CreateStick` implementation

Create a dynamic body on the STICK layer:
- Shape: your chosen approximation
- Mass: 1.0 (with CalculateInertia, or manually set inertia tensor to (1,1,1) to match prefab)
- Gravity factor: 0 (UseGravity = false)
- Friction: 0
- Restitution: 0
- Linear damping: 0
- Angular damping: 0
- AllowSleeping: false (the PID applies constant forces)
- Motion quality: `LinearCast` (matches Unity's Continuous collision detection)

For mass properties: the prefab uses manual inertia tensor (1,1,1). In Jolt, you can set this
via `mMassPropertiesOverride` with `EOverrideMassProperties::MassAndInertiaProvided`:

```
player_settings.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
player_settings.mMassPropertiesOverride.mMass = 1.0f;
// Set diagonal inertia tensor to (1, 1, 1)
player_settings.mMassPropertiesOverride.mInertia = Mat44::sIdentity();
```

Compute `length` = distance between the two handle local positions.

Initialize both PID structs to zero. Set `bladeGainMultiplier` to 1.0.

### 2e: `UpdateStick` implementation

This is the core of the stick physics. It replicates `Stick.FixedUpdate()` from the game.

The game code (line by line from Stick.cs FixedUpdate, server-only section):

```
// 1. Set PID gains (including collision-modulated multiplier)
shaftPID.pGain = params.shaftPGain * shaftGainMultiplier;  // shaftGainMult is always 1.0
bladePID.pGain = params.bladePGain * bladeGainMultiplier;
// (I, D gains set from params too)

// 2. Compute PID outputs
Vec3 shaftForce = shaftPID.Update(dt, currentShaftHandlePos, shaftTarget);
Vec3 bladeForce = bladePID.Update(dt, currentBladeHandlePos, bladeTarget);

// 3. Apply player body velocity transfer to stick (at each handle)
//    This couples player movement into the stick
//    ForceMode.VelocityChange in Unity = instantaneous velocity change, no mass scaling
Vec3 shaftVelTransfer = playerBody.GetPointVelocity(shaftHandleWorldPos)
                        * linearVelTransferMult * dt;
Vec3 bladeVelTransfer = playerBody.GetPointVelocity(bladeHandleWorldPos)
                        * linearVelTransferMult * dt;
// Applied as VelocityChange (impulse, not force)
AddImpulseAtPosition(stick, shaftVelTransfer, shaftHandleWorldPos);
AddImpulseAtPosition(stick, bladeVelTransfer, bladeHandleWorldPos);

// 4. Apply PID output forces at handle positions
//    Also VelocityChange, scaled by dt
AddImpulseAtPosition(stick, shaftForce * dt, shaftHandleWorldPos);
AddImpulseAtPosition(stick, bladeForce * dt, bladeHandleWorldPos);

// 5. Zero out local Z angular velocity (prevent stick roll)
Vec3 localAngVel = InverseTransformDirection(stick.rotation, stick.angularVelocity);
localAngVel.z = 0;
stick.angularVelocity = TransformDirection(stick.rotation, localAngVel);

// 6. Snap Z euler angle to 0 via MoveRotation
Vec3 euler = WrapEulerAngles(stick.eulerAngles);
Quat targetRot = Quat::sEulerAngles(Vec3(euler.x, euler.y, 0));
// MoveRotation in Unity sets angular velocity to reach targetRot over one timestep

// 7. Transfer stick angular velocity back to player body (as torque)
Vec3 scaledAngVel = stick.angularVelocity * Vec3(0.5, 1, 0) * angularVelTransferMult;
if (transferAngularVelocity) {
    playerBody.AddTorque(-scaledAngVel);  // ForceMode.Acceleration
}

// 8. Reset blade gain multiplier for next tick
bladeGainMultiplier = 1.0;
```

#### Translating to Jolt

Key differences between Unity and Jolt you need to handle:

**Getting handle world positions:**
The handles are at fixed local offsets on the stick body. To get world position:
```
Vec3 shaftWorldPos = stickPos + stickRot * params.shaftHandleLocalPos;
Vec3 bladeWorldPos = stickPos + stickRot * params.bladeHandleLocalPos;
```

**ForceMode.VelocityChange → Jolt:**
Unity's `AddForceAtPosition(force, pos, ForceMode.VelocityChange)` applies an instantaneous
velocity change (ignores mass). In Jolt, the equivalent is `AddImpulse` (which DOES account
for mass). Since mass is 1.0, `AddImpulse` and VelocityChange are numerically identical here.
Use `BodyInterface::AddImpulse(bodyId, impulse, point)`.

**GetPointVelocity:**
Unity's `Rigidbody.GetPointVelocity(worldPoint)` returns the velocity of a point on the body,
accounting for both linear and angular velocity:
```
pointVel = linearVel + cross(angularVel, worldPoint - centerOfMass)
```
Jolt has `BodyInterface::GetPointVelocityCOM(bodyId, pointRelativeToCOM)`, or you can compute
it manually from `GetLinearVelocity` + `GetAngularVelocity`.

**MoveRotation (step 6):**
Unity's `Rigidbody.MoveRotation(target)` sets the angular velocity so the body arrives at the
target rotation after one timestep. In Jolt, you can compute the required angular velocity
yourself:
```
Quat deltaRot = targetRot * currentRot.Conjugated();
// Convert to axis-angle, divide angle by dt to get angular velocity
```
Or simply set the angular velocity directly via `SetAngularVelocity`.

**Torque transfer (step 7):**
Unity's `AddTorque(torque, ForceMode.Acceleration)` means torque is applied as angular
acceleration (ignores inertia). In Jolt, `AddTorque` applies `torque = inertia * angAccel`.
Since the player's inertia is auto-calculated from a 90kg capsule (not (1,1,1)), you need to
account for this. Multiply the desired angular acceleration by the player's inertia tensor.

Alternatively, for simplicity, you can use `AddAngularImpulse` with the value scaled
appropriately, or compute the equivalent torque.

**However**: for initial Phase 6 testing, you can **skip the torque transfer back to the player**
(step 7) and the Z-rotation constraint (steps 5-6). Get the basic PID positioning working first,
then add these refinements.

### 2f: Suggested implementation order within UpdateStick

1. Compute handle world positions from body transform + local offsets
2. Run PID update for shaft and blade (output = force vectors)
3. Apply velocity transfer impulses at handle positions
4. Apply PID impulses at handle positions
5. (Later) Zero Z angular velocity and snap Z rotation
6. (Later) Transfer angular velocity back to player

### 2g: The PID Update function

Implement a free function or method:

```
Vec3 UpdatePID(Vector3PID& state, float pGain, float iGain, float iSat,
               float dGain, float dSmoothing, float dt,
               Vec3 current, Vec3 target)
```

Per axis:
1. `error = target - current`
2. `state.integral += error * dt`; clamp each component to [-iSat, +iSat] (skip if iSat==0)
3. `rawDerivative = (error - state.prevError) / dt`
4. `state.prevDerivative = lerp(rawDerivative, state.prevDerivative, dSmoothing)`
   (Note: with smoothing=1, this keeps prevDerivative unchanged = effectively zero)
5. `output = pGain * error + iGain * state.integral + dGain * state.prevDerivative`
6. `state.prevError = error`
7. Return `output`

**Beware the lerp direction.** The game code in Vector3PIDController uses:
`derivative = smoothing * prevDerivative + (1-smoothing) * rawDerivative`
With smoothing=1 this means derivative = prevDerivative (starts at 0, stays at 0).

### 2h: `DestroyStick` implementation

Same pattern as DestroyPuck/DestroyPlayer: RemoveBody + DestroyBody.

---

## Step 3: Update `PuckSim.cpp`

### 3a: Include Stick.h

Add `#include "Stick.h"`.

### 3b: Create sticks after players

```
Stick attackerStick = CreateStick(body_interface, ATTACKER_STICK_PARAMS,
    RVec3(5.0_r, 1.5_r, 0.0_r));  // near attacker, at handle height
Stick goalieStick = CreateStick(body_interface, GOALIE_STICK_PARAMS,
    RVec3(-5.0_r, 1.5_r, 0.0_r));
```

### 3c: Update sticks in the loop

For initial testing, use static target positions to verify the PID works. Place the shaft
target at the player's hand position and the blade target out in front:

```
// Simple test targets — shaft near player body, blade extended forward
RVec3 attPos = body_interface.GetPosition(attacker.bodyId);
Vec3 attShaftTarget = Vec3(attPos) + Vec3(0, 1.5f, -1.0f);
Vec3 attBladeTarget = Vec3(attPos) + Vec3(0, 1.5f, 1.3f);

UpdateStick(body_interface, attackerStick,
    attShaftTarget, attBladeTarget,
    attacker.bodyId, cDeltaTime);

// Same for goalie
RVec3 goalPos = body_interface.GetPosition(goalie.bodyId);
Vec3 goalShaftTarget = Vec3(goalPos) + Vec3(0, 1.5f, -1.0f);
Vec3 goalBladeTarget = Vec3(goalPos) + Vec3(0, 1.5f, 1.3f);

UpdateStick(body_interface, goalieStick,
    goalShaftTarget, goalBladeTarget,
    goalie.bodyId, cDeltaTime);
```

### 3d: Diagnostic printing

Print stick position and velocity alongside player/puck data:

```
RVec3 stk_pos = body_interface.GetCenterOfMassPosition(attackerStick.bodyId);
Vec3 stk_vel = body_interface.GetLinearVelocity(attackerStick.bodyId);
cout << "  Att Stick Pos=(" << stk_pos.GetX() << "..." << ")" << endl;
```

### 3e: Destroy sticks before players

```
DestroyStick(body_interface, attackerStick);
DestroyStick(body_interface, goalieStick);
```

Destroy sticks **before** players since the stick update references player body IDs.

### 3f: Update CMakeLists.txt

Add `Source/Stick.cpp` and `Source/Stick.h` to the source list.

---

## Step 4: Build and Test

### What to expect

With static targets and P-only control (since D smoothing kills derivative):

- The stick should oscillate toward its target position and settle
- Because there's no damping (drag=0, D-term effectively=0), expect some oscillation
  that persists. This matches the game — the stick in Puck is springy and bouncy
- The PID output * dt acts as a velocity impulse each tick
- The stick should float (gravity=0) and not fall

### Verification checks

1. **Stick doesn't fall**: Gravity factor = 0, so the stick should hover at spawn height
   unless forces push it. If it falls, check your gravity factor setting.

2. **Stick moves toward targets**: With P=500 and the handle offset, the stick should
   accelerate toward the target position. Print the distance between blade handle world pos
   and blade target each tick — it should decrease (with oscillation).

3. **Velocity coupling**: With the player stationary (hovering), velocity transfer should be
   near zero. If you give the player a push, the stick should partially follow.

4. **Stick-puck collision**: Fire the puck at the stick. It should deflect off the stick body
   (STICK and PUCK layers already collide in your matrix).

### Debugging tips

- If the stick explodes (huge velocities), your PID output scaling is wrong. Remember: the
  game applies `pidOutput * dt` as VelocityChange. With dt=0.02 and P=500, a 1m error gives
  500 * 0.02 = 10 m/s impulse per handle per tick. That's aggressive but correct.

- If the stick doesn't move at all, check that the body is Dynamic (not Kinematic), not
  sleeping, and that AllowSleeping is false.

- If the stick rotates wildly, you may need to add the Z-rotation constraint (step 5-6 from
  the FixedUpdate breakdown). The stick is meant to stay flat — without the constraint,
  asymmetric forces on shaft vs blade handles will spin it.

---

## What We're Deferring

These are NOT part of Phase 6 but will be needed later:

- **StickPositioner logic** — raycast-based target computation from player input. For RL,
  the agent directly outputs target positions.
- **Blade angle rotation** — rotating the blade based on discrete angle input. Cosmetic for
  RL unless blade shape affects puck deflection angle.
- **Stick-to-stick collision gain modulation** — the `OnCollisionStay` that weakens the blade
  PID when your blade hits another stick's shaft. Important for gameplay but secondary for
  basic functionality.
- **Stick freeze/unfreeze** — constraining all DOF during game pauses.

---

## Gotchas

- **Inertia tensor (1,1,1) is manual, not calculated.** The game explicitly sets this. If you
  let Jolt auto-calculate from a capsule shape, the inertia will be much smaller and the PID
  gains won't produce the right behavior. Use `MassAndInertiaProvided`.

- **VelocityChange in Unity ignores mass.** With stick mass=1, `AddForce(f, VelocityChange)`
  equals `AddImpulse(f)` in Jolt. If you later change mass, this equivalence breaks.

- **The derivative term is dead.** derivativeSmoothing=1 means the smoothed derivative never
  departs from its initial value (0). The prefab deliberately kills the D term this way. Don't
  "fix" this — it's intentional.

- **linearVelocityTransferMultiplier = 20** (not 0.25). The .cs default is 0.25 but the prefab
  overrides it to 20. This is a 80x difference. If your stick doesn't follow the player at all,
  check this value.

- **Handle positions are in local space.** You must transform them to world space using the
  stick body's current position and rotation before computing PID errors or applying forces.

- **The stick has no gravity.** UseGravity=false. In Jolt, set `mGravityFactor = 0.0f`.
