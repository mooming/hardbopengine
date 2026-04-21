# AGENTS.md - HardBop Engine Developer Guide

This document provides essential information for developers and AI agents working on the HardBop Engine codebase.

## Project Overview

HardBop Engine is a high-performance C++23 engine focused on modularity and efficiency. It utilizes custom memory management, an advanced task system, and a robust abstraction layer (OSAL) for cross-platform compatibility.

## Build System

### Building Tools
All tools located in the `Tools/` directory can be compiled and installed to `bin/` using:
```bash
./Tools/BuildAllTools.sh
```

### CMake Configuration
The project uses a custom tool, `MakeBuild`, to manage CMake configurations. 
**Important:** Always use `MakeBuild` when modifying `CMakeLists.txt` files.

Read `Tool/MakeBuild/README.md` for detailed usage instructions.

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

## Development Workflow

1.  **Understand**: Read relevant source code and documentation (`docs/`) before making changes.
2.  **Implement**: Apply requested changes or new features.
3.  **Verify**: 
    *   Create or update test cases in `Engine/Test/`.
    *   Build the project to ensure no errors or warnings are introduced.
    *   Execute `EngineTest` to verify correctness.
4.  **Review**: Update `ReviewNote.txt` and other relevant documentation (e.g., `README.md`, `ReviewNoteSummary.txt`) if necessary.

## Code Style & Standards

### Formatting & Analysis
*   **Formatting**: Use `clang-format` to maintain consistency.
    ```bash
    find Engine/ Applications/ -name '*.h' -o -name '*.cpp' | xargs clang-format -i
    ```
*   **Static Analysis**: Use `clang-tidy` for code quality checks.
    ```bash
    # For a single file
    clang-tidy Engine/Memory/Memory.h -- -std=cmake++23 -I. -IEngine -IExternal

    # For the entire codebase
    find Engine/ Applications/ -name '*.h' -o -name '*.cpp' | xargs clang-tidy -header-filter='.*' -- -std=c++23 -I. -IEngine -IExternal
    ```

### Naming Conventions
*   **Classes, Functions, Types**: `PascalCase` (e.g., `MemoryManager`, `TLogOut`)
*   **Variables**: `camelCase` (e.g., `defaultValue`, `isDone`)
*   **Constants**: `PascalCase` with a prefix (eg., `MaxNameLength`)
*   **Macros**: `SCREAMING_SNAKE_CASE` (e.g., `__DEBUG__`)
*   **Namespaces**: `hbe`

### C++ Standards & Features
*   **C++23**: Utilize modern C++23 features where appropriate. Use `static_assert` for type validation in templates.
*   **Memory Management**: Prefer the engine's custom allocators (`PoolAllocator`, `StackAllocator`, etc.) via `MemoryManager`.
*   **Error Handling**: The project is **exception-free**. Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for runtime checks.
*   **Attributes**: Use `[[nodiscard]]`, `[[maybe_unused]]`, `explicit`, `final`, and `override` to improve code clarity and safety.

### File Organization
*   **Headers**: `.h` extension.
*   **Implementation**: `.cpp` extension.
*   **Module Structure**: Modules reside in `Engine/<ModuleName>/` (e.g., `Memory`, `Math`, `String`).
*   **Copyright**: Every file must start with a copyright notice:
    `// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.`

### General Patterns
*   Use `#pragma once` for header guards.
*   Prefer pass-by-reference over raw pointers.
*   Forward declare classes in headers whenever possible to reduce dependencies.
*   Place member variables first, followed by properties and methods (ordered by visibility: `public > protected > private`).
*   Use `constexpr` for compile-time constants.
