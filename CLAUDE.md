# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PuckSim is a headless physics simulation replicating the physics of **Puck**, a physics-based hockey game. It uses **Jolt Physics** (C++) to enable large-scale parallelized simulations for training reinforcement learning agents.

## Role Guidelines

The user is learning Jolt Physics and refreshing their C++ skills. **Do not write code.** Instead, teach, explain, and guide. Help them understand concepts, debug issues, and make architectural decisions — but let them write every line themselves. The ONE exception is build system files (CMake), which the user has delegated.

## Build System

- **CMake 3.20+** generating Visual Studio 2022 solution
- **Jolt Physics** included as a git submodule at `extern/JoltPhysics/`
- C++17, RTTI and exceptions disabled, AVX2 enabled

### Generate VS solution
```
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64
```

### Build from command line
```
cmake --build out/build --config Release
```

Or open `out/build/PuckSim.sln` in Visual Studio and build from there.

## Architecture

Single-file simulation currently in `Source/PuckSim.cpp`. Will be split into multiple files around Phase 4-5.

- **20 object layers** matching the original game's Unity collision matrix (defined in `Layers` namespace)
- **3 broadphase layers**: STATIC, DYNAMIC, SENSOR
- **Table-based collision filters**: `ObjectLayerPairFilterTable`, `BroadPhaseLayerInterfaceTable`, `ObjectVsBroadPhaseLayerFilterTable`
- **Physics tick rate**: 50 Hz (dt = 0.02s), matching the original game
- All physics values extracted from decompiled source and AssetRipper dumps — documented in `docs/physics-values.md`

## Key Constraint

All physics values must be **exact** replications from the decompiled game source. No guessing, approximating, or "eyeballing". The only acceptable deviations are inherent differences between PhysX (Unity) and Jolt engines.
