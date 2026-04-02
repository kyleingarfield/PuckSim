# Phase 11 — Sim2GameTest Discrepancy Fixes

## Purpose

This guide documents every physics discrepancy found by comparing Unity (PhysX) telemetry against PuckSim (Jolt) telemetry using the Sim2GameTest validation pipeline. Each fix is prioritized by impact, includes root cause analysis, the exact PhysX behavior to match, the PuckSim code to change, and the expected improvement.

Run `compare.py --all --plot` after each fix to measure progress.

---

## Baseline Error (before any fixes)

| Scenario | Position RMSE | Max Dev | Primary Axis |
|----------|--------------|---------|--------------|
| puck_free_slide | 3.55 m | 4.77 m | X=3.05, Z=1.83 |
| puck_wall_bounce | 4.43 m | 5.63 m | X=4.42 |
| player_forward | 14.58 m | 24.22 m | Z=14.58 |
| player_turn | 6.51 m | 10.55 m | X=5.18, Z=3.95 |
| player_lateral | 16.08 m | 28.47 m | X=14.86, Z=6.16 |
| player_hover | 0.38 m | 1.82 m | Y=0.38 |

---

## Fix 1: Player Forward Direction (CRITICAL)

### Root Cause

Jolt uses a **right-handed** coordinate system. Unity uses **left-handed**. Both are Y-up. The critical difference: for the same quaternion values, `rotation * Vec3(0, 0, 1)` points in **opposite world directions** between the two systems.

In Unity, `transform.forward` at `Quaternion.identity` is `(0, 0, +1)`. In Jolt, the same quaternion identity with `Vec3(0, 0, 1)` gives `+Z`, but because the handedness is flipped, the physical "forward" direction that matches Unity's forward is actually `Vec3(0, 0, -1)` in Jolt when using the same quaternion values.

PuckSim stores the same quaternion float values as Unity (captured from prefabs and telemetry). Since the quaternion values are preserved, the rotation-to-direction mapping must account for the handedness flip.

### Evidence

The `player_forward` scenario places the player at identity rotation and applies `forward=1.0` input. Unity's player accelerates along +Z (reaching Z~35 by frame 500). PuckSim's player accelerates in the opposite Z direction. The position error is 14.58m RMSE, almost entirely on the Z axis (14.5757 vs X=0.0013).

### What Unity Does

`Movement.cs` line 197:
```csharp
this.Rigidbody.AddForce(this.MovementDirection.forward * d, ForceMode.Acceleration);
```
`MovementDirection.forward` is the child transform's `+Z` in world space. At identity rotation with laterality=0, this is `(0, 0, +1)`.

### What PuckSim Does Now

`Player.cpp` line 188:
```cpp
Vec3 forward = (rot * lateralRot) * Vec3(0, 0, 1);
```

### Fix

Change `Vec3(0, 0, 1)` to `Vec3(0, 0, -1)` everywhere a "forward" direction is derived from a Unity-sourced quaternion. Affected locations in `Player.cpp`:

| Line | Function | Current | Fix |
|------|----------|---------|-----|
| 188 | `UpdateMovement` | `Vec3(0, 0, 1)` | `Vec3(0, 0, -1)` |
| 284 | `UpdateVelocityLean` | `Vec3(0, 0, 1)` (worldForward) | `Vec3(0, 0, -1)` |

The `worldRight` direction (`Vec3(1, 0, 0)`) also needs checking. In a right-handed system, if forward is `-Z`, then right should be `+X` (cross product of up `+Y` and forward `-Z` in right-handed = `+X`). In Unity (left-handed), forward `+Z` cross up `+Y` = `+X`. Both give `+X` for right. **So `Vec3(1, 0, 0)` for right is correct and does not need changing.**

### Laterality Rotation Direction

`Player.cpp` line 187:
```cpp
Quat lateralRot = Quat::sRotation(Vec3(0, 1, 0), -player.laterality * 0.5f * JPH_PI);
```

In Unity (left-handed), a positive Y-axis rotation is clockwise from above. `Laterality = +1` rotates the movement direction 90 degrees to the right (clockwise from above), which is a positive Y rotation.

In Jolt (right-handed), a positive Y-axis rotation is **counter-clockwise** from above. To get a clockwise rotation (= right), we need a **negative** Y rotation.

With the current code: `laterality=+1` gives angle = `-1 * 0.5 * PI = -PI/2`, which in Jolt (right-handed) is a clockwise rotation (right). **This is correct.**

But the forward vector it operates on is wrong (`+Z` instead of `-Z`). Once forward is fixed to `-Z`, we need to re-verify: rotating `-Z` by `-PI/2` around Y in right-handed gives... `+X`? No, it should give `-X` to go "right" relative to a `-Z` forward.

Actually, let's think about this more carefully. The laterality rotation is applied as `rot * lateralRot`, which rotates the local forward. In Unity:
- Local forward before laterality = `(0, 0, 1)`
- Laterality = +1: rotate +90 deg around local Y (positive = clockwise in left-handed) → local forward becomes `(1, 0, 0)` → world right

In Jolt with forward = `(0, 0, -1)`:
- Laterality = +1: lateralRot angle = `-PI/2`, in right-handed this is clockwise from above
- `Quat::sRotation(Y, -PI/2) * Vec3(0, 0, -1)`:
  - Rotating `-Z` clockwise by 90 deg around Y → result is `(-1, 0, 0)` → world **left**, not right

**This is wrong.** We need to also flip the laterality rotation sign. Change to:
```cpp
Quat lateralRot = Quat::sRotation(Vec3(0, 1, 0), player.laterality * 0.5f * JPH_PI);
```

With `player.laterality = +1`, angle = `+PI/2` (counter-clockwise in right-handed, i.e., leftward from above... wait). Let's just compute directly.

`Quat::sRotation(Vec3(0,1,0), +PI/2)` in right-handed = counter-clockwise 90 deg from above.
Applied to `Vec3(0, 0, -1)`:
- `-Z` rotated CCW 90 deg from above → `(+1, 0, 0)` → world right. **Correct.**

So the fix is: **remove the negation** on the laterality angle:
```cpp
Quat lateralRot = Quat::sRotation(Vec3(0, 1, 0), player.laterality * 0.5f * JPH_PI);
```

This same laterality rotation appears in `UpdateSkate` at line 258. Apply the same fix there.

### Turn Direction

`Player.cpp` line 236:
```cpp
angVel += Vec3(0, turnAccel * dt, 0);
```

In Unity, positive Y angular velocity = clockwise from above = turn right. `input.turn > 0` means turn right, so `turnAccel > 0`.

In Jolt (right-handed), positive Y angular velocity = **counter-clockwise** from above = turn **left**.

**Fix:** Negate the Y component:
```cpp
angVel += Vec3(0, -turnAccel * dt, 0);
```

Also check the turn drag logic. The `localAngVel.GetY()` sign interpretation in `sameDirection` check (line 224) depends on whether local Y angular velocity has the same sign convention. Since we're only negating the world-space application but the `sameDirection` check uses local angular velocity dotted against input sign, this should still work correctly — but verify after the fix.

### VelocityLean Adjustments

`Player.cpp` lines 283-293:
```cpp
Vec3 worldRight = rot * Vec3(1, 0, 0);       // correct, no change needed
Vec3 worldForward = rot * Vec3(0, 0, 1);     // FIX: change to Vec3(0, 0, -1)

// forwardSpeed = localVel.GetZ() at line 277
// In Jolt with forward=-Z, "forward speed" is actually -localZ
// FIX: forwardSpeed = -localVel.GetZ()

Vec3 rollTorque = worldRight * forwardSpeed * ...;
Vec3 pitchTorque = -worldForward * turnRate * ...;
```

The `forwardSpeed` at line 277 also needs fixing:
```cpp
float forwardSpeed = localVel.GetZ();  // FIX: negate to -localVel.GetZ()
```

And `turnRate` at line 281:
```cpp
float turnRate = localAngVel.GetY();  // FIX: negate to -localAngVel.GetY()
```

### Skate Traction

`Player.cpp` line 262:
```cpp
float lateralDrift = -localVel.GetX();
```

Local X is the right direction. In Unity, `localVel.X > 0` means drifting right, so the correction is `-localVel.X` (push left). In Jolt with the corrected laterality rotation, local X should still correspond to world right. **However**, since the lateralRot sign is being changed, the local frame definition changes. After fixing the lateralRot sign, verify that `localVel.GetX() > 0` still means rightward drift.

With the fixed `lateralRot` (positive angle for positive laterality):
- `movementRot = rot * lateralRot`
- `localVel = movementRot.Conjugated() * vel`
- The local X axis of `movementRot` is `movementRot * Vec3(1,0,0)`

At identity rotation, laterality=0: `movementRot = identity`, local X = world `+X` = right. `localVel.X > 0` = moving right = drift right. Correction `= -localVel.X < 0` applied along `worldRight (+X)` → pushes left. **Correct, no change needed for lateralDrift.**

### Summary of All Forward-Direction Changes

| File | Line | Change |
|------|------|--------|
| Player.cpp:187 | lateralRot angle | Remove negation: `player.laterality * 0.5f * JPH_PI` |
| Player.cpp:188 | forward vector | `Vec3(0, 0, -1)` |
| Player.cpp:236 | turn angular vel | Negate: `Vec3(0, -turnAccel * dt, 0)` |
| Player.cpp:258 | lateralRot in UpdateSkate | Remove negation: `player.laterality * 0.5f * JPH_PI` |
| Player.cpp:277 | forwardSpeed | Negate: `-localVel.GetZ()` |
| Player.cpp:281 | turnRate | Negate: `-localAngVel.GetY()` |
| Player.cpp:284 | worldForward | `Vec3(0, 0, -1)` |

### Expected Improvement

This should dramatically reduce errors on `player_forward` (Z-axis error should drop from 14.58m to near-zero), `player_turn` (rotation RMSE from 112 deg to much lower), and `player_lateral` (both X and Z errors should collapse). These three scenarios should improve by an order of magnitude.

---

## Fix 2: Jolt Bounce Threshold (HIGH)

### Root Cause

Jolt's `PhysicsSettings::mMinVelocityForRestitution` defaults to **1.0 m/s**. Unity's `DynamicsManager.m_BounceThreshold` is **0.1 m/s**. Any collision with normal relative velocity between 0.1 and 1.0 m/s bounces in Unity but is treated as perfectly inelastic (zero restitution) in Jolt.

This affects every scenario where the puck contacts boards at moderate speed or at a shallow angle (where the normal velocity component is small even if tangential speed is high).

### Evidence

The `puck_wall_bounce` scenario shows the puck hitting the boards at `(15, 0.075, 0)` with velocity `(10, 0, 0)`. At the curved oval barrier, the contact normal may not be purely along X — a glancing hit would reduce the normal velocity component. After the first bounce, subsequent softer contacts would fall below 1.0 m/s normal velocity and lose all restitution in Jolt but retain it in Unity.

### What Unity Does

`DynamicsManager.asset`:
```
m_BounceThreshold: 0.1
```
Contacts with normal relative velocity >= 0.1 m/s apply the combined restitution (0.2 for puck-boards). Below 0.1 m/s, restitution is zero.

### What PuckSim Does Now

Never sets `mMinVelocityForRestitution`. Jolt defaults to 1.0 m/s.

### Fix

After `physicsSystem.Init(...)`, override the physics settings:

```cpp
PhysicsSettings settings = physicsSystem.GetPhysicsSettings();
settings.mMinVelocityForRestitution = 0.1f;
physicsSystem.SetPhysicsSettings(settings);
```

Apply in both:
- `PuckSim.cpp` `RunInteractiveDemo()` (after line 49)
- `Scenarios.cpp` `ScenarioWorld` constructor (after line 133)

### Expected Improvement

Should noticeably improve `puck_wall_bounce` — the puck will bounce more accurately at low-to-moderate collision speeds. May also slightly improve other scenarios where the puck or player contacts surfaces at low normal velocities.

---

## Fix 3: Puck Drag — PhysX vs Jolt Application Point (MEDIUM-HIGH)

### Root Cause

Both PhysX and Jolt use the same damping formula: `velocity *= (1 - damping * dt)`. With `drag=0.3` and `dt=0.02`, both apply `velocity *= 0.994` per tick. The formula is identical.

However, the **timing** of when drag is applied within the physics step differs:
- **PhysX**: Applies drag as part of the velocity integration, **before** constraint solving. The drag-reduced velocity enters the solver.
- **Jolt**: Applies drag in `ApplyForceTorqueAndDragInternal()`, which runs during `IntegrateVelocities` at the **start** of the step, before collision detection and constraint solving.

These should produce very similar results for free-sliding objects. The 3.55m RMSE on `puck_free_slide` (a straight-line slide with no collisions until potentially the end) is surprisingly large for identical drag formulas.

### Additional Investigation Needed

The puck slides **further** in PuckSim than in Unity (Sim positions overshoot Unity positions). This means PuckSim applies **less** effective drag. Possible causes:

1. **Jolt's linear damping on the body is bypassed by `SetLinearVelocity()`**: If any PuckSim code calls `SetLinearVelocity()` on the puck after Jolt's internal damping step, it could override the damped velocity with an undamped value. Check if any post-step processing sets puck velocity.

2. **Contact solver energy injection**: Jolt's contact resolution with the ice surface could be adding micro-impulses that PhysX doesn't. The puck rests on ice with gravity pulling it down — each frame the contact solver pushes it back up. Numerical differences in how the tangential velocity is preserved during this correction could affect horizontal speed.

3. **Double-check that no manual drag is applied in Unity beyond `Rigidbody.drag`**: Confirmed — `Puck.cs` `FixedUpdate()` does NOT apply any manual velocity reduction. The drag comes entirely from the Rigidbody property.

### Diagnostic Step

Before attempting a fix, run a **diagnostic**: disable ice completely (or place the puck in free space with gravity off) and compare the drag-only deceleration. If the curves match perfectly in free space, the discrepancy comes from ice contact interaction. If they don't match, there's a fundamental drag application difference.

To do this in PuckSim:
1. Create a variant of `puck_free_slide` that places the puck at Y=5.0 with no Y velocity and gravity override = 0
2. Compare velocity decay over 200 frames
3. If the velocity curves match exactly, the issue is ice contact, not drag

### Potential Fix (if contact-related)

If ice contact is injecting energy, try:
- Setting puck motion quality to `LinearCast` (see Fix 5)
- Increasing Jolt solver iterations for the puck-ice contact
- Adding a custom velocity callback that applies additional damping matching the PhysX solver's energy loss

### Potential Fix (if drag timing related)

Apply a manual pre-step drag correction:
```cpp
// Before physics_system.Update():
Vec3 puckVel = bi.GetLinearVelocity(puck.puckId);
puckVel *= (1.0f - 0.3f * dt);
bi.SetLinearVelocity(puck.puckId, puckVel);
// Then disable Jolt's built-in damping on the puck:
// Set mLinearDamping = 0.0f on the puck body
```

This gives full control over drag timing. Only do this if the diagnostic shows the built-in damping doesn't match.

---

## Fix 4: SoftCollider (MEDIUM)

### Root Cause

Unity's puck has a `SoftCollider` component that casts 4 diagonal rays from the puck's position, each 0.5m long, against a layer mask (presumably the Boards layer). When a ray hits within 0.5m, it applies a repulsion force proportional to penetration depth, using `ForceMode.Acceleration` at the hit position.

PuckSim does not implement this system. Without it, the puck contacts boards more abruptly and with different energy transfer.

### What Unity Does

`SoftCollider.cs`:
```csharp
// 4 rays in diagonal directions from object center
Vector3[] directions = {
    (forward + right).normalized,
    (forward - right).normalized,
    (-forward + right).normalized,
    (-forward - right).normalized
};

// For each ray that hits within 0.5m:
float penetration = distance - hit.distance;    // how much inside the soft zone
float magnitude = Vector3.Cross(hit.normal, dir).magnitude;  // sin(angle)
float scale = 1f - magnitude;                   // ~cos(angle), stronger for head-on
rigidbody.AddForceAtPosition(
    hit.normal * penetration * (force * scale),  // force = 10
    hit.point,
    ForceMode.Acceleration
);
```

Key properties from the prefab:
- `localOrigin`: `Vector3.zero`
- `distance`: 0.5 m
- `force`: 10
- `layerMask`: Boards layer (12)
- `intensity`: 1.0 (unused in code)

### Impact Assessment

For `puck_free_slide` (puck sliding in the middle of the rink), the SoftCollider has **zero effect** — no boards are within 0.5m. So this does NOT explain the free slide discrepancy.

For `puck_wall_bounce`, this has **significant effect** — the puck approaches and contacts the boards, and the soft repulsion cushions the impact, producing a gentler bounce than a hard collision alone.

### Implementation Approach

Add a `UpdatePuckSoftCollider()` function to `Puck.cpp` that:
1. Gets the puck's world position, forward, and right vectors
2. Casts 4 rays using `PhysicsSystem::CastRay()` against the BOARDS broadphase layer
3. For hits within 0.5m, computes the same force formula
4. Applies force via `BodyInterface::AddForce()` with the equivalent of `ForceMode.Acceleration` (force = acceleration * mass, so multiply by puck mass 0.375)

**Important**: `AddForceAtPosition` in Unity applies both a linear force and a torque (because the force is applied off-center). In Jolt, use `BodyInterface::AddForce()` for the linear part and `BodyInterface::AddTorque()` for the torque part, or use the `AddForce(bodyId, force, point)` overload if available.

Call `UpdatePuckSoftCollider()` **before** `physics_system.Update()` each tick, since Unity's `FixedUpdate` runs before PhysX steps.

### Directional Mapping

The 4 ray directions use the puck's `transform.forward` and `transform.right`. Since the puck at identity rotation has forward = `+Z` in Unity = `-Z` in Jolt, and right = `+X` in both:
```cpp
Vec3 forward = rot * Vec3(0, 0, -1);  // Unity forward in Jolt
Vec3 right = rot * Vec3(1, 0, 0);
Vec3 dirs[4] = {
    (forward + right).Normalized(),
    (forward - right).Normalized(),
    (-forward + right).Normalized(),
    (-forward - right).Normalized()
};
```

---

## Fix 5: Puck Motion Quality (MEDIUM)

### Root Cause

Unity's puck uses `CollisionDetectionMode.ContinuousDynamic`. PuckSim's puck uses Jolt's default `Discrete` mode — the `LinearCast` line is commented out.

`Puck.cpp` line 37:
```cpp
//puck_settings.mMotionQuality = EMotionQuality::LinearCast;
```

### Impact

- **Tunneling**: Fast pucks can pass through thin geometry (boards, goal posts) in Discrete mode
- **Contact accuracy**: ContinuousDynamic provides more accurate contact points for fast-moving objects, which affects bounce direction and energy transfer
- **Ice contact**: With Discrete mode, the puck may penetrate the ice slightly each frame before being pushed back, which could affect horizontal velocity through solver artifacts

### Fix

Uncomment the line in `Puck.cpp`:
```cpp
puck_settings.mMotionQuality = EMotionQuality::LinearCast;
```

Note: Unity's `PuckCollisionDetectionModeSwitcher.cs` switches to `ContinuousSpeculative` when the puck is touching a stick, and back to `ContinuousDynamic` otherwise. Jolt doesn't have a direct equivalent to `ContinuousSpeculative`, but `LinearCast` is the closest to `ContinuousDynamic`. Switching modes per-frame in Jolt is possible but adds complexity — start with always-on `LinearCast` and measure the difference.

### Expected Improvement

Should reduce ice-contact artifacts and improve collision accuracy with boards, particularly for `puck_wall_bounce`. May also help with `puck_free_slide` if ice penetration recovery is injecting horizontal energy.

---

## Fix 6: Hover PID Response (LOW-MEDIUM)

### Root Cause

The `player_hover` scenario drops the player from Y=2.0 with no input. The Y-axis position RMSE is 0.38m with a max deviation of 1.82m at frame 25 (during the initial oscillation). The PID oscillation shape is similar but the Sim overshoots more.

Possible causes:
1. **Gravity application timing**: PhysX applies gravity at the start of velocity integration. Jolt applies gravity in `ApplyForceTorqueAndDragInternal()` during `IntegrateVelocities`. The hover PID reads position/velocity and applies a corrective force — if gravity is already partially integrated when the PID reads velocity, the derivative term sees a different value.

2. **Raycast hit distance**: The hover system raycasts downward to find the ice surface. PhysX and Jolt may report slightly different hit distances for the same ray, especially if the ray origin is inside or near the surface of the capsule shape.

3. **ForceMode.Acceleration vs Jolt force application**: Unity's `AddForce(force, ForceMode.Acceleration)` applies `force` as raw acceleration (ignoring mass). PuckSim should apply `force * mass` via `AddForce()` to get the same effect. Verify this is correct.

### What Unity Does

`PlayerBodyV2.cs` hover (line ~250):
```csharp
float error = targetDistance - hitDistance;
float derivative = (hitDistance - prevHitDistance) / dt;
float force = error * pGain - derivative * dGain;
rigidbody.AddForce(Vector3.up * force * gravityMultiplier, ForceMode.Acceleration);
```

Values from prefab:
- `targetDistance` = 1.2
- `raycastDistance` = 1.3
- `pGain` = 50
- `dGain` = 8
- `gravityMultiplier` = 1.25

### Verification Steps

1. Read `Player.cpp` `UpdatePlayerHover()` and confirm the PID formula matches exactly
2. Confirm `ForceMode.Acceleration` mapping: PuckSim should do `bi.AddForce(bodyId, Vec3(0, force * gravityMultiplier * mass, 0))`
3. Check if `prevHoverDist` is initialized correctly on the first frame (Unity initializes it differently than PuckSim might)
4. Check the raycast direction and origin point match between Unity and Jolt

### Expected Improvement

This is the smallest discrepancy. Fixing the PID timing or initialization should bring the max deviation below 0.5m and the RMSE below 0.1m. The hover settles correctly after ~2s in both engines — the difference is mainly in the transient response.

---

## Fix 7: PhysX Solver Iterations (LOW)

### Root Cause

Unity's PhysX uses `Default Solver Iterations: 6` and `Default Solver Velocity Iterations: 1` (from `DynamicsManager.asset`). Jolt uses its own internal iteration counts.

Jolt's `PhysicsSettings` has:
- `mNumVelocitySteps = 10` (default)
- `mNumPositionSteps = 2` (default)

These control the constraint solver convergence. Different iteration counts can affect contact resolution accuracy, penetration recovery, and stacking stability.

### Impact Assessment

Solver iterations primarily affect scenarios with sustained contacts (player hovering on ice, puck resting on ice). They have less effect on free-flight or single-collision scenarios.

### Verification Step

Try adjusting Jolt's solver settings to see if it changes results:
```cpp
PhysicsSettings settings = physicsSystem.GetPhysicsSettings();
settings.mNumVelocitySteps = 6;
settings.mNumPositionSteps = 1;
physicsSystem.SetPhysicsSettings(settings);
```

This is unlikely to have a large effect, but worth testing.

---

## Fix 8: Keep-Upright Derivative Term (LOW)

### Possible Issue

The keep-upright PID uses derivative-on-measurement (not derivative-on-error) to avoid derivative kick. The derivative term reads the angular velocity. If the angular velocity in Jolt and PhysX differs slightly (due to solver differences), the derivative contribution will differ.

This is a second-order effect — the keep-upright system is a stabilizing controller, so small differences in the derivative term cause small differences in oscillation frequency/amplitude, which compound over time in movement scenarios.

### No Fix Needed Now

This will likely improve automatically once Fix 1 (forward direction) is applied, since the keep-upright system will no longer fight against incorrectly-directed movement forces.

---

## Implementation Order

Apply fixes in this order, running `compare.py --all --plot` after each:

| Step | Fix | Files Modified | Expected Impact |
|------|-----|---------------|----------------|
| 1 | Forward direction + turn + laterality (Fix 1) | Player.cpp | player_forward, player_turn, player_lateral: ~10x improvement |
| 2 | Bounce threshold (Fix 2) | PuckSim.cpp, Scenarios.cpp | puck_wall_bounce: moderate improvement |
| 3 | Puck motion quality (Fix 5) | Puck.cpp | puck_wall_bounce, puck_free_slide: small-moderate improvement |
| 4 | SoftCollider (Fix 4) | Puck.cpp (new function), Scenarios.cpp | puck_wall_bounce: moderate improvement near boards |
| 5 | Puck drag diagnostic (Fix 3) | New diagnostic scenario | Identifies remaining puck_free_slide cause |
| 6 | Hover PID (Fix 6) | Player.cpp | player_hover: small improvement |
| 7 | Solver iterations (Fix 7) | PuckSim.cpp, Scenarios.cpp | All: negligible-small |

---

## Diagnostic Scenarios to Add

### puck_free_space_drag

Place puck at `(0, 5, 0)` with velocity `(5, 0, 3)` and **gravity disabled** (or override gravity to zero). No ice contact, no collisions. Run 200 frames. Compare velocity decay curves. If they match exactly, the `puck_free_slide` drag issue is from ice contact, not from the damping formula.

To disable gravity on one body in Jolt: `bodyCreationSettings.mGravityFactor = 0.0f`.

### puck_vertical_drop

Place puck at `(0, 2, 0)` with zero velocity. Let it fall onto ice. Compare Y position over time. This isolates gravity + contact response without any horizontal motion.

---

## Coordinate System Reference

For all future code, use this mapping:

| Concept | Unity (left-handed) | Jolt (right-handed) | Notes |
|---------|-------------------|-------------------|-------|
| Forward (from identity quat) | `Vec3(0, 0, +1)` | `Vec3(0, 0, -1)` | Most critical difference |
| Right | `Vec3(+1, 0, 0)` | `Vec3(+1, 0, 0)` | Same |
| Up | `Vec3(0, +1, 0)` | `Vec3(0, +1, 0)` | Same |
| Positive Y rotation | Clockwise from above | Counter-clockwise from above | Turn signs flip |
| World positions | Same | Same | No position transform needed |
| Quaternion values | Same float values | Same float values | But rotation-to-direction differs |
