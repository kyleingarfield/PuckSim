# Phase 9 — Puck Polish + Stick Verification

## Overview

Add missing puck behaviors (Y-axis correction, grounded COM shift, inertia tensor switching, angular speed cap) and verify stick mechanics (angular velocity transfer, Z-rotation constraint, blade gain multiplier).

---

## Part A — Puck Y-Axis Correction on Stick Exit

**Source:** `Puck.cs` OnCollisionExit (line 371-382)

When the puck **leaves** stick contact, the game applies a one-shot downward acceleration to kill any upward velocity the stick imparted:

```csharp
// Unity — Puck.OnCollisionExit
Vector3 force = new Vector3(0f, Mathf.Min(0f, -Rigidbody.linearVelocity.y), 0f) * 5f;
Rigidbody.AddForce(force, ForceMode.Acceleration);
Rigidbody.linearVelocity = Vector3.ClampMagnitude(Rigidbody.linearVelocity, MaxSpeed);
Rigidbody.angularVelocity = Vector3.ClampMagnitude(Rigidbody.angularVelocity, MaxAngularSpeed);
```

### Algorithm

1. If puck Y velocity > 0: force.Y = -vel.Y * 5 (pushes down)
2. If puck Y velocity <= 0: force.Y = 0 (no correction)
3. ForceMode.Acceleration = mass-independent in Unity -> multiply by mass in Jolt
4. Clamp linear velocity magnitude to 30 m/s
5. Clamp angular velocity magnitude to 60 rad/s (prefab overrides .cs default of 30)

### Jolt mapping

This fires on `OnContactRemoved` — when puck and stick stop colliding. The contact listener's `OnContactRemoved` only gives you a `SubShapeIDPair`, not the bodies directly, so you need to store the puck and stick body IDs and look them up.

### Values

| Parameter | Value | Source |
|-----------|-------|--------|
| Y correction multiplier | 5 | Puck.cs (hardcoded) |
| Max linear speed | 30 m/s | Puck.prefab |
| Max angular speed | 60 rad/s | Puck.prefab (overrides .cs default of 30) |

---

## Part B — Puck Grounded Center-of-Mass Shift

**Source:** `Puck.cs` FixedUpdate (lines 130-142)

Every tick, the puck checks if it's on the ice and shifts its center of mass downward when grounded. This makes the puck more bottom-heavy on ice, reducing wobble.

```csharp
// Unity — Puck.FixedUpdate
IsGrounded = Physics.CheckSphere(transform.position, groundedCheckSphereRadius, groundedCheckSphereLayerMask);
if (IsGrounded)
    Rigidbody.centerOfMass = transform.TransformVector(groundedCenterOfMass);
else
    Rigidbody.centerOfMass = Vector3.zero;
```

### Algorithm

1. Sphere overlap check at puck position, radius 0.075, ice layer only
2. If overlapping ice: COM = local (0, -0.01, 0) transformed by puck rotation
3. If not overlapping: COM = (0, 0, 0) (geometric center)

### Jolt challenge

Jolt does not support runtime COM shifting on a body. The COM is baked into the shape at creation. Options:

**Option 1 — OffsetCenterOfMassShape wrapper (recommended):** Create two shapes — one with COM offset (0, -0.01, 0) and one without. Swap between them using `BodyInterface::SetShape()`. This is the cleanest approach and Jolt supports shape swapping on dynamic bodies.

**Option 2 — Corrective torque:** Apply a small gravitational torque to simulate the off-center mass. The effect of a 0.01m COM shift on a 0.375kg puck is tiny (torque = mass * g * offset * sin(tilt) = 0.375 * 9.81 * 0.01 ≈ 0.037 N*m at 90 degrees), so this could be approximated.

**Option 3 — Skip for now.** The 0.01m offset on a 0.304m diameter puck is extremely subtle. Could defer and see if validation (Phase 10) flags it as a meaningful discrepancy.

### Ground detection in Jolt

Use `PhysicsSystem::GetNarrowPhaseQuery().CollidePoint()` or `CollideShape()` with a small sphere. A sphere overlap at puck position with radius 0.075 against ice layer will replicate `Physics.CheckSphere()`.

### Values

| Parameter | Value | Source |
|-----------|-------|--------|
| Ground check sphere radius | 0.075 | Puck.prefab |
| Grounded COM offset | (0, -0.01, 0) local | Puck.prefab |
| Ground check layer mask | Ice (layer 13) | Puck.prefab |

---

## Part C — Puck Inertia Tensor Switching

**Source:** `Puck.cs` FixedUpdate (lines 152-160)

Every tick, the puck's inertia tensor changes based on whether it's touching a stick. When on a stick, higher X/Z inertia makes the puck resist tilting (like cupping the puck), while Y stays low for easy spin.

```csharp
// Unity — Puck.FixedUpdate
if (IsTouchingStick)
{
    Server_UpdateStickTensor(stickTensor, Quaternion.identity);
    TouchingStick = null;
}
else
{
    Server_UpdateStickTensor(defaultTensor, Quaternion.identity);
}

void Server_UpdateStickTensor(Vector3 inertiaTensor, Quaternion inertiaTensorRotation)
{
    Rigidbody.inertiaTensor = inertiaTensor;
    Rigidbody.inertiaTensorRotation = inertiaTensorRotation;
}
```

### Jolt approach

In Jolt, inertia is stored in `MotionProperties` as inverse inertia diagonal. To change it at runtime:

```cpp
// Need body lock to access MotionProperties
BodyLockWrite lock(physics_system.GetBodyLockInterface(), puckId);
if (lock.Succeeded())
{
    Body& body = lock.GetBody();
    MassProperties mp;
    mp.mMass = 0.375f;
    mp.mInertia = Mat44::sScale(Vec3(ix, iy, iz)); // diagonal inertia
    body.GetMotionProperties()->SetMassProperties(EAllowedDOFs::All, mp);
}
```

Alternatively, since we already call `BodyInterface` methods freely, check if `BodyInterface` exposes a mass properties setter. If not, the body lock approach works — just do it outside the physics step.

### Values

| Parameter | Value | Source |
|-----------|-------|--------|
| Default tensor (no stick) | (0.002, 0.002, 0.002) | Puck.prefab |
| Stick tensor (touching stick) | (0.006, 0.002, 0.006) | Puck.prefab |
| Tensor rotation | Identity | Puck.cs |

**Note:** The Puck.prefab rigidbody has initial m_InertiaTensor = (0.002, 0.002, 0.01), but the script's `defaultTensor` field overrides this to (0.002, 0.002, 0.002) every FixedUpdate. The script field wins at runtime.

### Stick contact tracking

`IsTouchingStick` is set by collision callbacks (OnCollisionStay with stick layer). In Jolt, use `OnContactPersisted` (fires every step while contact exists) to detect ongoing puck-stick contact. Set a flag each frame, clear it before physics, let the contact listener re-set it.

---

## Part D — Puck Angular Speed Cap

**Source:** `Puck.cs` OnCollisionExit (line 379)

Angular velocity is clamped to 60 rad/s, but ONLY on stick exit (not every frame). This is already covered by Part A — it's the third line of the stick-exit sequence.

Jolt has `mMaxAngularVelocity` on BodyCreationSettings, but that's a per-frame hard cap. The game only clamps on stick exit, so do NOT set Jolt's max angular velocity — instead apply the clamp manually in the stick-exit handler alongside the Y-correction.

---

## Part E — Stick Angular Velocity Transfer (Verification + Fix)

**Source:** `Stick.cs` FixedUpdate (lines 183-187)

```csharp
// Unity
Vector3 a3 = Vector3.Scale(Rigidbody.angularVelocity, new Vector3(0.5f, 1f, 0f))
             * angularVelocityTransferMultiplier;
if (transferAngularVelocity)
    PlayerBody.Rigidbody.AddTorque(-a3, ForceMode.Acceleration);
```

### Current implementation (Stick.cpp:164-170)

```cpp
Vec3 scaledAngVel = stickAngVel * Vec3(0.5f, 1.0f, 0.0f) *
    stick.params->angularVelTransferMult;
bi.AddTorque(playerBodyId, -scaledAngVel);
```

### Bug: Missing mass multiplication

Unity's `ForceMode.Acceleration` is mass-independent (applies torque = value * mass internally). Current Jolt code calls `AddTorque()` without multiplying by the player's mass. This means the torque is 90x weaker than intended (player mass = 90 kg).

**Fix:** Multiply by the player body's mass:

```
torque = -scaledAngVel * playerMass
```

Or pass the player mass into the function. Since we already have `playerBodyId`, we can get the mass or just hardcode 90.

### Values (all correct in current code)

| Parameter | Value | Source |
|-----------|-------|--------|
| Scale | (0.5, 1, 0) | Stick.cs (hardcoded) |
| Angular velocity transfer multiplier | 0.3 | Stick.prefab |
| Transfer enabled | true | Stick.prefab |

---

## Part F — Stick Z-Rotation Constraint (Verification)

**Source:** `Stick.cs` FixedUpdate (lines 177-182)

```csharp
// Unity — Step 1: Zero local Z angular velocity
Vector3 direction = transform.InverseTransformVector(Rigidbody.angularVelocity);
direction.z = 0f;
Rigidbody.angularVelocity = transform.TransformDirection(direction);

// Unity — Step 2: Correct Euler Z angle to 0
Vector3 vector = Utils.WrapEulerAngles(transform.eulerAngles);
Quaternion rot = Quaternion.Euler(new Vector3(vector.x, vector.y, 0f));
Rigidbody.MoveRotation(rot);
```

### Current implementation (Stick.cpp:155-162)

```cpp
// Step 1: Zero local Z angular velocity
Vec3 stickAngVel = bi.GetAngularVelocity(stick.bodyId);
Vec3 localAngVel = stickRot.Conjugated() * stickAngVel;
localAngVel.SetZ(0.0f);
bi.SetAngularVelocity(stick.bodyId, stickRot * localAngVel);

// Step 2: Correct Euler Z angle to 0
Vec3 euler = stickRot.GetEulerAngles();
Quat fixedRot = Quat::sEulerAngles(Vec3(euler.GetX(), euler.GetY(), 0.0f));
bi.SetRotation(stick.bodyId, fixedRot, EActivation::DontActivate);
```

### Verification status: LOOKS CORRECT

Both steps match the game logic:
1. Convert angular velocity to local space, zero Z, convert back
2. Extract euler angles, rebuild quaternion with Z=0

**One concern:** Jolt's `GetEulerAngles()` may use a different convention (rotation order) than Unity's `eulerAngles` (ZXY). If the euler decomposition order differs, the constraint could behave differently under large rotations. For small stick rotations this won't matter. Verify in Phase 10 validation if stick behavior diverges.

---

## Part G — Blade Gain Multiplier on Stick-Stick Contact

**Source:** `Stick.cs` OnCollisionStay (lines 245-266)

When THIS stick's blade hits ANOTHER stick's shaft, the blade PID proportional gain is scaled down based on where on the shaft the contact occurs. Contact near the other stick's handle = gain multiplied by 0.25 (weaker), contact near blade end = gain × 1.0 (full strength).

```csharp
// Unity
if (thisCollider.tag == "Stick Blade" && otherCollider.tag == "Stick Shaft")
{
    float num = Clamp(Distance(component.ShaftHandlePosition, contactPoint) / Length,
                      minShaftHandleProportionalGainMultiplier, 1f);
    bladeHandleProportionalGainMultiplier = num;
}
```

### Jolt approach

This requires knowing WHICH sub-shapes of the compound stick collided. In Jolt, `OnContactAdded`/`OnContactPersisted` provide `SubShapeID` pairs. Since each stick is a `StaticCompoundShape` with sub-shape 0 = shaft, sub-shape 1 = blade, you can decode sub-shape indices to determine blade-vs-shaft.

The multiplier is already wired in `Stick.cpp` — `stick.bladeGainMultiplier` is used at line 136 and reset to 1.0 at line 172. The missing piece is the contact listener logic to compute and apply it.

### Implementation

1. In `OnContactPersisted`, check if both bodies are on layer STICK
2. Decode sub-shape IDs to determine which part of each stick is in contact
3. If body1's blade hits body2's shaft (or vice versa), compute the gain multiplier
4. Store it on the appropriate Stick struct

### Complexity note

Sub-shape detection in Jolt compound shapes requires `SubShapeID::GetValue()` decoding. For a `StaticCompoundShape`, sub-shape index 0 = first added shape (shaft), index 1 = second (blade). The contact listener will need access to both Stick structs to write the multiplier.

**Recommendation:** Defer to Phase 10 validation. This only affects stick-on-stick battles. If validation shows divergence in 1v1 stick fights, add it then.

### Values

| Parameter | Value | Source |
|-----------|-------|--------|
| Min proportional gain multiplier | 0.25 | Stick.prefab |
| Clamp range | [0.25, 1.0] | Stick.cs |
| Reset | 1.0 per frame | Stick.cs |

---

## Part H — Struct & Header Changes

### Puck.h additions

```
struct Puck {
    BodyID puckId;
    BodyID triggerId;
    bool isTouchingStick;  // NEW: set by contact listener each frame
    bool isGrounded;       // NEW: set by ground check each frame
};
```

New function declarations:
- `void UpdatePuckPostStickExit(BodyInterface& bi, Puck& puck)` — Y correction + speed clamps
- `void UpdatePuckGroundCheck(BodyInterface& bi, PhysicsSystem& ps, Puck& puck)` — grounded detection + COM shift
- `void UpdatePuckTensor(PhysicsSystem& ps, Puck& puck)` — inertia tensor switching

### Listeners.h additions

The contact listener needs:
- A queue for puck-stick exit events (like netDampingQueue)
- A set/flag for ongoing puck-stick contact (for tensor switching)

---

## Part I — Implementation Order

1. **Puck struct expansion** — Add `isTouchingStick`, `isGrounded` fields
2. **Stick exit handler (Part A + D)** — Contact listener `OnContactRemoved` for puck-stick, queue Y-correction + clamps
3. **Stick contact tracking (Part C)** — Contact listener `OnContactPersisted` for puck-stick, set `isTouchingStick` flag
4. **Inertia tensor switching (Part C)** — Update tensor each frame based on `isTouchingStick`
5. **Grounded COM shift (Part B)** — Sphere overlap check + shape swap (or defer)
6. **Angular velocity transfer fix (Part E)** — Add mass multiplication to stick torque transfer
7. **Verification pass** — Run tests from Part J

Parts B and G can be deferred if complexity is too high for the current phase.

---

## Part J — Testing Plan

### Test 1: Y-Axis Correction

Setup: Give puck upward velocity (0, 5, 0), place on stick collision course
Expected: After stick exit, puck Y velocity reduced by 5x acceleration pulse
Verify: Print puck velocity before/after the correction event

### Test 2: Angular Speed Cap

Setup: Set puck angular velocity to (100, 100, 100) rad/s, trigger stick exit
Expected: Angular velocity magnitude clamped to 60 rad/s
Verify: Print angular velocity after correction

### Test 3: Linear Speed Cap

Setup: Set puck linear velocity to (40, 0, 0), trigger stick exit
Expected: Linear velocity magnitude clamped to 30 m/s
Verify: Print velocity after correction

### Test 4: Inertia Tensor Switching

Setup: Place puck on stick, verify tensor = (0.006, 0.002, 0.006)
Remove from stick, verify tensor = (0.002, 0.002, 0.002)
Verify: Print inertia tensor values each frame during transition

### Test 5: Angular Velocity Transfer Fix

Setup: Give stick high angular velocity, observe player body response
Expected: With mass multiplication, player actually rotates in response
Verify: Compare player angular velocity with and without the fix

### Test 6: Grounded COM (if implemented)

Setup: Place puck on ice, tilt slightly
Expected: Puck self-rights faster with COM shifted down
Verify: Compare settling time with and without COM shift

---

## Part K — Key Gotchas

1. **OnContactRemoved gives SubShapeIDPair, not Body references.** You need `physics_system.GetBodyLockInterface()` or store body IDs from `OnContactAdded` to look them up later.

2. **Contact listener callbacks happen during physics step.** You cannot modify velocities inside callbacks (bodies are locked). Queue events and process them after `physics_system.Update()` — same pattern as net damping.

3. **Inertia tensor changes require body lock or BodyInterface.** Check if `BodyInterface` has a mass properties setter; otherwise use `BodyLockWrite` outside the physics step.

4. **isTouchingStick must be cleared BEFORE physics step, then re-set by contact callbacks.** Clear at start of frame, let `OnContactPersisted` set it during the step, read it after the step.

5. **The puck's prefab initial inertia tensor (0.002, 0.002, 0.01) is NOT the runtime default.** The script's `defaultTensor` field (0.002, 0.002, 0.002) overrides it every FixedUpdate. Use (0.002, 0.002, 0.002) as default.

6. **ForceMode.Acceleration on stick angular transfer = multiply by body mass.** Player mass = 90. Current code is 90x too weak.

7. **Y-correction uses Acceleration, not VelocityChange.** The force is `(0, min(0, -vel.y) * 5, 0) * mass`. This is an impulse-like acceleration applied once on exit, not sustained.

8. **Angular speed cap is 60, not 30.** Prefab overrides the code default. Already documented in physics-values.md.

9. **Grounded COM shift uses TransformVector, not TransformPoint.** The offset (0, -0.01, 0) is rotated by the puck's rotation but NOT translated. It's a direction, not a position.

10. **Shape swap for COM shift may reset velocity.** Test whether `BodyInterface::SetShape()` preserves linear/angular velocity. If not, save and restore them around the swap.
