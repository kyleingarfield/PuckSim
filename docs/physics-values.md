# Puck Physics Values

Extracted from decompiled source (`~/Developer/PuckMods/Puck/`) and AssetRipper dump of game assets.
Where prefab values differ from .cs defaults, the prefab (runtime) value is authoritative and marked accordingly.

## Simulation Settings

| Parameter | Value | Source |
|-----------|-------|--------|
| Tick Rate | 50 | PhysicsManager.cs |
| Fixed Delta Time | 0.02s (1/50) | PhysicsManager.cs |
| Simulation Mode | Script (manual Physics.Simulate) | PhysicsManager.cs |
| Gravity | (0, -9.81, 0) | DynamicsManager.asset |
| Bounce Threshold | 0.1 | DynamicsManager.asset |
| Sleep Threshold | 0.001 | DynamicsManager.asset |
| Default Contact Offset | 0.001 | DynamicsManager.asset |
| Default Solver Iterations | 6 | DynamicsManager.asset |
| Default Solver Velocity Iterations | 1 | DynamicsManager.asset |
| Max Depenetration Velocity | Infinity | DynamicsManager.asset |
| Max Angular Speed | Infinity | DynamicsManager.asset |
| Auto Sync Transforms | true | DynamicsManager.asset |
| Friction Type | 0 (default) | DynamicsManager.asset |
| Enhanced Determinism | false | DynamicsManager.asset |

## Layers

| Layer | Name |
|-------|------|
| 0 | Default |
| 1 | TransparentFX |
| 2 | Ignore Raycast |
| 3 | Level |
| 4 | Water |
| 5 | UI |
| 6 | Stick |
| 7 | Puck |
| 8 | Player |
| 9 | Player Mesh |
| 10 | PostProcessing |
| 11 | Goal Post |
| 12 | Boards |
| 13 | Ice |
| 14 | Goal Net |
| 15 | Goal Trigger |
| 16 | Player Collider |
| 17 | Net Cloth Collider |
| 18 | Goal Frame |
| 19 | Puck Trigger |

## Layer Collision Matrix

| Layer | Collides With |
|-------|---------------|
| Stick (6) | Stick, Puck |
| Puck (7) | Stick, Puck, Player Mesh, Goal Post, Boards, Ice, Goal Net, Goal Frame |
| Player (8) | Player, Goal Post, Boards, Ice, Goal Net, Player Collider |
| Player Mesh (9) | Puck |
| Goal Post (11) | Puck, Player |
| Boards (12) | Puck, Player |
| Ice (13) | Puck, Player |
| Goal Net (14) | Puck, Player |
| Goal Trigger (15) | Puck Trigger |
| Player Collider (16) | Player |
| Goal Frame (18) | Puck |
| Puck Trigger (19) | Goal Trigger |

No collisions: Default (0), TransparentFX (1), Ignore Raycast (2), Level (3), Water (4), UI (5), PostProcessing (10), Net Cloth Collider (17).

Key observations:
- Puck does NOT collide with Player directly — puck-player contact goes through Player Mesh
- Stick only collides with Stick and Puck
- Goal Trigger and Puck Trigger only collide with each other (dedicated goal detection pair)

## Physics Materials

| Material | Dynamic Friction | Static Friction | Bounciness | Friction Combine | Bounce Combine |
|----------|-----------------|-----------------|------------|-----------------|----------------|
| Barrier | 0 | 0 | 0.2 | Multiply | Maximum |
| Default | 0 | 0 | 0 | Multiply | Average |
| Goal Net | 0 | 0 | 0 | Multiply | Average |
| Ice | 0 | 0 | 0 | Multiply | Average |
| Player Body | 0 | 0 | 0.2 | Multiply | Maximum |
| Player Mesh | 0 | 0 | 0.2 | Multiply | Maximum |
| Puck | 0 | 0 | 0 | Multiply | Maximum |
| Stick | 0 | 0 | 0 | Multiply | Average |
| Stick Blade | 0.3 | 0.3 | 0 | Maximum | Maximum |
| Stick Shaft | 0 | 0 | 0 | Multiply | Average |

## Rink Dimensions

### Ice Surface

| Parameter | Value | Source |
|-----------|-------|--------|
| Length (Z-axis) | 61.0 | Mesh AABB |
| Width (X-axis) | 30.0 | Mesh AABB |
| Spans | (-15, -30.5) to (15, 30.5) | Mesh AABB |
| Ice Bottom Y | 0 | level_1.unity |
| Ice Top Y | 0.025 | level_1.unity |

### Boards & Barriers

#### Visual meshes (not physics — colliders disabled)

| Component | Local Width (X) | Local Length (Z) | Height | Notes |
|-----------|-----------------|------------------|--------|-------|
| Barrier (dasher boards) | 30.1 | 61.1 | 0.75 | Z range: -1.0 to -0.25 |
| Barrier Bottom Border | 30.07 | 61.12 | 0.25 | Kickplate, Z: -0.25 to 0.0 |
| Barrier Top Border | 30.18 | 61.16 | 0.02 | Cap rail, Z: ~-1.51 |
| Glass | 30.15 | 61.15 | 3.1 | Sits on boards, Z: -4.6 to -1.5 |

#### Barrier Collider (physics — the main rink boundary)

The barrier collider is a **non-convex MeshCollider** (256 vertices, 128 triangles) forming a closed **oval (rounded rectangle)** wall with no openings. It is a child of the "Rink" GameObject at world origin.

| Parameter | Value | Source |
|-----------|-------|--------|
| Type | MeshCollider (non-convex) | level_1.unity |
| Layer | 12 (Boards) | level_1.unity |
| Tag | Soft Collider | level_1.unity |
| Material | Barrier (friction 0, bounciness 0.2) | level_1.unity |
| Transform Scale | (1.25, 1.25, 1) | level_1.unity |
| Transform Rotation | 90 degrees around X | level_1.unity |

**Local mesh dimensions (before transform):**
- Straight sides at localX = ±15.4, from localY = -22.833 to +22.833
- Curved corners from localY = ±22.833 to ±30.9
- Wall height from localZ = -20.025 to +4.975

**World-space dimensions (after 1.25 scale and 90-degree X rotation):**
- Side walls at **X = ±19.25**
- Straight sections from **Z = ±28.54**, curving to ends
- End walls reach **Z = ±38.625** (at X=0, the tip of the oval)
- Wall extends from **Y = -4.975** to **Y = +20.025**
- Goals at Z=±34 are fully inside the oval (~4.5m from end wall)

#### Arena Boundary Colliders (BoxColliders)

Additional box colliders on the Rink, with 90-degree X and Z rotations applied. Layer 12 (Boards), tagged "Soft Collider".

| Collider | Size | Center | Source |
|----------|------|--------|--------|
| Back | (25.0, 5.0, 47.5) | (12.475, -40.625, 0) | level_1.unity |
| Front | (25.0, 5.0, 47.5) | (12.475, 40.625, 0) | level_1.unity |
| Right | (47.5, 86.25, 5.0) | (0, 0, 2.475) | level_1.unity |
| Left | (25.0, 86.25, 5.0) | (12.475, 0, -21.25) | level_1.unity |
| Top | (47.5, 86.25, 5.0) | (0, 0, 2.475) | level_1.unity |

### Goals

| Parameter | Value | Source |
|-----------|-------|--------|
| Blue Goal Position | (0, 0, 34) | level_1.unity |
| Red Goal Position | (0, 0, -34) | level_1.unity |
| Frame Dimensions | 3.41 W x 2.04 D x 1.62 H | Mesh AABB |
| Net Dimensions | 3.26 W x 1.92 D x 1.47 H | Mesh AABB |
| Net Collider Dimensions | 3.37 W x 1.98 D x 1.60 H | Mesh AABB |
| Goal Trigger (scoring plane) | 3.03 W x 0.05 D x 1.49 H | Mesh AABB |
| Goal Player Collider | 3.11 W x 1.79 D x 1.48 H | Mesh AABB |
| Goal Post Radius | 0.06 | CapsuleCollider |
| Goal Post Height | 3.11 (main posts), 2.0 (crossbar) | CapsuleCollider |
| Post Centers | (0, 1.15, -1.56) posts, (±1.5, 1.15, -0.62) crossbar | CapsuleCollider |

### Puck Mesh

| Parameter | Value | Source |
|-----------|-------|--------|
| Diameter | 0.304 | Mesh AABB |
| Height | 0.10 | Mesh AABB |

## Puck

### Rigidbody (from prefab — authoritative)

| Parameter | Value | Source |
|-----------|-------|--------|
| Mass | 0.375 | Puck.prefab |
| Drag | 0.3 | Puck.prefab |
| Angular Drag | 0.3 | Puck.prefab |
| Center of Mass | (0, 0, 0) | Puck.prefab |
| Inertia Tensor | (0.002, 0.002, 0.01) | Puck.prefab |
| Use Gravity | true | Puck.prefab |
| Collision Detection | Continuous | Puck.prefab |
| Interpolation | Interpolate | Puck.prefab |

### Behavior

| Parameter | Value | Source |
|-----------|-------|--------|
| Max Speed | 30 | Puck.prefab |
| Max Angular Speed | 60 | Puck.prefab (overrides .cs default of 30) |
| Inertia Tensor (on stick) | (0.006, 0.002, 0.006) | Puck.prefab |
| Inertia Tensor (default) | (0.002, 0.002, 0.002) | Puck.prefab |
| Grounded Check Sphere Radius | 0.075 | Puck.prefab |
| Grounded Center of Mass | (0, -0.01, 0) | Puck.prefab |
| Sphere Collider Radius Min | 0.15 | Puck.cs |
| Sphere Collider Radius Max | 0.75 | Puck.cs |
| Radius Adjust Speed | fixedDeltaTime * 5 | Puck.cs |
| Y-Axis Correction Multiplier | 5 (ForceMode.Acceleration) | Puck.cs |
| Collision Detection (default) | ContinuousDynamic | PuckCollisionDetectionModeSwitcher.cs |
| Collision Detection (on stick) | ContinuousSpeculative | PuckCollisionDetectionModeSwitcher.cs |

### Puck Colliders

| Collider | Type | Material | Details |
|----------|------|----------|---------|
| Stick Collider | MeshCollider (convex) | Stick Blade | ExcludeLayers: 0x2000 |
| Ice Collider | MeshCollider (convex) | Ice | ExcludeLayers: 0x40 |
| Net Trigger | SphereCollider | — | Radius: 0.15, center: (0,0,0) |
| Goal Net Collider | MeshCollider (convex, trigger) | Puck | IsTrigger: true |

### Puck in Goal Net

| Parameter | Value | Source |
|-----------|-------|--------|
| Linear Velocity Max Magnitude | 2 | Puck.prefab |
| Angular Velocity Max Magnitude | 2 | Puck.prefab |

## Player Body (Attacker)

### Rigidbody (from prefab — authoritative)

| Parameter | Value | Source |
|-----------|-------|--------|
| Mass | 90 | Player Body V2 (Attacker).prefab |
| Drag | 0 | Player Body V2 (Attacker).prefab |
| Angular Drag | 0 | Player Body V2 (Attacker).prefab |
| Center of Mass | (0, 1.25, 0) | Player Body V2 (Attacker).prefab |
| Inertia Tensor | (1, 1, 1) auto-calculated | Player Body V2 (Attacker).prefab |
| Use Gravity | true | Player Body V2 (Attacker).prefab |
| Collision Detection | Continuous | Player Body V2 (Attacker).prefab |
| Interpolation | Interpolate | Player Body V2 (Attacker).prefab |

### Player Colliders

| Collider | Type | Material | Details |
|----------|------|----------|---------|
| Body | CapsuleCollider | Player Body | Radius: 0.3, Height: 1.5, Y-axis, Center: (0, 1.25, 0) |
| Shoulder | SphereCollider | Player Body | Radius: 0.4, Center: (0, 1.5, 0) |
| Head | SphereCollider | Player Body | Radius: 0.225, Center: (0, 0.225, 0) |

### Movement (Both Roles — shared values)

| Parameter | Value | Source |
|-----------|-------|--------|
| Forward Acceleration | 2 | Prefab |
| Forward Sprint Acceleration | 4.75 | Prefab |
| Forward Sprint Overspeed Acceleration | 1 | Prefab |
| Backward Acceleration | 1.8 | Prefab |
| Backward Sprint Acceleration | 2 | Prefab |
| Backward Sprint Overspeed Acceleration | 1 | Prefab |
| Brake Acceleration | 5 | Prefab |
| Drag | 0.015 | Prefab (overrides .cs default of 0.025) |
| Overspeed Drag | 0.25 | Prefab (overrides .cs default of 0.025) |
| Turn Acceleration | 1.5 | Prefab (overrides .cs default of 1.625) |
| Turn Brake Acceleration | 4.5 | Prefab (overrides .cs default of 3.25) |
| Max Turn Speed | 1.3125 | Prefab (overrides .cs default of 1.375) |
| Turn Drag | 3 | Prefab |
| Turn Overspeed Drag | 2.25 | Prefab |

### Movement (Speed Caps — differ by role)

| Parameter | Attacker | Goalie | Source |
|-----------|----------|--------|--------|
| Max Forward Speed | 7.5 | 5 | Prefab |
| Max Forward Sprint Speed | 8.75 | 6.25 | Prefab |
| Max Backward Speed | 7.5 | 5 | Prefab (overrides .cs default of 7.25) |
| Max Backward Sprint Speed | 8.5 | 6 | Prefab (overrides .cs default of 7.25) |

### Gravity & Hover (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Gravity Multiplier | 2 | Prefab |
| Hover Distance | 1.2 | Prefab |
| Slide Hover Distance | 0.9 | Prefab (overrides .cs default of 0.8) |
| Upwardness Threshold | 0.8 | Prefab |
| Sideways Threshold | 0.2 | Prefab |

### Hover PID (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Proportional Gain | 100 | Prefab |
| Integral Gain | 0 | Prefab |
| Derivative Gain | 8 | Prefab (overrides .cs default of 15) |
| Target Distance | 1.2 | Prefab (overrides .cs default of 1.0) |
| Raycast Offset | (0, 1, 0) | Prefab |
| Raycast Distance | 1.3 | Prefab (overrides .cs default of 1.45) |
| Raycast Layer Mask | 8192 (Ice layer only) | Prefab |
| Max Force | 40 | Prefab |

### Keep Upright PID (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Proportional Gain | 50 | Prefab |
| Integral Gain | 0 | Prefab |
| Derivative Gain | 8 | Prefab (overrides .cs default of 5) |

### Balance & Fall

| Parameter | Value | Source |
|-----------|-------|--------|
| Balance Loss Time | 0.25s | Prefab |
| Balance Recovery Time | 5s | Prefab |

### Stamina (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Stamina Regen Rate | 10s | Prefab |
| Sprint Drain Rate | 1.4s | Prefab |
| Jump Stamina Drain | 0.125 | Prefab |
| Twist Stamina Drain | 0.125 | Prefab |
| Dash Stamina Drain | 0.25 | Prefab (overrides .cs default of 0.125) |

### Jump (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Jump Velocity | 6 (VelocityChange) | Prefab |
| Jump Turn Multiplier | 3.5 | Prefab (overrides .cs default of 5) |

### Twist (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Twist Velocity | 5 (VelocityChange) | Prefab |

### Dash & Slide (Both Roles — shared values)

| Parameter | Value | Source |
|-----------|-------|--------|
| Dash Velocity | 6 (VelocityChange) | Prefab |
| Dash Drag | 5 | Prefab |
| Dash Drag Time | 1.5s | Prefab (overrides .cs default of 1) |
| Stop Drag | 2.5 | Prefab |
| Fallen Drag | 0.2 | Prefab |
| Slide Turn Multiplier | 2 | Prefab |

### Dash & Slide (Values that differ by role)

| Parameter | Attacker | Goalie | Source |
|-----------|----------|--------|--------|
| Can Dash | false | true | Prefab (overrides .cs default of true) |
| Slide Drag | 0.1 | 1 | Prefab (overrides .cs default of 0.2) |

### Tackle (Shared values)

| Parameter | Value | Source |
|-----------|-------|--------|
| Tackle Force Multiplier | 0.3 | Prefab |
| Tackle Bounce Max Magnitude | 10 | Prefab |

### Tackle (Values that differ by role)

| Parameter | Attacker | Goalie | Source |
|-----------|----------|--------|--------|
| Tackle Speed Threshold | 7.6 | 5.1 | Prefab |
| Tackle Force Threshold | 7 | 5 | Prefab |

### Lateral Movement (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Max Laterality | 1 | Prefab |
| Min Laterality | 0.5 | Prefab |
| Min Laterality Speed | 2 | Prefab |
| Max Laterality Speed | 5 | Prefab |

## Player Body (Goalie) — Differences from Attacker

Rigidbody and colliders are identical to Attacker (except Goalie has no Head SphereCollider listed).

All physics systems (Movement shared params, Hover PID, KeepUpright PID, Skate, VelocityLean,
Stamina, Jump, Twist, Dash shared params, Laterality) are **identical** between both prefabs.

### Complete list of Attacker vs Goalie differences (only 8 values)

| Parameter | Attacker | Goalie | Component |
|-----------|----------|--------|-----------|
| Max Forward Speed | 7.5 | 5 | Movement |
| Max Forward Sprint Speed | 8.75 | 6.25 | Movement |
| Max Backward Speed | 7.5 | 5 | Movement |
| Max Backward Sprint Speed | 8.5 | 6 | Movement |
| Can Dash | false | true | PlayerBodyV2 |
| Slide Drag | 0.1 | 1 | PlayerBodyV2 |
| Tackle Speed Threshold | 7.6 | 5.1 | PlayerBodyV2 |
| Tackle Force Threshold | 7 | 5 | PlayerBodyV2 |

**Note:** Many values previously listed as "goalie differences" were actually comparing goalie prefab
values against .cs source defaults. Both prefabs override the same .cs defaults identically.
The corrected shared values are documented in the Attacker sections above.

## Stick (Attacker & Goalie — identical)

### Rigidbody (from prefab — authoritative)

| Parameter | Value | Source |
|-----------|-------|--------|
| Mass | 1 | Stick.prefab |
| Drag | 0 | Stick.prefab |
| Angular Drag | 0 | Stick.prefab |
| Inertia Tensor | (1, 1, 1) manual | Stick.prefab |
| Use Gravity | false | Stick.prefab |
| Collision Detection | Continuous | Stick.prefab |
| Interpolation | Interpolate | Stick.prefab |

### Blade

| Parameter | Value | Source |
|-----------|-------|--------|
| Blade Angle Step | 12.5 degrees | Stick.prefab |

### Velocity Transfer (from prefab — authoritative)

| Parameter | Value | Source |
|-----------|-------|--------|
| Transfer Angular Velocity | true | Stick.prefab |
| Angular Velocity Transfer Multiplier | 0.3 | Stick.prefab (overrides .cs default of 0.25) |
| Linear Velocity Transfer Multiplier | 20 | Stick.prefab (overrides .cs default of 0.25) |
| Angular Velocity Scale | (0.5, 1, 0) | Stick.cs |

### Shaft Handle PID (from prefab — authoritative)

| Parameter | Value | Source |
|-----------|-------|--------|
| Proportional Gain | 500 | Stick.prefab |
| Integral Gain | 0 | Stick.prefab |
| Integral Saturation | 0 | Stick.prefab |
| Derivative Gain | 20 | Stick.prefab |
| Derivative Smoothing | 1 | Stick.prefab (overrides .cs default of 0.1) |
| Min Proportional Gain Multiplier | 0.25 | Stick.prefab |

### Blade Handle PID (from prefab — authoritative)

| Parameter | Value | Source |
|-----------|-------|--------|
| Proportional Gain | 500 | Stick.prefab |
| Integral Gain | 0 | Stick.prefab |
| Integral Saturation | 0 | Stick.prefab |
| Derivative Gain | 20 | Stick.prefab |
| Derivative Smoothing | 1 | Stick.prefab (overrides .cs default of 0.1) |

## Stick Positioning

| Parameter | Value | Source |
|-----------|-------|--------|
| Maximum Reach | 2.5 | StickPositioner.cs |
| Raycast Origin Padding | 0.2 | StickPositioner.cs |
| Blade Target Rotation Threshold | 25 | StickPositioner.cs |
| Blade Target Max Angle | 45 | StickPositioner.cs |
| Soft Collision Force | 1 | StickPositioner.cs |

### Stick Position PID

| Parameter | Value | Source |
|-----------|-------|--------|
| Proportional Gain | 0.75 | StickPositioner.cs |
| Integral Gain | 5 | StickPositioner.cs |
| Integral Saturation | 5 | StickPositioner.cs |
| Derivative Gain | 0 | StickPositioner.cs |
| Output Min | -15 | StickPositioner.cs |
| Output Max | 15 | StickPositioner.cs |

## Skate (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Traction | 12.5 | Prefab (overrides .cs default of 0.15) |

## Velocity Lean (Both Roles — identical)

| Parameter | Value | Source |
|-----------|-------|--------|
| Linear Force Multiplier | 0.75 | Prefab (overrides .cs default of 1) |
| Angular Force Multiplier | 6 | Prefab |

## Goal Net Collider

| Parameter | Value | Source |
|-----------|-------|--------|
| Damping Coefficient | 0.25 | GoalNetCollider.cs |
| Linear Velocity Max Magnitude | 2 | GoalNetCollider.cs |
| Angular Velocity Max Magnitude | 2 | GoalNetCollider.cs |

## Soft Collider

| Parameter | Value | Source |
|-----------|-------|--------|
| Raycast Distance | 0.5 | SoftCollider.cs |
| Force | 10 | SoftCollider.cs |
| Local Origin | (0, 0, 0) | SoftCollider.cs |
| Directions | 4 diagonal | SoftCollider.cs |

## Puck Shooter

| Parameter | Value | Source |
|-----------|-------|--------|
| Force | 1000 | PuckShooter.cs |
| Interval | 1s | PuckShooter.cs |
| Destroy Time | 2s | PuckShooter.cs |
