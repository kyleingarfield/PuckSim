# PuckSim

Headless physics simulation replicating the physics of [Puck](https://store.steampowered.com/app/2150490/Puck/), a physics-based hockey game. Built with [Jolt Physics](https://github.com/jrouwe/JoltPhysics) in C++ for eventual use in training reinforcement learning agents through large-scale parallelized simulations.

All physics values are extracted directly from the decompiled game source and Unity asset dumps to ensure exact replication of in-game behavior.

## Status

Work in progress. Current state: empty rink with sliding puck and full 20-layer collision matrix matching the original game.

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
  Source/PuckSim.cpp    Main simulation source
  docs/
    physics-values.md   Extracted physics values from decompiled source
    phase1-guide.md     Phase 1 implementation guide
    phase2-guide.md     Phase 2 implementation guide
  extern/
    JoltPhysics/        Jolt Physics library (git submodule)
  CMakeLists.txt        Build configuration
```

## License

This project is for educational and research purposes. Jolt Physics is used under its [MIT license](https://github.com/jrouwe/JoltPhysics/blob/master/LICENSE).
