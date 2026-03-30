# CLAUDE.md

## Project Overview

PuckSim is a headless Jolt Physics (C++) simulation replicating the physics of **Puck**, a physics-based hockey game, for training reinforcement learning agents.

## Role Guidelines

The user is learning Jolt Physics and refreshing C++ skills. **Do not write code.** Teach, explain, and guide — let them write every line. The ONE exception is build system files (CMake).

## Build System

- **CMake 3.20+** → Visual Studio 2022, C++17, no RTTI/exceptions, AVX2
- **Jolt Physics** submodule at `extern/JoltPhysics/`
- CMake path: `"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"`
- Generate: `"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" -S "C:/Users/kyle/Developer/PuckSim" -B "C:/Users/kyle/Developer/PuckSim/out/build" -G "Visual Studio 17 2022" -A x64`
- Build: `"/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build "C:/Users/kyle/Developer/PuckSim/out/build" --config Release`

## Architecture

- `Source/PuckSim.cpp` — main(), sim loop, rink/puck/goal setup
- `Source/Player.h` — PlayerParams, Player struct, function declarations
- `Source/Player.cpp` — player creation, hover PID, mesh sync, cleanup
- `Source/Stick.h` — StickParams, Vector3PID, Stick struct, function declarations
- `Source/Stick.cpp` — stick creation (compound shape), dual PID control, velocity coupling, Z-rotation constraint
- 20 object layers, 3 broadphase layers, table-based collision filters, 50 Hz tick rate

## Key Constraint: Prefab Values Are Authoritative

All physics values must be **exact** replications from the game. **Never use .cs source defaults** — Unity prefabs override them. The `.prefab` files from AssetRipper are the ground truth for runtime values. When .cs defaults and prefab values conflict, the prefab wins.

## Reference Data

- `docs/physics-values.md` — All extracted physics values (corrected against prefabs)
- `docs/phase*-guide.md` — Implementation guides per phase
- Decompiled source: `C:/Users/kyle/Developer/PuckMods/Puck/` — .cs files with code logic and default values (not authoritative for runtime values; use for understanding game logic, algorithms, and behavior)
- AssetRipper dump: `C:/Users/kyle/Downloads/AssetRipper_win_x64/PuckDumpedAssets/` — Full Unity asset export including .prefab files, scene files (.unity), mesh assets, physics materials, and transforms. **Authoritative** for runtime physics values, transforms, collider dimensions, and hierarchy. Use to verify coordinate transforms, scales, rotations, and parent hierarchies.
