# Phase 7 — Accurate Collision Geometry

## Overview

This phase upgrades ALL collision shapes to match the real game. The current sim uses approximations (sphere puck, capsule+box stick, rectangular rink) that produce fundamentally wrong physics interactions. This phase extracts and uses the actual game meshes where they matter, and corrects primitive shapes where meshes aren't needed.

### Performance Analysis: Convex vs Non-Convex Meshes

**Convex meshes (ConvexHullShape)** use GJK iteration — essentially the same cost as primitive shapes regardless of vertex count. **No performance penalty.**

**Non-convex meshes (MeshShape)** require BVH traversal + per-triangle tests. ~4.4% overhead for boards + goal frame combined, which is acceptable for guaranteed accuracy.

| Shape | Type | Verts/Tris | Perf vs Primitive | Decision |
|-------|------|-----------|-------------------|----------|
| Puck (stick surface) | Convex | 68 / 64 | ~0% overhead | **Use real mesh** |
| Puck (ice surface) | Convex | 102 / 96 | ~0% overhead | **Use real mesh** |
| Stick shaft | Convex | 24 / 12 | ~0% overhead | **Use real mesh** |
| Stick blade | Convex | 34 / 16 | ~0% overhead | **Use real mesh** |
| Goal player collider | Convex | 73 / 33 | ~0% overhead | **Use real mesh** |
| Goal trigger | Convex | 24 / 12 | ~0% overhead | **Use real mesh** |
| Boards (barrier) | Non-convex | 256 / 128 | ~1.5% overhead | **Use real mesh** |
| Goal frame (×2) | Non-convex | 1128 / 668 | ~1.2% overhead | **Use real mesh** |
| Net collider (×2) | Non-convex | 276 / 152 | minimal (rare contact) | **Use real mesh** |

**Total overhead: ~3% — worth it for exact replication.**

All meshes extracted from [PuckAssets](https://github.com/ckhawks/PuckAssets/tree/main/mesh) (GLB format) via `tools/extract_meshes.py` → `Source/MeshData.h`.

---

## Part A — Puck Shape Correction (HIGHEST PRIORITY)

### The Problem

The puck is currently a **SphereShape (radius 0.152)**. The real puck is a **flat disc** (0.304 diameter × 0.10 height). This is the single largest source of simulation error — a sphere and a disc have completely different:
- Contact normals against flat surfaces (ice, stick blade)
- Bounce behavior off walls
- Sliding/rolling dynamics
- Stick-puck interaction angles

### What the Game Uses

The puck has **two separate convex MeshColliders** for different contact scenarios:

| Collider | Mesh | Verts | Tris | AABB Extent | Material | Collides With |
|----------|------|-------|------|-------------|----------|---------------|
| Stick Collider | Puck Collider (Stick).asset | 68 | 64 | 0.152 × 0.152 × 0.05 | Stick Blade | Stick (6) |
| Ice Collider | Puck Collider (Level).asset | 102 | 96 | 0.152 × 0.152 × 0.05 | Ice | Boards, Ice, Goal Post, Player Mesh, Goal Net, Goal Frame |

Both are convex, so in Jolt these become `ConvexHullShape`. The puck body would use a `StaticCompoundShape` with both hulls (or a single hull if the shapes are similar enough).

**Note**: The game also switches the puck's collision detection mode:
- Default: ContinuousDynamic
- When touching stick: ContinuousSpeculative

And dynamically changes inertia tensor:
- Default: (0.002, 0.002, 0.002)
- On stick: (0.006, 0.002, 0.006)

### Mesh Data

Already extracted to `Source/MeshData.h` via `tools/extract_meshes.py`:
- `PUCK_STICK_COLLIDER_VERTICES` — 68 verts, flat disc for stick contact
- `PUCK_LEVEL_COLLIDER_VERTICES` — 102 verts, flat disc for ice/boards/goal contact
- AABB confirms flat disc shape: X[-0.152, 0.152] Y[-0.152, 0.152] Z[-0.05, 0.05]

### Implementation in Jolt

```
// Pseudocode — use the pre-extracted vertex arrays
ConvexHullShapeSettings stickHull(PUCK_STICK_COLLIDER_VERTICES, PUCK_STICK_COLLIDER_NUM_VERTICES);
ConvexHullShapeSettings levelHull(PUCK_LEVEL_COLLIDER_VERTICES, PUCK_LEVEL_COLLIDER_NUM_VERTICES);
```

**Option 1 — Two-hull compound**: Use both meshes in a StaticCompoundShape, with different layers/materials. This matches the game's dual-collider setup where stick contacts use one surface and everything else uses another.

**Option 2 — Single hull**: If the two meshes have very similar geometry (both are flat discs, just different triangle densities), use whichever has more detail (the 102-vert one) for all contacts. Simpler, and the shape difference may be negligible.

Recommended: Start with Option 2 (single hull) and only split if testing shows stick-puck interaction needs the separate surface.

### Changes to Puck.h/cpp

- Replace `SphereShape` with `ConvexHullShape` in CreatePuck
- Keep everything else the same (mass, damping, velocity cap)
- Consider adding inertia tensor switching (deferred — need stick contact detection first)

### Testing

- Drop puck from height: should land flat on ice, not roll (sphere rolls, disc doesn't)
- Fire puck along ice: should slide, not bounce/wobble
- Stick-puck contact: blade hitting flat disc top vs sphere surface — observe deflection angle differences

---

## Part B — Stick Shape Correction

### The Problem

The stick currently uses CapsuleShape (shaft) + BoxShape (blade) in a compound. The game uses **two convex MeshColliders** — one for the shaft and one for the blade. The blade shape particularly matters for puck contact.

### What the Game Uses

| Collider | Mesh | Verts | Tris | AABB Extent | Material | Tag |
|----------|------|-------|------|-------------|----------|-----|
| Shaft | Stick Shaft Collider.asset | 24 | 12 | 0.025 × 0.939 × 0.061 | Stick Shaft | "Stick Shaft" |
| Blade | Stick Blade Collider.asset | 34 | 16 | 0.025 × 0.501 × 0.304 | Stick Blade | "Stick Blade" |

Both are convex. Attacker and goalie versions have identical geometry.

**Blade material differs from shaft**:
- Shaft: friction 0, bounciness 0 (combine: Multiply)
- Blade: friction 0.3, bounciness 0 (combine: Maximum)

The blade has friction — this affects how the puck slides across the blade surface during carries and shots.

### Mesh Data

Already extracted to `Source/MeshData.h`:
- `STICK_SHAFT_COLLIDER_VERTICES` — 24 verts, AABB: X[-0.025, 0.025] Y[-1.184, 0.694] Z[-0.058, 0.064]
- `STICK_BLADE_COLLIDER_VERTICES` — 34 verts, AABB: X[-0.025, 0.025] Y[0.661, 1.663] Z[-0.543, 0.064]
- Attacker and goalie have identical collider geometry

Note: Shaft runs Y[-1.18, 0.69] and blade runs Y[0.66, 1.66] — they don't overlap, the blade starts where the shaft ends.

### Implementation in Jolt

Replace the current CapsuleShape + BoxShape compound with ConvexHullShape + ConvexHullShape compound using the pre-extracted vertex arrays.

The vertex positions are already in the correct local space relative to the stick body origin (extracted from the game mesh directly). No additional offset transforms needed in the compound — just add both hulls at the origin.

### Changes to Stick.h/cpp

- Replace shape creation in CreateStick
- Keep all PID logic, velocity coupling, Z-constraint unchanged
- May need to adjust sub-shape positions in the compound if the mesh origins differ from current capsule/box positions

---

## Part C — Player Shape Correction

### The Problem

The player currently uses a single CapsuleShape. The game uses **CapsuleCollider + SphereCollider (head)**. The head sphere extends above the capsule and changes how puck deflections off the player's upper body work.

### What the Game Uses

| Collider | Type | Params | Center | Material | Layer |
|----------|------|--------|--------|----------|-------|
| Body | CapsuleCollider | r: 0.3, h: 1.5, Y-axis | (0, 1.25, 0) | Player Body | 8 (Player) |
| Head | SphereCollider | r: 0.4 | (0, 1.5, 0) | Player Body | 8 (Player) |

**Note**: physics-values.md also mentions a Shoulder SphereCollider (r: 0.225, center: (0, 0.225, 0)) but this was NOT found in the prefab analysis. It may be wrong — verify against the prefab before implementing.

### Player Mesh (Layer 9) — Clarification Needed

The collision matrix says Puck collides with Player Mesh (Layer 9), not Player (Layer 8). However, investigation found **no collider on the Player Mesh object** (only Transform + MonoBehaviour). This needs further investigation:
- The Player Mesh kinematic body in our sim may be the correct approach
- Or the puck collision may actually happen against the body colliders via a different mechanism
- Check the prefab hierarchy more carefully for colliders on Layer 9

**Current approach** (kinematic mesh body synced to player position) should continue until this is clarified.

### Implementation

Add a SphereShape to the player's compound shape:

```
// In existing RotatedTranslatedShape setup, switch to StaticCompoundShape:
// Sub-shape 1: CapsuleShape(0.45, 0.3) at (0, 1.25, 0) — existing
// Sub-shape 2: SphereShape(0.4) at (0, 1.5, 0) — new head collider
```

This is a trivial change — just adding one primitive to the existing body. No performance impact.

---

## Part D — Rink Boards (Real Mesh)

### What the Game Uses

The barrier collider is a single **non-convex MeshCollider** on Layer 12 (Boards):

| Parameter | Value | Source |
|-----------|-------|--------|
| Type | MeshCollider (non-convex) | level_1.unity |
| Mesh | Barrier Collider.asset (256 verts, 384 tris) | Mesh/ |
| Layer | 12 (Boards) | level_1.unity |
| Physics Material | Barrier (friction 0, bounciness 0.2, bounce combine: Maximum) | level_1.unity |
| Transform Position | (0, 0, 0) | level_1.unity |
| Transform Rotation | 90° around X | level_1.unity |
| Transform Scale | (1.25, 1.25, 1) | level_1.unity |

### Mesh Data

Already extracted to `Source/MeshData.h`:
- `BARRIER_COLLIDER_VERTICES` — 256 verts, 128 triangles
- Raw AABB: X[-15.4, 15.4] Y[-30.9, 30.9] Z[-20.025, 4.975]

**Important**: The raw mesh is in local space. You must apply the scene transform before creating the Jolt MeshShape:
1. **90° X rotation**: (x, y, z) → (x, z, -y) — swaps Y/Z axes
2. **1.25 scale**: multiply X and new-Y by 1.25 (scale was 1.25, 1.25, 1 — the Z component maps to height after rotation, which had scale 1)

After transform:
- Side walls at X = ±19.25 (15.4 × 1.25)
- Rink extends Z = ±38.625 (30.9 × 1.25)
- Wall height from Y = -4.975 to Y = +25.03

Feed transformed vertices + triangle indices into Jolt's `MeshShapeSettings`.

**Triangle winding**: Must face **inward** (Jolt MeshShape is single-sided). If puck passes through walls, reverse index order.

### What to Remove

Delete the 4 box walls from the Rink struct. Replace with a single `BodyID boardsId`.

### Physics Material

| Property | Value |
|----------|-------|
| Friction | 0 |
| Restitution | 0.2 |

"Maximum" bounce combine → Jolt uses max(A, B) for restitution by default, which matches.

### Arena Boundary Colliders

The game has large box colliders outside the rink as safety nets (Back, Front, Right, Left, Top) on Layer 12. Skip unless testing reveals escape issues.

### Testing

1. **Corner bounce**: Fire puck at 45° into corner — should deflect smoothly along curve
2. **Side wall bounce**: Straight at side wall — should bounce with ~0.2 restitution
3. **Slide along curve**: Shallow angle into corner — should follow the curve, not stop dead

---

## Part E — Goal Structures

### Goal Positions & Transforms

| Goal | World Position | Rotation | Scale |
|------|---------------|----------|-------|
| Red | (0, 0, -34.1) | Identity | (0.9, 0.9, 0.8) |
| Blue | (0, 0, 34.1) | 180° around Y | (0.9, 0.9, 0.8) |

All children have local position (0,0,0) and **90° X rotation**. Child local (x, y, z) maps to parent (x, z, -y).

### Goal Post Colliders (Layer 11) — Capsules

3 CapsuleColliders per goal. The mesh collider on this object is **disabled**.

**Local-space values (before child 90° X rotation and parent scale):**

| Capsule | Radius | Height | Direction | Center | Role |
|---------|--------|--------|-----------|--------|------|
| Crossbar | 0.06 | 3.11 | X (0) | (0, 1.15, -1.56) | Horizontal bar across goal mouth |
| Right Post | 0.06 | 2.0 | Z (2) | (1.5, 1.15, -0.62) | Right side depth post |
| Left Post | 0.06 | 2.0 | Z (2) | (-1.5, 1.15, -0.62) | Left side depth post |

**Transform chain**: Child 90° X rotation → Parent scale (0.9, 0.9, 0.8) → Parent translation.

Non-uniform parent scale distorts capsules. Compute final world-space dimensions and create Jolt capsules directly at those sizes. No physics material assigned (default: friction 0, bounciness 0).

### Goal Frame (Layer 18) — Real Mesh

**Non-convex MeshCollider** using Frame.asset:
- 1,128 vertices, 668 triangles (from GLB export)
- Raw AABB: X[-1.70, 1.70] Y[-0.84, 1.20] Z[-1.62, 0.00]

Already extracted as `GOAL_FRAME_VERTICES` / `GOAL_FRAME_INDICES` in MeshData.h.

**Transform chain** (must apply to vertices before creating Jolt MeshShape):
1. Child 90° X rotation: (x, y, z) → (x, z, -y)
2. Parent scale: (0.9, 0.9, 0.8)
3. Parent translation: (0, 0, ±34.1) for red/blue

Apply once per goal (two separate MeshShapes with mirrored transforms). Blue goal also has 180° Y rotation.

### Net Collider (Layer 14) — Real Mesh + Damping

**Non-convex MeshCollider** using Net Collider.asset:
- 276 vertices, 152 triangles (from GLB export)
- Raw AABB: X[-1.68, 1.68] Y[-0.82, 1.16] Z[-1.60, 0.00]
- Physics Material: Goal Net (friction 0, bounciness 0, bounce combine: Minimum)
- Tagged "Soft Collider"

Already extracted as `GOAL_NET_COLLIDER_VERTICES` / `GOAL_NET_COLLIDER_INDICES` in MeshData.h. Same transform chain as goal frame.

**Damping behavior** (GoalNetCollider.cs — .cs defaults are authoritative, no prefab override found):

On puck collision with net collider:
1. Linear velocity × 0.75 (25% damping)
2. Angular velocity × 0.75
3. Clamp linear velocity to max **2 m/s**
4. Clamp angular velocity to max **2 rad/s**
5. Skip if puck is grounded

Implement via `ContactListener::OnContactAdded` — detect puck vs GOAL_NET layer contacts and modify puck velocity.

### Goal Trigger (Layer 15) — Real Mesh

**Convex MeshCollider** (trigger) using Goal Trigger.asset:
- 24 vertices, 12 triangles — basically a box but exact
- Raw AABB: X[-1.52, 1.52] Y[0.69, 0.74] Z[-1.48, 0.00]
- Free performance (convex)

Already extracted as `GOAL_TRIGGER_VERTICES`. Replace current box sensor with ConvexHullShape. Same transform chain as other goal children.

### Goal Player Collider (Layer 16) — Real Mesh

**Convex MeshCollider** using Goal Player Collider.asset:
- 73 vertices, 33 triangles
- Raw AABB: X[-1.56, 1.55] Y[-0.66, 1.13] Z[-1.48, 0.00]
- Layer 16 — collides with Player (Layer 8) only
- Prevents player from entering goal
- Free performance (convex)

Already extracted as `GOAL_PLAYER_COLLIDER_VERTICES`. Use as ConvexHullShape. Same transform chain.

---

## Part F — Mesh Extraction (DONE)

All meshes have been extracted from [PuckAssets](https://github.com/ckhawks/PuckAssets/tree/main/mesh) GLB files.

- **Source GLB files**: `C:\Users\kyle\Developer\PuckAssets\mesh\`
- **Extraction script**: `tools/extract_meshes.py`
- **Output**: `Source/MeshData.h` — static const Float3/uint32_t arrays, auto-generated
- **Re-run**: `python tools/extract_meshes.py` to regenerate

### Extracted Meshes

| C++ Identifier | Verts | Tris | Shape Type | Used For |
|----------------|-------|------|-----------|----------|
| PUCK_STICK_COLLIDER | 68 | 64 | ConvexHull | Puck body (stick contact) |
| PUCK_LEVEL_COLLIDER | 102 | 96 | ConvexHull | Puck body (ice/boards/goal) |
| STICK_SHAFT_COLLIDER | 24 | 12 | ConvexHull | Stick shaft |
| STICK_BLADE_COLLIDER | 34 | 16 | ConvexHull | Stick blade |
| BARRIER_COLLIDER | 256 | 128 | TriangleMesh | Rink boards |
| GOAL_FRAME | 1128 | 668 | TriangleMesh | Goal frame (×2) |
| GOAL_NET_COLLIDER | 276 | 152 | TriangleMesh | Goal net (×2) |
| GOAL_TRIGGER | 24 | 12 | ConvexHull | Goal trigger (×2) |
| GOAL_PLAYER_COLLIDER | 73 | 33 | ConvexHull | Goal keep-out (×2) |

**Total: 1,985 vertices, 1,181 triangles across 9 meshes.**

Note: Triangle counts from GLB export are lower than the .asset file counts (GLB deduplicates). This makes the non-convex meshes even cheaper than originally estimated.

---

## Part G — Implementation Plan

### New/Modified Files

| File | Changes |
|------|---------|
| `Source/MeshData.h` | **DONE** — extracted mesh vertex/index arrays (auto-generated) |
| `Source/Puck.h/cpp` | Replace SphereShape with ConvexHullShape |
| `Source/Stick.h/cpp` | Replace CapsuleShape+BoxShape with ConvexHullShape+ConvexHullShape |
| `Source/Player.h/cpp` | Add head SphereShape to player compound |
| `Source/Rink.h/cpp` | Replace box walls with MeshShape boards, add full goal structures |
| `Source/Listeners.h` | Add net collider damping in contact listener |
| `Source/Layers.cpp` | Verify all goal-related collision pairs |

### Implementation Order

1. ~~**Extract meshes**~~ — DONE (`Source/MeshData.h`)
2. **Puck shape** — Replace sphere with convex hull (highest impact fix)
3. **Stick shape** — Replace capsule+box with convex hulls
4. **Player head** — Add sphere to player compound
5. **Rink boards** — Replace box walls with triangle mesh
6. **Goal posts** — 3 capsules per goal
7. **Goal frame** — Triangle mesh from extracted data
8. **Goal net collider** — Triangle mesh + contact listener damping
9. **Goal trigger** — Convex hull from extracted data
10. **Goal player collider** — Convex hull from extracted data
11. **Test everything**

### Collision Pairs to Verify

| Pair | Layers |
|------|--------|
| Puck ↔ Goal Post | 7 ↔ 11 |
| Puck ↔ Goal Frame | 7 ↔ 18 |
| Puck ↔ Goal Net | 7 ↔ 14 |
| Player ↔ Goal Post | 8 ↔ 11 |
| Player ↔ Goal Net | 8 ↔ 14 |
| Player ↔ Player Collider | 8 ↔ 16 |
| Goal Trigger ↔ Puck Trigger | 15 ↔ 19 |

### Updated Rink Struct

```
Rink {
    BodyID iceId;
    BodyID boardsId;              // MeshShape — replaces 4 box walls

    // Blue goal
    BodyID blueGoalTriggerId;     // ConvexHullShape sensor
    BodyID bluePostsId;           // Compound: 3 capsules
    BodyID blueFrameId;           // MeshShape
    BodyID blueNetColliderId;     // MeshShape + damping behavior
    BodyID bluePlayerColliderId;  // ConvexHullShape

    // Red goal (mirror of blue)
    BodyID redGoalTriggerId;
    BodyID redPostsId;
    BodyID redFrameId;
    BodyID redNetColliderId;
    BodyID redPlayerColliderId;
}
```

---

## Part H — Key Gotchas

1. **MeshShape triangle winding**: Jolt MeshShape is single-sided. Normals must face inward for rink walls, inward for goal net/frame. If objects pass through, reverse the triangle index order.

2. **MeshShape is static only**: Jolt MeshShapes only work with `EMotionType::Static`. Fine for rink and goals.

3. **ConvexHullShape is dynamic-compatible**: Convex hulls work with dynamic bodies. Fine for puck and stick.

4. **Non-uniform goal scale (0.9, 0.9, 0.8)**: Apply the full transform chain to mesh vertices before creating Jolt shapes. Don't try to scale Jolt bodies at runtime.

5. **Goal rotation**: Blue goal has 180° Y rotation. Apply this to vertices during extraction.

6. **Transform chain for goal children**: Local space → 90° X rotation → parent scale (0.9, 0.9, 0.8) → parent translation (0, 0, ±34.1). Bake all transforms into vertex positions.

7. **Net collider damping**: Collision *response* modification, not shape property. Implement in ContactListener.

8. **Bounce combine modes**: Barrier = Maximum (Jolt default matches). Goal Net = Minimum (may need contact listener override).

9. **Puck dual collider**: The game uses different collider surfaces for stick contact vs level contact. If using a single hull, test that stick-puck interaction feels correct. Split into compound if needed.

10. **Convex hull tolerance**: Jolt's ConvexHullShapeSettings has a `mMaxConvexRadius` and `mHullTolerance`. For small meshes (puck, stick blade), you may need to reduce the convex radius to preserve fine geometry details.

---

## Part I — Deferred Items

- **Cloth net simulation**: Unity Cloth, client-side only, doesn't affect server physics. Skip.
- **SoftCollider raycast cushioning**: Not instantiated in scene. Skip.
- **Glass above boards**: Visual only. Skip.
- **Puck inertia tensor switching**: Game changes tensor when puck touches stick. Implement later with stick contact detection.
- **Puck collision detection mode switching**: ContinuousDynamic ↔ ContinuousSpeculative. Jolt handles CCD differently — may not need this.
- **Puck sphere radius adjustment**: Game dynamically scales the net trigger sphere (0.15-0.75) based on speed. Implement later if goal detection accuracy requires it.
