# Phase 1: Empty Rink with Sliding Puck

## Step 1: Rename and update build
- Rename `Source/HelloWorld.cpp` → `Source/PuckSim.cpp`
- In `CMakeLists.txt`, change the `add_executable` line from `Source/HelloWorld.cpp` to `Source/PuckSim.cpp`

## Step 2: Strip down to skeleton
Keep from HelloWorld.cpp:
- All `#include` lines
- `TraceImpl` and `AssertFailedImpl` callbacks
- `Layers` namespace (`NON_MOVING = 0`, `MOVING = 1`)
- `ObjectLayerPairFilterImpl`, `BPLayerInterfaceImpl`, `ObjectVsBroadPhaseLayerFilterImpl`
- In `main()`: everything from `RegisterDefaultAllocator()` through `PhysicsSystem::Init()`. **Delete everything after that** (the floor, sphere, simulation loop).

## Step 3: Create the ice surface
Static `BoxShape` with half-extents `(15, 0.0125, 30.5)` positioned at `(0, 0.0125, 0)`.

```
BodyCreationSettings:
  - shape: BoxShape(Vec3(15.0f, 0.0125f, 30.5f))
  - position: RVec3(0, 0.0125, 0)
  - rotation: Quat::sIdentity()
  - motionType: EMotionType::Static
  - objectLayer: Layers::NON_MOVING
  - friction: 0
  - restitution: 0
```

## Step 4: Create 4 barrier walls
Thin static boxes around the perimeter of the ice (30 wide x 61 long). Each wall should be tall enough that the puck can't escape.

Example wall placement (half-extents and positions):

| Wall | Half-Extents | Position | Notes |
|------|-------------|----------|-------|
| +Z (far end) | `(15.4, 12.5, 0.4)` | `(0, 12.5, 31.3)` | Slightly wider than ice to close corners |
| -Z (near end) | `(15.4, 12.5, 0.4)` | `(0, 12.5, -31.3)` | Mirror of +Z |
| +X (right) | `(0.4, 12.5, 30.5)` | `(15.4, 12.5, 0)` | Along the length |
| -X (left) | `(0.4, 12.5, 30.5)` | `(-15.4, 12.5, 0)` | Mirror of +X |

```
BodyCreationSettings (for each wall):
  - motionType: EMotionType::Static
  - objectLayer: Layers::NON_MOVING
  - friction: 0
  - restitution: 0.2
```

Note: The exact wall positions/sizes don't need to be perfect. The key is that they enclose the ice surface and have the right restitution (0.2). You'll refine the geometry in later phases.

## Step 5: Create the puck

```
BodyCreationSettings:
  - shape: SphereShape(0.152f)
  - position: RVec3(0, 0.2, 0)
  - rotation: Quat::sIdentity()
  - motionType: EMotionType::Dynamic
  - objectLayer: Layers::MOVING
  - friction: 0
  - restitution: 0
  - linearDamping: 0.3f
  - angularDamping: 0.3f
  - maxLinearVelocity: 30.0f
  - overrideMassProperties: EOverrideMassProperties::CalculateInertia
  - massPropertiesOverride.mMass: 0.375f
```

After creating and adding the puck, set initial velocity:
```cpp
body_interface.SetLinearVelocity(puck_id, Vec3(10.0f, 0.0f, 5.0f));
```

## Step 6: Simulation loop

```
deltaTime = 0.02f  (50 Hz, matching the game)
collisionSteps = 1

Loop while puck is active:
  1. physics_system.Update(deltaTime, collisionSteps, &temp_allocator, &job_system)
  2. Every 50 steps (every 1 second game time), print:
     - Step number
     - Puck position (x, y, z)
     - Puck velocity magnitude
  3. Stop when body_interface.IsActive(puck_id) returns false
```

## What to expect
- Puck starts at (0, 0.2, 0) moving at (10, 0, 5)
- It drops slightly to rest on the ice surface (~Y = 0.177 for sphere radius 0.152 on ice at Y = 0.025)
- Velocity decreases over time due to damping (0.3)
- Puck bounces off walls — velocity component flips, reduced by restitution factor
- Eventually puck slows enough to sleep (Jolt default sleep threshold)

## After it compiles and runs
- Verify Y position stabilizes (puck stays on ice, doesn't fall through)
- Verify velocity magnitude decreases each second
- Verify at least one wall bounce occurs (velocity component sign flip)
- Verify puck eventually sleeps

## Regenerating the VS solution
After changing CMakeLists.txt, regenerate:
```
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B out/build -G "Visual Studio 17 2022" -A x64
```
Then reopen `out/build/PuckSim.sln` in VS.
