# AGENTS.md - HardBop Engine Developer Guide

## Build Commands

### CMake Configuration
The project supports three build types: `Debug`, `Dev`, and `Release` (default).
```bash
cmake --fresh -B build -G "Ninja Multi-Config" -S .
```

### Build Targets
```bash
# Build with config options(Debug, Dev, Release)
cmake --build build --config Debug
cmake --build build --config Dev
cmake --build build --config Release
```

### Running All Tests
```bash
./build/Applications/EngineTest/Debug/EngineTest
./build/Applications/EngineTest/Dev/EngineTest
./build/Applications/EngineTest/Release/EngineTest
```

### Running a Single Test
Tests are registered in `Engine/Test/UnitTestCollection.cpp` via `TestEnv::AddTestCollection<T>()`.
To run a specific test:

1. Open `Engine/Test/UnitTestCollection.cpp`
2. Comment out unwanted test collections in the `RunTests()` function
3. Rebuild and run:

## Code Formatting

### Format Code
```bash
find Engine/ Applications/ -name '*.h' -o -name '*.cpp' | xargs clang-format -i
```

### Static Analysis (clang-tidy)
```bash
# Single file
clang-tidy Engine/Memory/Memory.h -- -std=c++23 -I. -IEngine -IExternal

# Entire codebase
find Engine/ Applications/ -name '*.h' -o -name '*.cpp' | xargs clang-tidy -header-filter='.*' -- -std=c++23 -I. -IEngine -IExternal
```
## Basic Workflow
- Understand the current code base by reading related source code first
- Do requested jobs
- Create or revise testcases to verify changes
- Build the project to ensure it doesn't have any errors or warnings. Fix errors and warnings.
- Execute EngineTest if you have changed the engine. And then, Review test results and fix errors and warnings.
- Update ReviewNote.txt files if necessary
- Update README.md and ReviewNoteSummary.txt files at the base directory if necessary

## Code Style Guidelines

### File Organization
- Header files: `.h` extension, implementation: `.cpp`
- Source file header: `// Created by email@example.com`
- Modules live in `Engine/<ModuleName>/` (Memory, Math, String, etc.)
- Custom types follow module naming (e.g., `Memory/Array.h` creates `hbe::Array`)

### Naming Conventions
- **Classes/Functions/Types**: PascalCase (e.g., `MemoryManager`, `ConfigParam`)
- **Variables**: camelCase (e.g., `defaultValue`, `isDone`)
- **Constants**: PascalCase with prefix (e.g., `MaxNameLength`)
- **Macros**: SCREAMING_SNAKE_CASE (e.g., `__DEBUG__`, `__UNIT_TEST__`)
- **Namespace**: `hbe` (HardBopEngine)
- **Internal types**: Prefix with `T` (e.g., `TLogOut`, `TAllocFunc`)

### Code Formatting (per .clang-format)
- **Indentation**: 4 spaces (UseTab: Always)
- **Column limit**: 120 characters
- **Brace wrapping**: BreakBeforeBraces: Custom (K&R variant)
- **Access modifiers**: Indented -4 relative to class
- **Template declarations**: Always break before `>`
- **Short functions**: InlineOnly (only in header if one-liner)
- **Readability**: Put an empty line before "return". Place two empty lines after include declerations.
- **TemplateType**: Make sure template type starts with 'T' letter. e.g. TIndex (O), Index (X)

### Import Organization
Order (per `.clang-format` IncludeCategories):
1. Standard library headers (`<cstddef>`, `<memory>`, etc.)
2. Local project headers (`"Memory/Memory.h"`)
3. Other

Example:
```cpp
#include "MemoryManager.h"
#include "Core/Debug.h"
#include "String/StaticString.h"
#include <cstddef>
#include <functional>
#include <memory>
```

### C++23 Type System
- Use `static_assert` to validate type sizes in allocator templates
- Use `std::conditional_t` for conditional type selection
- Use `std::enable_if` for template constraints
- Use `std::atomic` for thread-safe types
- Prefer `using` aliases over typedefs

### Attributes
- Use `[[nodiscard]]` for functions with important return values
- Use `[[maybe_unused]]` for intentionally unused parameters
- Use `explicit` for single-argument constructors
- Use `final` for classes not meant for inheritance
- Use `override` explicitly for virtual overrides

### Error Handling
- Use `Assert()` and `FatalAssert()` from `Core/Debug.h` for debug checks
- Use `likely()` and `unlikely()` macros for branch prediction hints
- No exceptions (project is exception-free)
- In release builds, `Assert()` becomes a no-op

### Memory Management
- Custom allocator system: `PoolAllocator`, `StackAllocator`, `MonotonicAllocator`, `MultiPoolAllocator`
- Use `MemoryManager::GetInstance()` for global allocation
- Helper templates in `Memory/Memory.h`:
  - `New<T>(args...)` - allocate and construct
  - `Delete<T>(ptr)` - destruct and deallocate

### Testing Patterns
- Tests extend `TestCollection` base class
- Register via `TestEnv::GetEnv().AddTestCollection<MyTest>(args...)`
- Use `Prepare()` virtual method to add test cases
- Output via `TLogOut` (std::stringstream) and `ELogLevel`

Example test class:
```cpp
class MyTest : public TestCollection
{
public:
    explicit MyTest() : TestCollection("My Test") {}

protected:
    void Prepare() override
    {
        AddTest("Test Name", [this](TLogOut& log) {
            // test code
        });
    }
};
```

### General Patterns
- Forward declare classes in headers when possible
- Place member variables first
- Place properties and methods by visibility order. e.g. (public > protected > private)
- Place static variables and methods first in class declarations.
- Use `#pragma once` for header guards
- Use `virtual ~ClassName() = default` for virtual destructors
- Prefer pass-by-reference over raw pointers.
- Ensure reference variables should be always valid.
- Always check pointer validity before using in functions.
- Use `constexpr` for compile-time constants
- Separate definition blocks with empty line (per `SeparateDefinitionBlocks: Always`)
- Remove unnecessary includes
// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.
- Place the copyright notice at the beginning of all files.