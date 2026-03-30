# CLAUDE.md

## Project Overview

PuckSim is a headless Jolt Physics (C++) simulation replicating the physics of **Puck**, a physics-based hockey game, for training reinforcement learning agents.

## Role Guidelines

The user is learning Jolt Physics and refreshing C++ skills. **Do not write code.** Teach, explain, and guide — let them write every line. The ONE exception is build system files (CMake).

## Build System

- **CMake 3.20+** → Visual Studio 2022, C++17, no RTTI/exceptions, AVX2
- **Jolt Physics** submodule at `extern/JoltPhysics/`
- Generate: `cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64`
- Build: `cmake --build out/build --config Release`

## Architecture

- `Source/PuckSim.cpp` — main(), sim loop, rink/puck/goal setup
- `Source/Player.h` — PlayerParams, Player struct, function declarations
- `Source/Player.cpp` — player creation, hover PID, mesh sync, cleanup
- 20 object layers, 3 broadphase layers, table-based collision filters, 50 Hz tick rate

## Key Constraint: Prefab Values Are Authoritative

All physics values must be **exact** replications from the game. **Never use .cs source defaults** — Unity prefabs override them. The `.prefab` files from AssetRipper are the ground truth for runtime values. When .cs defaults and prefab values conflict, the prefab wins.

## Reference Data

- `docs/physics-values.md` — All extracted physics values (corrected against prefabs)
- `docs/phase*-guide.md` — Implementation guides per phase
- Decompiled source: `~/Developer/PuckMods/Puck/` (.cs files — defaults only, not authoritative)
- AssetRipper dump: `~/Downloads/AssetRipper_win_x64/PuckDumpedAssets/AssetRipper_export_20260330_001745/` (prefabs — authoritative)
