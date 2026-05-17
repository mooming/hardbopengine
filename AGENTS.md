# AGENTS.md - HardBop Engine Developer Guide

**This document is mandatory for all AI agents working on this codebase.**
Every instruction below is a binding rule, not a suggestion. Violations undermine
consistency, reliability, and the project's quality standards. If you cannot
follow a rule, state why and ask for guidance before proceeding.

## Project Overview

HardBop Engine is a high-performance C++23 engine focused on modularity and efficiency. It utilizes custom memory management, an advanced task system, and a robust abstraction layer (OSAL) for cross-platform compatibility.

## Important Work Policy

**These rules are binding. You must follow every applicable rule in order. If you cannot comply, explain why and ask for guidance before proceeding.**

  1. **Understand goals first** — Read and fully grasp the objectives before diving into any task.
  2. **Suggest improvements** — After understanding the goals, propose ideas that could make the outcome better. Continue to do this throughout the process.
  3. **Create a checklist** — Once the user confirms your plan, create a structured task list for the goals.
  4. **Avoid unnecessary work** — Only do what was explicitly asked; don't add scope creep.
  5. **Self-review and improve** — Review your own work before presenting it, and try to refine the outcome before asking for feedback.
  6. **Verify functionality** — Always confirm things work as expected by building the project and running tests. When appropriate, offer test methods or functions to the user.
  7. **Update the checklist** — Mark tasks as completed, and provide a brief summary of what was done.
  8. **Create commits** — Always commit after finishing a task.
  9. **No push without approval** — Stage and commit only. Never run `git push` (or an equivalent command) until the user explicitly tells you to. Do not submit or release anything without explicit go-ahead.
 10. **Record progress** — Log useful information and progress with timestamps in `JOURNAL.md`. Keep it compact and well-organized with a top-level summary section.
 11. **Read the summary first** — Always check the top-level summary of `JOURNAL.md` before starting new work.
 12. **Follow the coding standards** — Always make sure any code you write or modify adheres to the conventions documented in `docs/CodingStandards.md` and demonstrated in `Engine/CodingStandards.h` and `Engine/CodingStandards.cpp`. Treat these files as the authoritative reference for style, structure, and best practices across the entire codebase.

## Build System

### Building Tools
All tools located in the `Tools/` directory can be compiled and installed to `bin/` using:
```bash
./Tools/BuildAllTools.sh
```

### CMake Configuration
The project uses a custom tool, `MakeBuild`, to manage CMake configurations. 
**Important:** Always use `MakeBuild` when modifying `CMakeLists.txt` files.

Read `Tools/MakeBuild/README.md` for detailed usage instructions.

Common build types include `Debug`, `Dev`, and `Release`. You can configure CMake using:
```bash
cmake --fresh -B build -G "Ninja Multi-Config" -S .
```

### Building Targets
To build the project with a specific configuration:
```bash
# Build with Debug configuration
cmake --build build --config Debug

# Build with Dev configuration
cmake --build build --config Dev

# Build with Release configuration (default)
cmake --build build --config Release
```

### Running Tests
Tests are registered in `Engine/Test/UnitTestCollection.cpp`. 

To run all tests:
```bash
./build/Applications/EngineTest/<Config>/EngineTest
```
*(Replace `<Config>` with `Debug`, `Dev`, or `Release`)*

To run a specific test collection, you must temporarily comment out the unwanted collections in `Engine/Test/UnitTestCollection.cpp`'s `RunTests()` function, rebuild, and execute.
