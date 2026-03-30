# RL Roadmap

Path from current state (Phase 6 complete) to first RL training run. Curriculum learning Phase 1: WASD movement only, no advanced mechanics.

---

## Phase 7 — Accurate Goal Geometry + Net Damping

**New file: `Goal.h` / `Goal.cpp`**

Build out complete goal structures (both blue and red). Move goal triggers from Rink into Goal. See `docs/phase7-guide.md` Part E for full verified details.

### Goal Container Transforms (verified against level_1.unity)
- Red: position (0, 0, -34.1), no rotation, scale (0.9, 0.9, 0.8)
- Blue: position (0, 0, 34.1), 180° Y rotation, scale (0.9, 0.9, 0.8)
- All children: local pos (0,0,0), 90° X rotation, scale (1,1,1)

### Goal Posts (Layer 11)
- 3 capsules per goal (1 crossbar + 2 depth posts)
- Capsule values in local space, must apply full transform chain
- Post radius: 0.06, heights: 3.11 (crossbar along X), 2.0 (posts along Z)
- Centers: (0, 1.15, -1.56) crossbar, (±1.5, 1.15, -0.62) posts

### ~~Goal Frame (Layer 18)~~ — SKIP
- Frame MeshCollider is **DISABLED** in both goals (m_Enabled: 0)
- Visual only, no physics collision. Do not create.

### Goal Net Collider (Layer 14)
- Non-convex MeshShape from GOAL_NET_COLLIDER_VERTICES (276 verts, 152 tris)
- Same transform chain as other children (90° X rotation + parent scale + parent pos/rot)
- Physics material: Goal Net (friction 0, bounciness 0)
- **Net damping** (OnCollisionEnter / OnContactAdded — fires once per contact):
  1. Skip if puck is grounded
  2. Linear velocity × 0.75
  3. Angular velocity × 0.75
  4. Clamp linear to 2 m/s, angular to 2 rad/s

### Goal Player Collider (Layer 16)
- Convex hull from GOAL_PLAYER_COLLIDER_VERTICES (73 verts)
- Prevents players from entering goal area

### Goal Trigger (Layer 15)
- Replace current BoxShape triggers with ConvexHullShape from GOAL_TRIGGER_VERTICES (24 verts)
- IsTrigger / sensor, same scoring detection

### Deliverable
- Both goals fully constructed (4 bodies each: posts, net, trigger, player collider)
- Net damping functional in contact listener
- Goal triggers moved from Rink to Goal, Rink struct simplified

---

## Phase 8 — Player Movement + Keep-Upright PID

**WASD only. No sprint, stamina, jump, twist, dash, slide, or tackle.**

### Basic Movement
- Forward/backward acceleration (2.0 / 1.8)
- Speed caps: attacker 7.5 / goalie 5.0 (forward), attacker 7.5 / goalie 5.0 (backward)
- Drag: 0.015 (normal), 0.25 (overspeed)
- Brake acceleration: 5.0
- Lateral movement with laterality scaling (min 0.5 at 5 m/s, max 1.0 at 2 m/s)
- Turn acceleration: 1.5, turn brake: 4.5, max turn speed: 1.3125
- Turn drag: 3.0, turn overspeed drag: 2.25
- Traction: 12.5

### Keep-Upright PID
- P=50, D=8 (derivative-on-measurement)
- Keeps player body vertical during movement
- Applied as torque around X and Z axes

### Skate System
- Traction value of 12.5 from prefab
- Velocity lean: linear force mult 0.75, angular force mult 6

### Deliverable
- Players respond to directional input (forward, backward, left, right, turn)
- Players remain upright during movement
- Speed caps and drag behave correctly
- No sprint/stamina — constant base speed

---

## Phase 9 — Puck Polish + Stick Verification

### Puck
- Puck Y-axis correction on stick exit (5x downward acceleration if upward velocity)
- Puck grounded center-of-mass shift (0, -0.01, 0) when ice detected via sphere check (r=0.075)
- Puck inertia tensor switching on stick contact (if time permits, otherwise defer)
- Max angular speed: 60

### Stick Verification
- Verify angular velocity transfer back to player is correct (scale (0.5, 1, 0) * 0.3 multiplier)
- Verify Z-rotation constraint (zero out local Z angular velocity, correct euler Z to 0)
- Verify blade gain multiplier behavior on stick-stick contact

### Deliverable
- Puck behaves correctly on stick contact/release
- Stick mechanics match game behavior
- All easy-win puck/stick features implemented

---

## Phase 10 — Validation Pipeline

### Unity Mod (Real-Time Comparison)
- Build a BepInEx/MelonLoader mod for the live game
- Each FixedUpdate, log: positions, velocities, rotations, forces for all physics objects (puck, players, sticks)
- Output to file or pipe (CSV or binary format)

### PuckSim Replay Mode
- Add ability to feed the same inputs/forces into PuckSim
- Run identical scenarios (same initial conditions, same input sequences)
- Output matching telemetry format

### Comparison Tool
- Script to diff Unity vs PuckSim telemetry
- Per-object, per-axis error metrics (RMSE, max deviation, drift over time)
- Flag any divergence > threshold

### Deliverable
- Concrete accuracy numbers for every physics subsystem
- List of discrepancies to fix before RL training

---

## Phase 11 — Fix Discrepancies

Address whatever Phase 10 surfaces. Likely candidates:
- PID tuning differences (Jolt vs Unity solver behavior)
- Collision response differences (restitution/friction combine modes)
- Damping model differences
- Numerical drift over long episodes

---

## Phase 12 — RL Interface

### Environment Wrapper
- C++ env class: reset(), step(actions), observe()
- State space: positions, velocities, rotations for puck, players, sticks; score; game clock
- Action space (Phase 1 curriculum): 4 discrete or 2 continuous (forward/backward + left/right)
- Reward signal: goal scored (+1/-1), puck possession, puck proximity (curriculum-dependent)

### Python Binding
- pybind11 or socket-based IPC to gymnasium/stable-baselines3
- Vectorized envs for parallel training
- Episode management: reset after goal, time limit, score limit

### Game Logic
- Faceoff / puck reset after goals
- Period clock
- Score tracking
- Basic game state machine (playing, goal scored, faceoff, period end)

### Deliverable
- Functional gymnasium-compatible environment
- Agents can be trained with WASD-only action space
- First training runs
