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
  2. **Suggest improvements** — After understanding the goals, propose ideas that could make the outcome better.
  3. **Create a checklist** — Once the user confirms your plan, create a structured task list for the goals.
  4. **Avoid unnecessary work** — Only do what was explicitly asked; don't add scope creep.
  5. **Self-review and improve** — Review your own work before presenting it, and try to refine the outcome before asking for feedback.
  6. **Verify functionality** — Always confirm things work as expected by building the project and running tests. When appropriate, offer test methods or functions to the user.
  7. **Update the checklist** — Mark tasks as completed, and provide a brief summary of what was done.
  8. **Create commits** — Always commit after finishing a task.
  9. **No premature submission** — Never submit or push anything without the user's explicit go-ahead.
 10. **No push without approval** — Stage and commit only. Never run `git push` (or an equivalent command) until the user explicitly tells you to.
 11. **Record progress** — Log useful information and progress with timestamps in `JOURNAL.md`.
 12. **Keep JOURNAL.md organized** — Maintain a compact, well-organized file with a top-level summary section for easy navigation.
 13. **Read the summary first** — Always check the top-level summary of `JOURNAL.md` before starting new work.
 14. **Keep suggesting improvements** — Continue to propose enhancements as you work through tasks.
 15. **Follow the coding standards** — Always make sure any code you write or modify adheres to the conventions documented in `docs/CodingStandards.md` and demonstrated in `Engine/CodingStandards.h` and `Engine/CodingStandards.cpp`. Treat these files as the authoritative reference for style, structure, and best practices across the entire codebase.

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

For detailed coding standards, including naming conventions, class design, and formatting rules, please refer to [docs/CodingStandards.md](./docs/CodingStandards.md).

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

### File Organization
*   **Headers**: `.h` extension.
*   **Implementation**: `.cpp` extension.
*   **Module Structure**: Modules reside in `Engine/<ModuleName>/` (e.g., `Memory`, `Math`, `String`).
*   **Copyright**: Every file must start with a copyright notice:
    `// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.`
