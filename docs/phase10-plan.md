# Phase 10 — Sim2GameTest: Validation Pipeline Execution Plan

## Purpose

This document is a **complete, standalone execution plan** for building a validation pipeline that measures how accurately PuckSim (a headless Jolt Physics C++ simulation) replicates the physics of **Puck** (a Unity/PhysX game). It is written for an AI or developer who has **never worked on this project** and needs full context to execute correctly.

The pipeline has three components:
1. **Sim2GameTest** — A C# Harmony mod for the live Puck game that logs physics telemetry to binary files
2. **PuckSim replay mode** — A C++ mode in PuckSim that reads the same scenarios and outputs matching telemetry
3. **compare.py** — A Python script that diffs the two telemetry files and produces accuracy metrics

---

## Background Context

### What is PuckSim?

PuckSim is a headless C++ physics simulation at `C:\Users\kyle\Developer\PuckSim\`. It uses **Jolt Physics** (an open-source physics engine) to replicate the physics of **Puck**, a multiplayer hockey game built in Unity with PhysX. The goal is to train reinforcement learning agents in PuckSim (fast, headless) and deploy them in the real game.

PuckSim replicates:
- **Rink** — Ice surface, boards (oval barrier), arena boundaries
- **Goals** — Posts, net collider, trigger volumes, player colliders (2 goals: blue at +Z, red at -Z)
- **Puck** — Convex hull shape, 0.375 kg, drag 0.3, max speed 30 m/s, max angular 60 rad/s, inertia tensor switching on stick contact, Y-axis correction on stick exit, grounded COM shift
- **Players** — Capsule+sphere body with kinematic mesh overlay, hover PID, keep-upright PID, movement (forward/backward/turning with soft speed caps and drag), skate traction, velocity lean, laterality
- **Sticks** — Compound shape (shaft+blade convex hulls), dual PID position control at shaft/blade handles, velocity coupling from player, Z-rotation constraint, angular velocity transfer back to player

All physics values are extracted from authoritative Unity prefab files (not code defaults). The simulation runs at **50 Hz** (dt = 0.02s).

### Key files in PuckSim

| File | Purpose |
|------|---------|
| `Source/PuckSim.cpp` | Main entry point, sim loop, object creation/destruction |
| `Source/Player.h/cpp` | Player struct, params, all movement systems |
| `Source/Stick.h/cpp` | Stick struct, PID control, velocity coupling |
| `Source/Puck.h/cpp` | Puck creation, stick-exit correction, ground check, tensor switching |
| `Source/Rink.h/cpp` | Ice, boards, barriers |
| `Source/Goal.h/cpp` | Goal posts, net, triggers, player colliders |
| `Source/Listeners.h` | Contact listener (goal scoring, net damping, stick exit/contact tracking) |
| `Source/Layers.h` | 20 object layers, 3 broadphase layers, collision matrix |
| `Source/MeshData.h` | Vertex data for all convex hulls and meshes |
| `docs/physics-values.md` | All extracted physics values with sources |

### What is the Puck game?

Puck is a multiplayer physics-based hockey game running on Unity with PhysX. The game runs at 50 Hz physics tick rate. Mods are loaded as .NET 4.8 DLLs implementing the `IPuckMod` interface, using **Harmony** for runtime method patching.

### Existing mod projects (use as reference)

All mods follow the same structure. Use these as templates:

- **PuckGym** (`C:\Users\kyle\Developer\PuckGym\PuckGym\`) — RL training mod with shared memory IPC. Best reference for project structure, .csproj, Harmony patching, and game API access.
- **PuckCapture** (`C:\Users\kyle\Developer\PuckGym\PuckCapture\`) — Episode recording/playback mod. Best reference for binary telemetry format and physics data capture (positions, velocities, rotations).

Key patterns from existing mods:
- `.csproj` targets `net4.8`, references all DLLs from a `libs/` folder
- Post-build copies DLL to `C:\Program Files (x86)\Steam\steamapps\common\Puck\Plugins\<ModName>\`
- Entry point class implements `IPuckMod` with `OnEnable()`/`OnDisable()`
- Harmony patches use `[HarmonyPatch]` attributes on static methods
- Game runs on server authority — physics state is on `NetworkManager.Singleton.IsServer`

### Physics engine differences (why this pipeline exists)

PuckSim uses Jolt Physics; the game uses Unity's PhysX. Even with identical parameters, these engines differ in:
- Contact resolution order and constraint iteration
- Penetration recovery algorithms
- Friction models and combine modes
- Numerical integration precision
- Collision detection broadphase/narrowphase details

This pipeline measures **how much** they diverge for each subsystem, so we can prioritize fixes.

---

## Component 1: Sim2GameTest (C# Harmony Mod)

### Overview

A Puck game mod that runs **scripted physics scenarios** and logs per-frame telemetry to binary files. Each scenario isolates a specific subsystem (puck only, player movement, stick control, etc.) for targeted comparison.

### Project Setup

**Location:** `C:\Users\kyle\Developer\Sim2GameTest\`

```
Sim2GameTest/
├── Sim2GameTest.sln
├── Sim2GameTest.csproj
├── libs/                          # Copy from PuckGym/PuckGym/libs/
└── src/
    ├── Plugin.cs                  # IPuckMod entry point
    ├── TestKeybinds.cs            # Harmony patches for keybinds
    ├── TelemetryWriter.cs         # Binary file writer
    ├── ScenarioRunner.cs          # Scenario execution engine
    └── Scenarios/
        ├── PuckFreeSlide.cs       # Scenario: puck sliding on ice
        ├── PuckWallBounce.cs      # Scenario: puck bouncing off boards
        ├── PuckStickExit.cs       # Scenario: puck leaving stick contact
        ├── PlayerForward.cs       # Scenario: player forward acceleration
        ├── PlayerTurn.cs          # Scenario: player turning
        ├── PlayerLateral.cs       # Scenario: player lateral movement
        ├── PlayerHover.cs         # Scenario: player hover settling
        ├── StickTracking.cs       # Scenario: stick PID tracking target
        └── FullPlay.cs            # Scenario: full gameplay (all systems)
```

### .csproj (copy from PuckGym, modify)

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net4.8</TargetFramework>
    <LangVersion>10</LangVersion>
    <PackageId>Sim2GameTest</PackageId>
  </PropertyGroup>
  <ItemGroup>
    <Libs Include="libs\*.dll" />
    <Reference Include="@(Libs)">
      <HintPath>%(Libs.FullPath)</HintPath>
      <Private>false</Private>
    </Reference>
  </ItemGroup>
  <Target Name="PostBuildMoveDll" AfterTargets="Build">
    <PropertyGroup>
      <TargetDir>C:\Program Files (x86)\Steam\steamapps\common\Puck\Plugins\Sim2GameTest</TargetDir>
    </PropertyGroup>
    <Copy SourceFiles="$(OutputPath)$(AssemblyName).dll" DestinationFolder="$(TargetDir)" />
  </Target>
</Project>
```

### Plugin.cs

```csharp
public class Plugin : IPuckMod
{
    public static string MOD_NAME = "Sim2GameTest";
    public static string MOD_GUID = "pw.stellaric.sim2gametest";
    static readonly Harmony harmony = new Harmony(MOD_GUID);

    public bool OnEnable()
    {
        harmony.PatchAll();
        TestKeybinds.Initialize();
        return true;
    }

    public bool OnDisable()
    {
        harmony.UnpatchSelf();
        TestKeybinds.Cleanup();
        return true;
    }
}
```

### Keybind Design

- **F5** — Run the currently selected scenario
- **F6** — Cycle through available scenarios
- **F7** — Run ALL scenarios sequentially

Use Harmony postfix patch on `PlayerInput.Update` (same pattern as PuckGym's `RLKeybinds.cs`).

### Telemetry Format

**Binary file**, one per scenario run. This format must be **identical** between the C# mod and the C++ replay so the Python comparison script can read both.

**File header (64 bytes):**

| Offset | Type | Size | Field |
|--------|------|------|-------|
| 0 | uint8 | 1 | Format version (1) |
| 1 | uint8 | 1 | Scenario ID (enum) |
| 2 | uint16 | 2 | Reserved (padding) |
| 4 | uint32 | 4 | Total frame count |
| 8 | float | 4 | Delta time (0.02) |
| 12 | uint8 | 1 | Object count (how many objects tracked) |
| 13 | uint8[51] | 51 | Reserved (zero-fill, future use) |

**Per-frame data (variable size, depends on objects tracked):**

For each tracked object per frame, write a **telemetry record (52 bytes)**:

| Offset | Type | Size | Field |
|--------|------|------|-------|
| 0 | float | 4 | pos.x |
| 4 | float | 4 | pos.y |
| 8 | float | 4 | pos.z |
| 12 | float | 4 | vel.x |
| 16 | float | 4 | vel.y |
| 20 | float | 4 | vel.z |
| 24 | float | 4 | angVel.x |
| 28 | float | 4 | angVel.y |
| 32 | float | 4 | angVel.z |
| 36 | float | 4 | rot.x (quaternion) |
| 40 | float | 4 | rot.y |
| 44 | float | 4 | rot.z |
| 48 | float | 4 | rot.w |

So each frame = `objectCount * 52` bytes. Objects are always written in a fixed order defined per scenario (e.g., scenario "PuckFreeSlide" has 1 object = puck, so 52 bytes/frame).

**Object order convention:**
1. Puck (if tracked)
2. Player 1 / Attacker (if tracked)
3. Player 1 mesh (if tracked)
4. Player 2 / Goalie (if tracked)
5. Stick 1 (if tracked)
6. Stick 2 (if tracked)

**Output directory:** `C:\Users\kyle\Developer\Sim2GameTest\recordings\`

**Filename:** `{scenario_name}_{timestamp}.bin` (e.g., `puck_free_slide_20260330_143022.bin`)

### Scenario Design

Each scenario must:
1. Teleport objects to exact initial positions/velocities
2. Set specific inputs (or no input)
3. Run for a fixed number of frames (e.g., 200 = 4 seconds)
4. Log telemetry every frame
5. Be fully deterministic (no random elements)

#### Scenario 1: PuckFreeSlide
- **Tests:** Puck drag, ice friction, linear deceleration
- **Setup:** Puck at (0, 0.075, 0), velocity (5, 0, 3), no angular velocity
- **Objects:** Puck only (1 object)
- **Duration:** 200 frames (4 seconds)
- **Inputs:** None (no players active)

#### Scenario 2: PuckWallBounce
- **Tests:** Puck-boards collision response, restitution
- **Setup:** Puck at (15, 0.075, 0), velocity (10, 0, 0) toward +X wall
- **Objects:** Puck only
- **Duration:** 200 frames

#### Scenario 3: PuckStickExit
- **Tests:** Y-axis correction, speed clamps, inertia tensor switching
- **Setup:** Place puck on stick, give it velocity, let it separate
- **Objects:** Puck + Stick (2 objects)
- **Duration:** 100 frames
- **Note:** This requires positioning the stick to make contact, then moving it away

#### Scenario 4: PlayerForward
- **Tests:** Forward acceleration, speed cap, drag, overspeed drag
- **Setup:** Player at (0, 0.5, 0), identity rotation, forward input = 1.0
- **Objects:** Player body only (1 object)
- **Duration:** 500 frames (10 seconds, enough to reach and sustain speed cap)
- **Inputs:** forward=1.0, turn=0, lateral=0

#### Scenario 5: PlayerTurn
- **Tests:** Turn acceleration, max turn speed, turn drag
- **Setup:** Player at (0, 0.5, 0), forward input = 1.0, turn = 1.0
- **Objects:** Player body (1 object)
- **Duration:** 300 frames

#### Scenario 6: PlayerLateral
- **Tests:** Laterality computation, movement direction rotation, skate traction in rotated frame
- **Setup:** Player at (0, 0.5, 0), forward=1.0, lateral=1.0
- **Objects:** Player body (1 object)
- **Duration:** 300 frames

#### Scenario 7: PlayerHover
- **Tests:** Hover PID settling, hover distance
- **Setup:** Player at (0, 2.0, 0) — dropped from height, no input
- **Objects:** Player body (1 object)
- **Duration:** 300 frames

#### Scenario 8: StickTracking
- **Tests:** Stick PID tracking accuracy, velocity coupling
- **Setup:** Player stationary, stick tracking a moving target point
- **Objects:** Player + Stick (2 objects)
- **Duration:** 200 frames

#### Scenario 9: FullPlay
- **Tests:** All systems combined, cumulative drift
- **Setup:** Player at (5, 0.5, 0), puck at (0, 0.2, 0), forward input toward puck
- **Objects:** Player + Player mesh + Puck + Stick (4 objects)
- **Duration:** 500 frames

### How to Capture Data from Unity

To access physics state in Unity (inside `FixedUpdate`):

```csharp
// Get references (find via NetworkManager or scene hierarchy)
// The game's main classes:
//   - Puck (has .Rigidbody property)
//   - PlayerBodyV2 (has .Rigidbody property)
//   - Stick (has .Rigidbody property)
//   - Movement (on same GameObject as PlayerBodyV2)

// Position
Vector3 pos = rigidbody.position;
// Velocity
Vector3 vel = rigidbody.linearVelocity;  // Unity 6+ uses .linearVelocity
// Angular velocity
Vector3 angVel = rigidbody.angularVelocity;
// Rotation
Quaternion rot = rigidbody.rotation;
```

**Finding game objects:** Look at how PuckGym's `RLController.cs` finds the local player, puck, and stick. It uses `GameManager`, `PuckManager`, and player references. The `PrefabHelper.cs` in PuckCapture shows how to find and manipulate game objects.

**Teleporting objects:** Use `Rigidbody.position = newPos`, `Rigidbody.rotation = newRot`, `Rigidbody.linearVelocity = newVel`, `Rigidbody.angularVelocity = newAngVel`. See PuckCapture's `PlaybackController.SyncPuckPosition()` and `SyncPlayerPosition()` for working examples.

**Setting player input:** Patch `PlayerInput.UpdateInputs` and override the input values. See PuckGym's `RLController.ApplyAction()` for the exact field names and how to set movement, turn, lateral, stick aim, etc.

**IMPORTANT:** All physics manipulation must happen on the **server**. Check `NetworkManager.Singleton.IsServer` before modifying rigidbody state. For local testing, host a private match.

### Scenario Execution Flow

```
ScenarioRunner.Run(scenario):
  1. Pause normal gameplay (set Time.timeScale or use a flag)
  2. Teleport objects to scenario initial conditions
  3. Zero all velocities
  4. Wait 1 frame for physics to settle positions
  5. For each frame (0..frameCount):
     a. Apply scenario inputs (if any)
     b. Capture telemetry for all tracked objects
     c. Write to TelemetryWriter buffer
  6. Flush buffer to binary file
  7. Print "Scenario complete: {filename}" to chat/console
  8. Resume normal gameplay
```

The scenario runner should be a `MonoBehaviour` that uses `FixedUpdate()` for frame-by-frame control (same as PuckGym's `RLController`).

---

## Component 2: PuckSim Replay Mode

### Overview

Add a command-line mode to PuckSim that runs the **same scenarios** as the Unity mod and outputs telemetry in the **identical binary format**.

### Implementation Approach

**Do NOT modify the main sim loop.** Instead, add a separate entry path in `main()` controlled by command-line arguments:

```
PuckSim.exe                          # Normal sim (existing behavior)
PuckSim.exe --test puck_free_slide   # Run specific scenario
PuckSim.exe --test all               # Run all scenarios
```

### New Files

| File | Purpose |
|------|---------|
| `Source/Telemetry.h/cpp` | Binary telemetry writer (matching the C# format exactly) |
| `Source/Scenarios.h/cpp` | All scenario definitions and runner |

### Telemetry Writer

Must produce **byte-identical format** to the C# mod:
- Same 64-byte header structure
- Same 52-byte per-object per-frame records
- Same object ordering
- Little-endian floats (both platforms are x86, so this is automatic)

```cpp
struct TelemetryHeader {
    uint8_t version;        // 1
    uint8_t scenarioId;
    uint16_t reserved;
    uint32_t frameCount;
    float deltaTime;
    uint8_t objectCount;
    uint8_t padding[51];
};

struct TelemetryRecord {
    float posX, posY, posZ;
    float velX, velY, velZ;
    float angVelX, angVelY, angVelZ;
    float rotX, rotY, rotZ, rotW;
};
```

### Scenario Implementation

Each scenario mirrors its Unity counterpart exactly:

```cpp
void RunPuckFreeSlide(BodyInterface& bi, PhysicsSystem& ps, ...) {
    // 1. Create all bodies (rink, puck, etc.)
    // 2. Set initial conditions: pos (0, 0.075, 0), vel (5, 0, 3)
    // 3. For 200 frames:
    //    a. Run pre-step systems (SyncPuckTrigger, etc.)
    //    b. physics_system.Update(0.02, 1, ...)
    //    c. Run post-step systems (net damping, tensor, ground check)
    //    d. Capture telemetry record for puck
    // 4. Write file
}
```

**Critical:** The scenario must create the FULL physics environment (rink, ice, boards, goals) even if only testing the puck. The puck interacts with ice (drag, ground check) and the collision geometry matters.

### Output Directory

`C:\Users\kyle\Developer\Sim2GameTest\recordings\` — same folder as the Unity mod, but with `_sim` suffix on filenames (e.g., `puck_free_slide_sim.bin`).

### What to Capture from Jolt

```cpp
Vec3 pos = bi.GetPosition(bodyId);       // or GetCenterOfMassPosition
Vec3 vel = bi.GetLinearVelocity(bodyId);
Vec3 angVel = bi.GetAngularVelocity(bodyId);
Quat rot = bi.GetRotation(bodyId);

TelemetryRecord rec;
rec.posX = pos.GetX(); rec.posY = pos.GetY(); rec.posZ = pos.GetZ();
rec.velX = vel.GetX(); rec.velY = vel.GetY(); rec.velZ = vel.GetZ();
rec.angVelX = angVel.GetX(); rec.angVelY = angVel.GetY(); rec.angVelZ = angVel.GetZ();
rec.rotX = rot.GetX(); rec.rotY = rot.GetY(); rec.rotZ = rot.GetZ(); rec.rotW = rot.GetW();
```

**IMPORTANT:** Use `GetPosition` (not `GetCenterOfMassPosition`) to match Unity's `Rigidbody.position`, which returns the transform position, not COM.

---

## Component 3: compare.py (Python Analysis)

### Overview

Reads pairs of telemetry files (one from Unity, one from PuckSim) and produces per-object, per-axis error metrics with optional plotting.

### Location

`C:\Users\kyle\Developer\Sim2GameTest\compare.py`

### Dependencies

```
pip install numpy matplotlib
```

### Usage

```
python compare.py recordings/puck_free_slide_20260330.bin recordings/puck_free_slide_sim.bin
python compare.py recordings/ --all          # Compare all matching pairs
python compare.py recordings/ --all --plot   # With matplotlib plots
```

### Reading the Binary Format

```python
import struct
import numpy as np

HEADER_SIZE = 64
RECORD_SIZE = 52  # 13 floats * 4 bytes

def read_telemetry(filepath):
    with open(filepath, 'rb') as f:
        header = f.read(HEADER_SIZE)
        version, scenario_id, _, frame_count, dt, object_count = struct.unpack_from(
            '<BBHIfB', header, 0
        )

        frames = []
        for frame_idx in range(frame_count):
            objects = []
            for obj_idx in range(object_count):
                data = f.read(RECORD_SIZE)
                values = struct.unpack('<13f', data)
                objects.append({
                    'pos': np.array(values[0:3]),
                    'vel': np.array(values[3:6]),
                    'angVel': np.array(values[6:9]),
                    'rot': np.array(values[9:13]),  # x, y, z, w
                })
            frames.append(objects)

    return {
        'version': version,
        'scenario_id': scenario_id,
        'frame_count': frame_count,
        'dt': dt,
        'object_count': object_count,
        'frames': frames,
    }
```

### Metrics to Compute

For each tracked object, compute:

1. **Position RMSE** — `sqrt(mean(||pos_unity - pos_sim||^2))` over all frames
2. **Position max deviation** — `max(||pos_unity - pos_sim||)` and the frame it occurs
3. **Velocity RMSE** — same for linear velocity
4. **Angular velocity RMSE** — same for angular velocity
5. **Rotation error** — quaternion angle difference: `2 * acos(|q1 . q2|)` in degrees
6. **Drift over time** — position error at frame N as a function of N (shows if error grows linearly, quadratically, etc.)
7. **Per-axis breakdown** — separate X, Y, Z errors to identify which axes diverge most

### Output Format

```
=== Scenario: puck_free_slide ===
Object 0 (Puck):
  Position RMSE:     0.0023 m
  Position Max Dev:  0.0089 m (frame 187)
  Velocity RMSE:     0.0156 m/s
  AngVel RMSE:       0.0034 rad/s
  Rotation RMSE:     0.12 deg
  Drift rate:        0.0012 m/s (linear)

  Per-axis position RMSE:  X=0.0018  Y=0.0005  Z=0.0021
  Per-axis velocity RMSE:  X=0.0134  Y=0.0023  Z=0.0098
```

### Plotting (--plot flag)

For each scenario + object, generate:
1. **Position overlay** — Unity vs Sim position over time (3 subplots for X, Y, Z)
2. **Velocity overlay** — Same for velocity
3. **Error over time** — Position error magnitude per frame (shows drift)
4. **Trajectory 2D** — Bird's-eye view (X vs Z) of both trajectories overlaid

Save plots to `C:\Users\kyle\Developer\Sim2GameTest\plots\{scenario}_{object}.png`

---

## Execution Order

### Step 1: Project Setup
1. Create `C:\Users\kyle\Developer\Sim2GameTest\` directory
2. Copy `libs/` folder from `C:\Users\kyle\Developer\PuckGym\PuckGym\libs\`
3. Create `.csproj` and `.sln` (template from PuckGym)
4. Create `recordings/` and `plots/` subdirectories
5. Verify the mod builds and loads in-game (empty `OnEnable`, just log to console)

### Step 2: Telemetry Writer (C#)
1. Implement `TelemetryWriter.cs` — binary writer matching the format spec above
2. Unit test: write a dummy file, read it back, verify values match

### Step 3: First Scenario — PuckFreeSlide (C#)
1. Implement `ScenarioRunner.cs` — MonoBehaviour with FixedUpdate frame counting
2. Implement `PuckFreeSlide.cs` — teleport puck, zero players, capture 200 frames
3. Wire up F5 keybind to run it
4. Run in-game, verify `.bin` file is written with correct frame count

### Step 4: Telemetry Writer (C++)
1. Add `Source/Telemetry.h/cpp` to PuckSim with identical binary format
2. Add `--test` argument parsing to `main()`
3. Implement PuckFreeSlide in C++ — same initial conditions, 200 frames
4. Run, verify `.bin` file is written

### Step 5: Python Comparison
1. Write `compare.py` with binary reader and all metrics
2. Compare the two PuckFreeSlide telemetry files
3. This gives the **first concrete accuracy number** for the simplest case

### Step 6: Remaining Scenarios
1. Implement scenarios one at a time in BOTH C# and C++
2. Run comparison after each
3. Prioritize: PuckFreeSlide > PlayerForward > PlayerTurn > PlayerHover > PuckWallBounce > others

### Step 7: Analysis and Fixes
1. Identify which subsystems have the largest error
2. Document specific divergences (e.g., "puck Y position drifts 0.02m over 4 seconds due to different ice contact resolution")
3. Feed findings back to PuckSim for Phase 11 fixes

---

## Critical Implementation Notes

### For the C# Mod Developer

1. **Host a private match** to test. You need server authority to manipulate rigidbodies. Start a local game, not a public server.

2. **Finding game objects at runtime:**
   - Puck: `PuckManager` singleton has references to spawned pucks
   - Players: `GameManager` has player references, or use `FindObjectsOfType<PlayerBodyV2>()`
   - Sticks: Each `PlayerBodyV2` has a reference to its `Stick` component
   - Look at PuckGym's `RLController.GatherObservation()` for working examples of accessing these

3. **FixedUpdate timing:** Capture telemetry in `FixedUpdate()`, not `Update()`. Physics state is only consistent between physics steps.

4. **Disabling normal gameplay during scenarios:** Set player inputs to zero, prevent puck respawning, freeze the game clock. See PuckGym's approach — it patches `PlayerInput.UpdateInputs` to block normal input and patches `PuckManager.Server_SpawnPucksForPhase` to prevent puck spawning.

5. **Quaternion conventions:** Unity uses (x, y, z, w) component order. Jolt also uses (x, y, z, w) internally. Verify both sides write in the same order.

6. **Unity's linearVelocity vs velocity:** Unity 6+ uses `Rigidbody.linearVelocity` (the old `.velocity` property was renamed). The game's decompiled code uses `.linearVelocity`.

### For the C++ PuckSim Developer

1. **Build system:** PuckSim uses CMake. The build commands are:
   ```
   cmake -S "C:/Users/kyle/Developer/PuckSim" -B "C:/Users/kyle/Developer/PuckSim/out/build" -G "Visual Studio 17 2022" -A x64
   cmake --build "C:/Users/kyle/Developer/PuckSim/out/build" --config Release
   ```

2. **The full environment must be created** for every scenario, even puck-only tests. The puck collides with ice and boards. Create rink, goals, and puck at minimum. Players and sticks only when the scenario needs them.

3. **Post-step processing is critical.** After `physics_system.Update()`, you MUST run:
   - Net damping queue processing
   - Stick exit queue processing (Y-correction + clamps)
   - Stick contact tracking (tensor switching)
   - Ground check (+ COM torque)

   See `PuckSim.cpp`'s main loop for the exact order.

4. **Contact listener setup:** The contact listener needs `puckBodyId`, `attackerStickId`, `goalieStickId` set before physics runs. Even in puck-only scenarios, set these (the stick IDs can be invalid BodyIDs if no sticks are created — just don't create stick scenarios without sticks).

5. **Jolt uses `GetPosition` for transform position.** This matches Unity's `Rigidbody.position`. Do NOT use `GetCenterOfMassPosition` — that includes the shape's COM offset.

6. **Determinism:** Use `JobSystemSingleThreaded` instead of `JobSystemThreadPool` for test scenarios. The thread pool introduces non-determinism from job scheduling order. Single-threaded is slower but gives reproducible results for A/B comparison.

### For the Python Developer

1. **Endianness:** Both platforms are x86/x64, so all floats are little-endian. No byte swapping needed.

2. **Frame alignment:** Both telemetry files should have the same frame count for a given scenario. If they differ, flag it as an error.

3. **Quaternion distance:** Use `2 * arccos(min(1, |dot(q1, q2)|))` to get the angle between two quaternions in radians. The `min(1, ...)` clamp handles floating point imprecision.

4. **Drift classification:** If position error grows linearly with time, it's a velocity bias. If it grows quadratically, it's an acceleration bias. If it's bounded, the systems are equivalent within solver noise.

---

## Scenario ID Enum (shared across all three components)

Use consistent IDs everywhere:

| ID | Name | Objects | Frames |
|----|------|---------|--------|
| 0 | puck_free_slide | Puck | 200 |
| 1 | puck_wall_bounce | Puck | 200 |
| 2 | puck_stick_exit | Puck, Stick | 100 |
| 3 | player_forward | Player | 500 |
| 4 | player_turn | Player | 300 |
| 5 | player_lateral | Player | 300 |
| 6 | player_hover | Player | 300 |
| 7 | stick_tracking | Player, Stick | 200 |
| 8 | full_play | Player, Mesh, Puck, Stick | 500 |

---

## Success Criteria

The pipeline is complete when:
1. At least scenarios 0, 3, and 4 (puck slide, player forward, player turn) run in both Unity and PuckSim
2. `compare.py` produces RMSE numbers for all three
3. You can articulate WHERE the sim diverges and by HOW MUCH

Stretch goals:
- All 9 scenarios running
- Matplotlib plots showing trajectory overlays
- Drift rate quantified for each subsystem
- Specific recommendations for Phase 11 fixes

---

## Reference File Locations

| Resource | Path |
|----------|------|
| PuckSim source | `C:\Users\kyle\Developer\PuckSim\Source\` |
| PuckSim physics values | `C:\Users\kyle\Developer\PuckSim\docs\physics-values.md` |
| PuckGym mod (reference) | `C:\Users\kyle\Developer\PuckGym\PuckGym\` |
| PuckCapture mod (reference) | `C:\Users\kyle\Developer\PuckGym\PuckCapture\` |
| Game decompiled source | `C:\Users\kyle\Developer\PuckMods\Puck\` |
| AssetRipper dump | `C:\Users\kyle\Downloads\AssetRipper_win_x64\PuckDumpedAssets\` |
| Mod DLLs (libs) | `C:\Users\kyle\Developer\PuckGym\PuckGym\libs\` |
| Game plugins folder | `C:\Program Files (x86)\Steam\steamapps\common\Puck\Plugins\` |
| Template mod | `C:\Users\kyle\Developer\PuckMods\TemplateMod\` |
