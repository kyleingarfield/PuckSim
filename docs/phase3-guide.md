# Phase 3: Goal Sensors and Contact Listener

## Step 1: Create two goal trigger bodies

Add these after the puck creation code. Each is a static sensor on the `GOAL_TRIGGER` layer.

Shape half-extents come from the mesh AABB (3.03 W x 1.49 H), with Z thickened from 0.025 to 0.25 to prevent tunneling at high puck speeds.

```
BodyCreationSettings (for each goal trigger):
  - shape: BoxShape(Vec3(1.515f, 0.745f, 0.25f))
  - motionType: EMotionType::Static
  - objectLayer: Layers::GOAL_TRIGGER
  - mIsSensor: true
```

| Goal | Position |
|------|----------|
| Blue (far) | `RVec3(0, 0.745, 34)` |
| Red (near) | `RVec3(0, 0.745, -34)` |

Use `CreateAndAddBody` with `EActivation::DontActivate` (they're static).

## Step 2: Create the puck trigger body

This is a kinematic sensor that follows the puck. It must be kinematic (not static) because Jolt does not generate contacts between two static bodies.

```
BodyCreationSettings:
  - shape: SphereShape(0.15f)
  - position: RVec3(0, 0.2, 0)   (same as puck start)
  - rotation: Quat::sIdentity()
  - motionType: EMotionType::Kinematic
  - objectLayer: Layers::PUCK_TRIGGER
  - mIsSensor: true
```

Use `CreateAndAddBody` with `EActivation::Activate`.

Store the returned `BodyID` as `puck_trigger_id`.

## Step 3: Update the simulation loop

Each tick, **before** calling `physics_system.Update()`, sync the puck trigger position to the puck:

```cpp
body_interface.SetPosition(puck_trigger_id, body_interface.GetPosition(puck_id), EActivation::DontActivate);
```

## Step 4: Update the contact listener

In `MyContactListener::OnContactAdded`, check if the contact is between a goal trigger and puck trigger by comparing object layers:

```
ObjectLayer layer1 = inBody1.GetObjectLayer();
ObjectLayer layer2 = inBody2.GetObjectLayer();

if ((layer1 == Layers::GOAL_TRIGGER && layer2 == Layers::PUCK_TRIGGER) ||
    (layer1 == Layers::PUCK_TRIGGER && layer2 == Layers::GOAL_TRIGGER))
{
    // Determine which goal by checking Z position of the goal trigger body
    const Body& goal_body = (layer1 == Layers::GOAL_TRIGGER) ? inBody1 : inBody2;
    float goal_z = goal_body.GetPosition().GetZ();

    if (goal_z > 0)
        cout << "GOAL SCORED — Blue goal (far, +Z)" << endl;
    else
        cout << "GOAL SCORED — Red goal (near, -Z)" << endl;
}
```

Note: `OnContactAdded` fires from Jolt's job threads. A `cout` is fine for now, but in later phases you'll need `std::atomic` for any shared goal state variables.

## Step 5: Change puck initial velocity

Aim the puck straight at the far goal to test:

```cpp
body_interface.SetLinearVelocity(puck_id, Vec3(0.0f, 0.0f, 15.0f));
```

## Step 6: Update cleanup

Add `RemoveBody` and `DestroyBody` calls at the end of `main()` for the two goal triggers and the puck trigger, same as you already do for the puck and ice.

## What to expect

- Puck starts at Z=0 moving at 15 m/s toward +Z
- Damping slows it, but 15 m/s is fast enough to reach Z=34
- When the puck trigger overlaps the blue goal trigger, `OnContactAdded` fires
- Console prints "GOAL SCORED — Blue goal (far, +Z)"
- Puck passes through the trigger (no physics deflection — it's a sensor)
- Puck eventually hits the far wall (Z=31.3) and bounces back, or reaches the goal at Z=34 (which is beyond the wall — the trigger is behind the net, as in the real game)

## Important note on goal position vs wall position

The goals are at Z=34 and Z=-34, but the far wall is at Z=31.3. In the real game, there are openings in the boards for the goal mouths. For this test, you have two options:

1. **Move the far wall back** past Z=34.25 temporarily so the puck can reach the goal trigger
2. **Remove the far wall** temporarily

Pick whichever is simpler. You'll replace the placeholder walls with proper rink geometry (including goal openings) in a later phase.

## After verification

- Restore the original puck velocity `(10.0f, 0.0f, 5.0f)` or keep `(0, 0, 15)` — your choice
- The goal triggers and puck trigger stay permanently — they're part of the game
