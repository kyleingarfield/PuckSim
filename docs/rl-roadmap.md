# RL Roadmap

Path from current state (Phase 6 complete) to first RL training run. Curriculum learning Phase 1: WASD movement only, no advanced mechanics.

---

## Phase 7 — Accurate Goal Geometry + Net Damping

**New file: `Goal.h` / `Goal.cpp`**

Build out complete goal structures (both blue and red) with all collider geometry from MeshData.h.

### Goal Posts
- 3 capsules per goal (2 vertical posts + 1 crossbar)
- Post radius: 0.06, heights: 3.11 (posts), 2.0 (crossbar)
- Post centers: (0, 1.15, -1.56) for posts, (+/-1.5, 1.15, -0.62) for crossbar
- Layer: GOAL_POST, static

### Goal Frame
- Non-convex MeshShape from GOAL_FRAME_VERTICES (1,128 verts, 668 tris)
- Layer: GOAL_FRAME, static
- Coordinate transform needed (verify against prefab transform)

### Goal Net Collider
- Non-convex MeshShape from GOAL_NET_COLLIDER_VERTICES (276 verts, 152 tris)
- Layer: GOAL_NET, static
- **Net damping**: when puck is inside net, clamp linear/angular velocity to 2 m/s magnitude, apply 0.25 damping coefficient (from GoalNetCollider.cs)
- Requires contact listener logic to detect puck-inside-net state

### Goal Player Collider
- Convex hull from GOAL_PLAYER_COLLIDER_VERTICES (73 verts)
- Layer: PLAYER_COLLIDER, static
- Prevents players from entering the goal crease area

### Goal Trigger Upgrade
- Replace current BoxShape triggers with ConvexHullShape from GOAL_TRIGGER_VERTICES (24 verts)
- Same sensor behavior, more accurate shape

### Deliverable
- Both goals fully constructed with all sub-components
- Net damping functional (puck slows inside net)
- All transforms verified against AssetRipper prefab data

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
