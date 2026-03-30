# Phase 8 — Player Movement + Keep-Upright

## Overview

Add WASD movement to players. This is the minimum input layer needed for RL curriculum Phase 1. **No sprint, stamina, jump, twist, dash, slide, or tackle** — only forward/backward acceleration, turning, and the supporting systems that make movement work correctly (keep-upright, skate traction, velocity lean, gravity compensation).

### System Dependency Order

The game's `PlayerBodyV2.FixedUpdate()` runs these systems in a specific order each physics tick:

1. **Gravity compensation** — cancel Unity gravity, re-apply at 2× multiplier
2. **Laterality update** — adjust movement direction based on lateral input
3. **Movement.Move()** — apply forward/backward acceleration + drag
4. **Movement.Turn()** — apply turn torque + turn drag
5. **Hover PID** — maintain height above ice (already implemented)
6. **Keep-Upright PID** — prevent player from tipping over
7. **Skate traction** — cancel lateral drift
8. **Velocity lean** — cosmetic body tilt into movement direction

All systems run every `FixedUpdate` (50 Hz, matching our sim tick rate).

---

## Part A — Gravity Compensation

### The Problem

The current sim sets `mGravityFactor = 2.0` on the player body. The game does something different: it **cancels** default gravity entirely, then re-applies it at 2× — but **only while upright**. When the player falls over, normal gravity applies (no compensation, no multiplier).

### Game Algorithm (PlayerBodyV2.cs)

```
if (IsUpright):
    AddForce(Vector3.up * -Physics.gravity.y, ForceMode.Acceleration)   // cancel gravity
    AddForce(Vector3.down * -Physics.gravity.y * gravityMultiplier, ForceMode.Acceleration)  // re-apply at 2x
```

With `Physics.gravity.y = -9.81` and `gravityMultiplier = 2.0`:
- Cancel force: `+9.81` (upward)
- Re-apply force: `-9.81 * 2.0 = -19.62` (downward)
- Net effect while upright: `-9.81` extra downward (total `-19.62`)
- When fallen: just normal `-9.81`

### What This Means for Jolt

Currently we set `mGravityFactor = 2.0` unconditionally. This is correct while upright, but wrong when fallen — a fallen player should experience normal gravity, not doubled.

For Phase 8 curriculum (WASD only, no tackles), players should rarely fall, so this is low priority. But to be correct:

- Track an `isUpright` flag (dot product of body's up vector vs world up > 0.8)
- When upright: `mGravityFactor = 2.0` (current behavior, correct)
- When fallen: dynamically set `mGravityFactor = 1.0`

Alternatively, set `mGravityFactor = 1.0` and manually apply the extra gravity force each tick only when upright. This matches the game's approach more closely.

### Values

| Parameter | Value | Source |
|-----------|-------|--------|
| Gravity Multiplier | 2.0 | Prefab |
| Upwardness Threshold | 0.8 | Prefab |

---

## Part B — Forward/Backward Movement

### Values (All from Prefab — Authoritative)

| Parameter | Attacker | Goalie | Notes |
|-----------|----------|--------|-------|
| Forward Acceleration | 2.0 | 2.0 | Same for both roles |
| Backward Acceleration | 1.8 | 1.8 | Same for both roles |
| Brake Acceleration | 5.0 | 5.0 | Applied when input opposes current direction |
| Max Forward Speed | 7.5 | 5.0 | |
| Max Backward Speed | 7.5 | 5.0 | Prefab overrides .cs default of 7.25 |
| Drag | 0.015 | 0.015 | Prefab overrides .cs default of 0.025 |
| Overspeed Drag | 0.25 | 0.25 | Prefab overrides .cs default of 0.025 (10× higher!) |

**Sprint values exist but are NOT used in Phase 8.** Listed here for reference only:

| Parameter | Attacker | Goalie |
|-----------|----------|--------|
| Forward Sprint Accel | 4.75 | 4.75 |
| Max Forward Sprint Speed | 8.75 | 6.25 |
| Backward Sprint Accel | 2.0 | 2.0 |
| Max Backward Sprint Speed | 8.5 | 6.0 |

### Algorithm (Movement.cs Move())

**Order of operations each tick:**

1. **Check grounded** — skip movement if `!Hover.IsGrounded`
2. **Determine direction** — is the player trying to move forward or backward?
3. **Check if opposing current motion** — if input direction opposes current velocity, use `brakeAcceleration` instead
4. **Apply acceleration** — `AddForce(MovementDirection.forward * accel, ForceMode.Acceleration)`
   - Only if `Speed < maxSpeed` for the current direction
   - If already at/above max speed: apply 0 force (coast, let drag handle it)
5. **Apply drag** — multiplicative velocity reduction:
   - If `Speed > maxSpeed`: `velocity *= (1 - overspeedDrag * dt)`
   - Else: `velocity *= (1 - drag * dt)`
6. **Apply ambient drag** — additional multiplicative drag stacked on top:
   - `velocity *= (1 - AmbientDrag * dt)`
   - AmbientDrag = 0 during normal movement (only non-zero during slide, stop, fall — not used in Phase 8)

**Key details:**
- `Speed` = horizontal speed only: `sqrt(vx² + vz²)` (Y component excluded)
- Speed cap is **soft** — acceleration withheld, not velocity clamped
- Drag is **multiplicative** per frame, not subtractive
- `ForceMode.Acceleration` = mass-independent. In Jolt: `AddForce(force * mass)` or use the acceleration overload if available

### Jolt Mapping: ForceMode.Acceleration

Unity's `ForceMode.Acceleration` applies `force = acceleration` (ignores mass). Jolt's `BodyInterface::AddForce` applies `F = force` (mass-dependent). To replicate:

```
// Unity: Rigidbody.AddForce(dir * accel, ForceMode.Acceleration)
// Jolt equivalent:
bi.AddForce(bodyId, dir * accel * mass);
```

This is how the hover PID already works in our codebase.

### Jolt Mapping: Direct Velocity Modification

Unity directly sets `Rigidbody.linearVelocity` for drag. In Jolt you can use `SetLinearVelocity` but this bypasses the physics solver. Since drag is applied after forces each tick, this should be fine — just get velocity, multiply, set it back:

```
// Unity: Rigidbody.linearVelocity *= (1 - drag * dt)
// Jolt:
Vec3 vel = bi.GetLinearVelocity(bodyId);
vel *= (1.0f - drag * dt);
bi.SetLinearVelocity(bodyId, vel);
```

**Important:** Only apply drag to horizontal components (X, Z). The Y component is controlled by hover + gravity.

Actually — the game applies drag to the full velocity vector (`linearVelocity *= factor`), including Y. This interacts with the hover PID which compensates. Replicate exactly: drag the full vector.

### Movement Direction

Forces are applied along `MovementDirection.forward`, which is the player's forward vector rotated by `Laterality`. See Part F for laterality details. For initial implementation, just use the body's forward direction.

---

## Part C — Turning

### Values (All from Prefab — Authoritative)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Turn Acceleration | 1.5 | Prefab overrides .cs default of 1.625 |
| Turn Brake Acceleration | 4.5 | Prefab overrides .cs default of 3.25 |
| Max Turn Speed | 1.3125 | Prefab overrides .cs default of 1.375 |
| Turn Drag | 3.0 | Same in code and prefab |
| Turn Overspeed Drag | 2.25 | Same in code and prefab |

### Algorithm (Movement.cs Turn())

1. **Measure turn speed** — local-space Y angular velocity:
   ```
   turnSpeed = abs(InverseTransformVector(angularVelocity).y)
   ```
   In Jolt: transform world angular velocity to body-local, take Y component.

2. **Apply turn torque** based on input:
   - If turning in **same direction** as current angular velocity AND `turnSpeed < maxTurnSpeed`:
     - `AddTorque(Y_axis * turnAcceleration * inputSign, ForceMode.Acceleration)`
   - If turning **opposite** to current angular velocity:
     - `AddTorque(Y_axis * turnBrakeAcceleration * inputSign, ForceMode.Acceleration)`
   - If **no turn input**: no torque applied, drag handles deceleration

3. **Apply turn drag**:
   - If `turnSpeed > maxTurnSpeed`: `angularVelocity *= (1 - turnOverspeedDrag * dt)`
   - Else: `angularVelocity *= (1 - turnDrag * dt)`

**Key details:**
- Torque is around **world Y axis** (up), not body-local Y
- `ForceMode.Acceleration` for torque = mass/inertia-independent. In Jolt: multiply by inertia or use equivalent
- Turn multiplier = 1.0 for normal movement (2.0 sliding, 5.0 jumping — not used in Phase 8)
- Angular velocity drag is applied to the full angular velocity vector, not just Y

### Jolt Mapping: ForceMode.Acceleration for Torque

Unity's `AddTorque(torque, ForceMode.Acceleration)` applies angular acceleration independent of inertia. In Jolt, `AddTorque(torque)` applies torque that depends on inertia. To replicate:

```
// Unity: Rigidbody.AddTorque(axis * accel, ForceMode.Acceleration)
// Jolt: need to multiply by inertia tensor
// Simpler approach: compute desired angular velocity change and use it
```

This requires careful handling. Options:
1. Get the body's inertia tensor and multiply: `AddTorque(axis * accel * inertia)`
2. Compute angular velocity delta directly and use `SetAngularVelocity`
3. Use `AddAngularImpulse` with appropriate scaling: `impulse = inertia * accel * dt`

Option 2 is simplest for turn acceleration since the game effectively does `angularVelocity.y += turnAcceleration * dt * inputSign`:

```
Vec3 angVel = bi.GetAngularVelocity(bodyId);
angVel += Vec3(0, turnAcceleration * dt * inputSign, 0);
bi.SetAngularVelocity(bodyId, angVel);
```

This is equivalent to ForceMode.Acceleration applied for one frame. Use this approach.

---

## Part D — Keep-Upright PID

### Values (All from Prefab — Authoritative)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Proportional Gain | 50.0 | Same in code and prefab |
| Integral Gain | 0.0 | Disabled |
| Derivative Gain | 8.0 | Prefab overrides .cs default of 5.0 |
| Derivative Mode | Velocity (measurement) | NOT derivative-on-error |

### Algorithm (KeepUpright.cs + Vector3PIDController.cs)

**Error measurement:**
```
currentValue = transform.up    // body's current up direction (unit vector)
targetValue  = Vector3.up      // world up (0, 1, 0)
error = targetValue - currentValue
```

**PID calculation (Vector3PIDController — derivative-on-measurement):**
```
P_term = proportionalGain * Balance * error
I_term = 0  (integral disabled)

valueDerivative = (currentValue - lastValue) / dt
D_term = derivativeGain * Balance * (-valueDerivative)   // negate = counteract motion

output = P_term + D_term
```

**Convert PID output to torque:**
```
torqueAxis = Cross(output, Vector3.up)
Rigidbody.AddTorque(-torqueAxis, ForceMode.Acceleration)
```

The cross product with world-up creates a torque axis perpendicular to both the correction vector and the vertical. The negation ensures the torque rotates the body toward upright.

**Balance factor:**
- Multiplies all PID gains (0.0 to 1.0)
- 1.0 when upright, decreases over `balanceLossTime` (0.25s) when falling
- Recovers over `balanceRecoveryTime` (5.0s) when upright again
- For Phase 8 (no tackles): balance should stay at 1.0, but implement the mechanism for correctness

### Jolt Mapping

Same issue as turning: `ForceMode.Acceleration` for torque means inertia-independent.

The simplest correct approach: compute the angular velocity change directly.

```
// torqueAxis = Cross(pidOutput, Vec3(0,1,0))
// angularAcceleration = -torqueAxis  (ForceMode.Acceleration)
// Apply as angular velocity delta:
Vec3 angVel = bi.GetAngularVelocity(bodyId);
angVel += -torqueAxis * dt;
bi.SetAngularVelocity(bodyId, angVel);
```

Alternatively, multiply by the inverse inertia tensor and use `AddTorque`. The direct velocity approach is simpler and matches Unity's ForceMode.Acceleration semantics exactly.

### State to Track

| Field | Type | Purpose |
|-------|------|---------|
| `prevUp` | `Vec3` | Previous frame's body up vector (for derivative-on-measurement) |
| `balance` | `float` | Current balance factor (0.0–1.0) |

---

## Part E — Skate Traction

### Values (All from Prefab — Authoritative)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Traction | 12.5 | Prefab overrides .cs default of 0.15 (83× higher!) |

### Algorithm (Skate.cs FixedUpdate)

**Purpose:** Prevent lateral drift. Players should only move in the direction they're facing, not slide sideways. This replaces traditional friction with an active correction system.

1. **Extract lateral velocity in movement-local space:**
   ```
   localVel = InverseTransformVector(linearVelocity)   // to movement frame
   lateralVelocity = -localVel.x   // rightward drift (negated)
   ```

2. **Clamp correction to traction limit:**
   ```
   correction = Clamp(lateralVelocity, -traction * dt, traction * dt)
   ```

3. **Apply correction as VelocityChange:**
   ```
   AddForce(MovementDirection.right * correction * Intensity, ForceMode.VelocityChange)
   ```

4. **Intensity modulation:**
   ```
   Intensity = (IsSliding || IsStopping || !IsGrounded) ? 0.0 : KeepUpright.Balance
   ```
   For Phase 8 (no sliding, always grounded): `Intensity = balance` (typically 1.0)

### Key Detail: ForceMode.VelocityChange

This is the ONE system that uses `VelocityChange` instead of `Acceleration`. It directly modifies velocity, bypassing mass entirely. In Jolt:

```
// Unity: AddForce(dir * correction, ForceMode.VelocityChange)
// Jolt: directly modify velocity
Vec3 vel = bi.GetLinearVelocity(bodyId);
vel += movementRight * correction * intensity;
bi.SetLinearVelocity(bodyId, vel);
```

### Why This Matters

With `traction = 12.5` and `dt = 0.02`, the max lateral correction per frame is `12.5 * 0.02 = 0.25 m/s`. This means any lateral drift up to 0.25 m/s is fully corrected each frame. Above that, traction is "lost" — the `IsTractionLost` flag gets set (used for visual effects in the game, not physics).

---

## Part F — Laterality (Movement Direction)

### Values (All from Prefab — Authoritative)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Min Laterality | 0.5 | Minimum body lean at high speed |
| Max Laterality | 1.0 | Maximum body lean at low speed |
| Min Laterality Speed | 2.0 | Lerp rate at low speed |
| Max Laterality Speed | 5.0 | Lerp rate at high speed |

### What Laterality Does

Laterality rotates the "movement direction" away from the body's true forward direction. When a player holds left/right, their movement direction shifts to the side, causing them to strafe rather than just turn. The body continues facing roughly the same direction while movement angles sideways.

### Algorithm (PlayerBodyV2.cs)

1. **Compute speed-dependent parameters:**
   ```
   NormalizedMinimumSpeed = Speed / MinimumSpeed   // 0 at rest, 1 at min speed cap
   t = Clamp01(1.0 - NormalizedMinimumSpeed)       // 1 at rest, 0 at speed cap
   targetSpeed = Lerp(minLateralitySpeed, maxLateralitySpeed, t)   // faster lean at low speed
   targetLaterality = Lerp(minLaterality, maxLaterality, t)        // more lean at low speed
   ```

2. **Update laterality value toward target:**
   ```
   if (leftInput):
       Laterality = Lerp(Laterality, -targetLaterality, dt * targetSpeed)
   elif (rightInput):
       Laterality = Lerp(Laterality, +targetLaterality, dt * targetSpeed)
   else:
       Laterality = Lerp(Laterality, 0, dt * targetSpeed)   // return to center
   ```

3. **Rotate movement direction:**
   ```
   MovementDirection.rotation = FromToRotation(
       forward,
       Slerp3(-right, forward, right, Laterality)   // -1=left, 0=forward, +1=right
   )
   ```

### Implementation Note

`Laterality` creates a separate "movement frame" that differs from the body orientation. All movement forces and skate corrections use this frame, not the body's actual facing direction.

For **initial** implementation, you can skip laterality and use the body's forward/right vectors directly. This simplifies Part B, C, and E. Add laterality as a refinement once basic movement works.

### State to Track

| Field | Type | Purpose |
|-------|------|---------|
| `laterality` | `float` | Current lean value (-1 to +1) |

---

## Part G — Velocity Lean

### Values (All from Prefab — Authoritative)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Linear Force Multiplier | 0.75 | Prefab overrides .cs default of 1.0 |
| Angular Force Multiplier | 6.0 | Same in code and prefab |

### Algorithm (VelocityLean.cs FixedUpdate)

**Purpose:** Apply torque that tilts the body into the direction of movement and rotation. Cosmetic effect that makes movement look natural, but also interacts with keep-upright PID.

1. **Measure velocities in movement-local space:**
   ```
   linearVelocity = localForwardVelocity   // forward component only (when grounded)
   angularVelocity = localYAngularVelocity  // turn rate around up axis
   ```

2. **Apply roll torque from linear velocity:**
   ```
   // Moving forward → lean right (into the right side)
   AddTorque(right * linearVelocity * linearForceMultiplier * LinearIntensity, ForceMode.Acceleration)
   ```

3. **Apply pitch torque from angular velocity:**
   ```
   // Turning right → lean forward
   AddTorque(-forward * angularVelocity * angularForceMultiplier * AngularIntensity, ForceMode.Acceleration)
   ```

4. **Intensity modulation:**
   ```
   LinearIntensity = Max(0.1, NormalizedMaxSpeed)   // scales with speed, min 0.1
   AngularIntensity = same value, divided by 2 if sliding/jumping
   ```

### Implementation Priority

Velocity lean is **lower priority** than the other systems. It affects visual tilt but doesn't directly determine movement accuracy. The keep-upright PID will fight the lean torque, finding equilibrium.

Implement it, but if the player is stable with just keep-upright + skate, this can be a refinement pass.

### Inverted Flag

When moving backward, the lean direction reverses (set `Inverted = true`). This flips the `right` vector used for roll torque.

---

## Part H — PlayerParams / Player Struct Updates

### New Fields in PlayerParams

```
// Add to existing PlayerParams:
float forwardAcceleration;      // 2.0
float backwardAcceleration;     // 1.8
float brakeAcceleration;        // 5.0
float drag;                     // 0.015
float overspeedDrag;            // 0.25

float turnAcceleration;         // 1.5
float turnBrakeAcceleration;    // 4.5
float maxTurnSpeed;             // 1.3125
float turnDrag;                 // 3.0
float turnOverspeedDrag;        // 2.25

float keepUprightPGain;         // 50.0
float keepUprightDGain;         // 8.0

float skateTraction;            // 12.5

float velocityLeanLinearMult;   // 0.75
float velocityLeanAngularMult;  // 6.0

float minLaterality;            // 0.5
float maxLaterality;            // 1.0
float minLateralitySpeed;       // 2.0
float maxLateralitySpeed;       // 5.0
```

All values are **identical** between Attacker and Goalie except speed caps (already in PlayerParams). Consider whether movement params should live in PlayerParams (per-role) or as global constants.

### New Fields in Player

```
// Add to existing Player struct:
Vec3  prevUp;           // previous frame body up vector (for keep-upright D term)
float balance;          // keep-upright balance factor (0-1, starts at 1)
float laterality;       // current lateral lean (-1 to +1, starts at 0)
```

### Fields to Potentially Remove from PlayerParams

Currently `PlayerParams` has sprint/dash/slide fields that aren't used in Phase 8:
- `maxForwardSprintSpeed`, `maxBackwardSprintSpeed` — keep for future
- `canDash`, `slideDrag`, `tackleSpeedThreshold`, `tackleForceThreshold` — keep for future

No need to remove them, but don't add logic for them.

---

## Part I — Implementation Order

### Step 1: Expand PlayerParams + Player struct
Add new fields, initialize in ATTACKER_PARAMS / GOALIE_PARAMS.

### Step 2: Keep-Upright PID
Implement first because movement torques will tip the player over without it. Test that the player stays vertical when pushed.

### Step 3: Forward/Backward Movement
Apply acceleration forces along body forward direction. Apply drag. Verify speed caps work (player reaches ~7.5 m/s and holds steady).

### Step 4: Turning
Apply turn torque around Y axis. Apply turn drag. Verify max turn speed is respected.

### Step 5: Skate Traction
Apply lateral drift correction. Without this, turning at speed will cause the player to drift sideways like on ice (ironic, since they ARE on ice, but the game uses active traction correction).

### Step 6: Velocity Lean
Add cosmetic body tilt. Verify it doesn't destabilize the keep-upright PID.

### Step 7: Laterality
Add movement direction rotation from left/right input. This changes how movement forces and skate corrections are oriented relative to the body.

### Step 8: Gravity Compensation Refinement
Make gravity multiplier conditional on upright state (optional for Phase 8, since players shouldn't fall).

### Step 9: Input System
Define an input struct that the sim loop accepts each tick. For RL, this will come from the agent; for testing, hardcode sequences.

```
struct PlayerInput {
    float forward;   // -1 (backward) to +1 (forward)
    float turn;      // -1 (left) to +1 (right)
    float lateral;   // -1 (left) to +1 (right)  — for laterality
};
```

---

## Part J — Testing Plan

### Test 1: Keep-Upright Stability
- Push player with an impulse from the side
- Player should rock but return to vertical
- Oscillation should damp within ~1 second

### Test 2: Forward Acceleration + Speed Cap
- Apply forward input for 200 ticks
- Verify speed ramps from 0 toward 7.5 m/s (attacker)
- Verify speed stabilizes near max (drag + withheld acceleration = equilibrium)
- Print speed each tick to verify curve shape

### Test 3: Braking
- Get player to max speed, then apply backward input
- Brake acceleration (5.0) should decelerate faster than backward acceleration (1.8)
- Player should stop, then begin moving backward

### Test 4: Overspeed Drag
- Give player velocity above max speed via impulse
- Verify overspeed drag (0.25) brings speed down faster than normal drag (0.015)
- Speed should drop quickly to below max, then hold at max with forward input

### Test 5: Turning
- Apply turn input, measure angular velocity
- Should reach max turn speed (1.3125 rad/s) and hold
- Release turn input — turn drag should slow rotation

### Test 6: Traction
- Move player forward, then apply a lateral impulse
- Lateral velocity should be corrected within a few frames
- Player should continue moving forward, not drift sideways

### Test 7: Combined Movement
- Move forward while turning
- Player should follow a curved path, not drift tangentially
- Skate traction should prevent the "ice skating" drift

---

## Part K — Hover PID Corrections

The hover system is already implemented but has minor discrepancies from the game:

### Current Values (verify these match)

| Parameter | Current | Correct (Prefab) | Status |
|-----------|---------|-------------------|--------|
| Proportional Gain | 100.0 | 100.0 | OK |
| Derivative Gain | 8.0 | 8.0 | OK |
| Max Force | 40.0 | 40.0 | OK |
| Target Distance | 1.2 | 1.2 | OK |
| Raycast Distance | 1.3 | 1.3 | OK |
| Raycast Offset | (0, 1, 0) | (0, 1, 0) | OK |

### Implementation Note

The current hover PID uses derivative-on-measurement (measuring velocity of distance change, then negating). This matches the game's `Vector3PIDController` with `derivativeMeasurement = Velocity` mode. The existing implementation appears correct.

### Hover Interaction with Movement

The hover PID operates on the Y axis only (height above ice). Movement drag affects all 3 axes including Y, which slightly interferes with hover. The hover PID compensates for this naturally — the same interaction happens in the game. No special handling needed.

---

## Part L — Key Gotchas

1. **ForceMode mapping is critical.** Movement, turning, keep-upright, and velocity lean all use `ForceMode.Acceleration` (mass/inertia-independent). Skate traction uses `ForceMode.VelocityChange` (direct velocity modification). Getting these wrong will produce fundamentally different dynamics.

2. **Drag applies to full velocity vector.** Don't strip Y before applying drag — the hover PID compensates. Stripping Y would create different hover behavior.

3. **Speed measurement is horizontal only.** `Speed = sqrt(vx² + vz²)`. Y velocity is excluded for speed cap checks, but NOT excluded from drag application.

4. **Turn torque is around WORLD Y axis.** Not body-local Y. This means turning always rotates around the vertical regardless of body tilt.

5. **Order of operations matters.** Forces first, then drag. If reversed, drag reduces velocity before acceleration is applied, changing the equilibrium speed.

6. **Skate traction operates in the MOVEMENT frame** (rotated by laterality), not the body frame. If laterality is 0, these are the same. Once laterality is added, the distinction matters.

7. **Keep-upright derivative smoothing.** The game's `Vector3PIDController` has a `derivativeSmoothing` parameter (lerp between previous and current derivative). Check if this is set to 1.0 (no smoothing) or something else in the prefab. If smoothing is applied, add a lerp to the D term.

8. **Balance affects multiple systems.** Keep-upright gains, skate intensity, and velocity lean intensity all multiply by `balance`. When balance drops (player tipping), all three weaken simultaneously, creating a cascading fall effect.

9. **AddTorque with ForceMode.Acceleration in Jolt.** Jolt doesn't have a direct equivalent. The cleanest approach is to compute angular velocity change directly (`angVel += angularAcceleration * dt`) since `ForceMode.Acceleration` means the torque IS the angular acceleration. This avoids needing to deal with inertia tensors.

10. **Movement acceleration is withheld, not clamped.** The game does NOT clamp velocity to max speed. It stops applying acceleration when speed exceeds max. Combined with very low drag (0.015), the player can briefly exceed max speed from external forces and slowly coast down.
