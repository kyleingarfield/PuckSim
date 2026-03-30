# PuckSim

Headless physics simulation replicating the physics of [Puck](https://store.steampowered.com/app/2150490/Puck/), a physics-based hockey game. Built with [Jolt Physics](https://github.com/jrouwe/JoltPhysics) in C++ for training reinforcement learning agents through large-scale parallelized simulations.

All physics values are extracted directly from the decompiled game source and Unity asset dumps (prefabs are authoritative over .cs defaults) to ensure exact replication of in-game behavior.

## Status

Work in progress. Current implementation:

- **Rink** — Ice surface, oval barrier boards (real mesh collider, 256 verts), goal trigger sensors
- **Puck** — Convex hull from game mesh, correct mass/damping/speed caps, kinematic trigger for goal detection
- **Players** — Attacker and goalie with distinct parameters, compound collider (capsule + head sphere), hover PID controller, kinematic mesh body for puck deflection, groin/torso convex hull meshes
- **Sticks** — Compound shaft + blade convex hulls, dual Vector3PID controllers (shaft and blade handles), player-to-stick velocity coupling, Z-rotation constraint, angular velocity transfer
- **Collision** — 20 object layers, 3 broadphase layers, full collision matrix matching the original game
- **Goals** — Sensor-based goal detection via contact listener

See [docs/rl-roadmap.md](docs/rl-roadmap.md) for the path from here to RL training.

## Prerequisites

- **Visual Studio 2022** (Community or higher) with the **Desktop development with C++** workload
  - MSVC v143 toolset (installed by default with the C++ workload)
  - Windows SDK (installed by default with the C++ workload)
- **CMake 3.20+** (bundled with VS 2022, or install separately)
- **Git** (for cloning with submodules)

## Building

Clone with submodules:

```
git clone --recurse-submodules https://github.com/kyleingarfield/PuckSim.git
cd PuckSim
```

Generate the Visual Studio solution:

```
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64
```

If `cmake` is not on your PATH, use the one bundled with VS:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B out/build -G "Visual Studio 17 2022" -A x64
```

Then either:

- Open `out/build/PuckSim.sln` in Visual Studio, set **PuckSim** as startup project, and build/run
- Or build from command line: `cmake --build out/build --config Release`

## Build Configuration

- **C++17**, RTTI and exceptions disabled
- **AVX2** enabled (SSE4.1, SSE4.2, AVX, AVX2, LZCNT, TZCNT, F16C, FMA)
- **Interprocedural optimization** (LTO) enabled for Release/Distribution
- Jolt Physics included as a git submodule at `extern/JoltPhysics/`

## Project Structure

```
PuckSim/
  Source/
    PuckSim.cpp       Main simulation loop
    Layers.h/.cpp      20 object layers, broadphase mapping, collision matrix
    Rink.h/.cpp        Ice surface, barrier mesh, goal triggers
    Puck.h/.cpp        Puck body, trigger sync
    Player.h/.cpp      Player creation, hover PID, mesh sync
    Stick.h/.cpp       Stick compound shape, dual PID, velocity coupling
    Listeners.h        Contact listener (goal detection)
    JoltSetup.h/.cpp   Jolt initialization/shutdown
    MeshData.h         Auto-generated vertex/index arrays from game meshes
  docs/
    physics-values.md  Extracted physics values (corrected against prefabs)
    rl-roadmap.md      Implementation plan through RL training
    phase*-guide.md    Per-phase implementation guides
    known-issues.md    Resolved issues and deferred items
  tools/
    extract_meshes.py  Mesh extraction from game assets
  extern/
    JoltPhysics/       Jolt Physics library (git submodule)
  CMakeLists.txt       Build configuration
```

## License

This project is for educational and research purposes. Jolt Physics is used under its [MIT license](https://github.com/jrouwe/JoltPhysics/blob/master/LICENSE).
