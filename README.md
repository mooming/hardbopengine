# HardBop Engine

A lightweight, multiplatform C++23 game engine designed for casual applications and experimental projects.

## Overview

HardBop Engine provides a comprehensive set of modules for game development while maintaining simplicity and ease of use. It supports Windows, macOS, and Linux with a unified API across platforms.

## Prerequisites

### All Platforms
- **CMake 3.12+**
- **C++23 compatible compiler** (Clang 16+, GCC 13+, MSVC 2022+)

No additional dependencies required beyond CMake and a C++23 compatible compiler.

### Vulkan SDK (Optional - for running tests)
If Vulkan SDK is not installed, tests will still run but Vulkan-related features will be disabled with a compile-time warning. To enable full test support:

```bash
# macOS
./Scripts/InstallVulkanMacOS.sh

# Linux
./Scripts/InstallVulkanLinux.sh

# Windows
Scripts\InstallVulkanWindows.bat
```

## Building

### Quick Start
```bash
# Build all tools and copy executables to bin/
./Tools/BuildAllTools.sh

# Configure with CMake
cmake --fresh -B build -G "Ninja Multi-Config" -S .

# Build with config options(Debug, Dev, Release)
cmake --build build --config Debug
cmake --build build --config Dev
cmake --build build --config Release

# Run tests (paths depend on build configuration)
./build/Applications/EngineTest/Debug/EngineTest
./build/Applications/EngineTest/Dev/EngineTest
./build/Applications/EngineTest/Release/EngineTest.app/Contents/MacOS/EngineTest
```

### Convenient Build Script
A helper script `build.sh` is provided for building specific targets with optional flags.
```bash
# Basic usage
./build.sh <target> [-dev] [-debug] [-release] [-clean] [-notest]

# Example: build TriangleExample for Dev, Debug and Release configurations, clean first, skip tests
./build.sh Applications/TriangleExample -dev -debug -release -clean -notest
```
The script defaults to the **Dev** configuration when no explicit config flag is given.

### Build Types
- **Debug**: `-g -O0` - Full debug symbols, no optimization
- **Dev**: `-O1` - Light optimization for development (default)
- **Release**: `-O3` - Full optimization for production

### Running Tests
```bash
# Build and run tests (Note: 'make' depends on your generator, e.g., Ninja)
cd build
cmake --build . --target EngineTest
./Applications/EngineTest/EngineTest
```

## Configuration

The engine can be configured via `Engine/Config/BuildConfig.h`. Most settings work out of the box with defaults.

### Key Configuration Options

| Category | Setting | Default | Description |
|----------|---------|---------|-------------|
| Engine | `MAX_NUM_TASK_STREAMS` | 64 | Maximum task streams |
| Engine | `ENGINE_LOG_ENABLED` | 1 | Enable logging system |
| Engine | `ENGINE_PARAM_DESC_ENABLED` | 1 | Enable parameter descriptions |
| Engine | `ENGINE_MIN_HARDWARE_THREADS` | 4 | Minimum recommended CPU cores |
| Memory | `__MEMORY_VERIFICATION__` | 0 | Verify memory integrity |
| Memory | `__FORCE_USE_SYSTEM_MALLOC__` | 0 | Use system malloc instead |
| Memory | `__MEMORY_LOGGING__` | 0 | Log memory operations |
| Memory | `__MEMORY_DANGLING_POINTER_CHECK__` | 0 | Detect dangling pointers |
| Memory | `MULTIPOOL_ALLOC_LOG` | ".multiPoolConfig.dat" | MultiPool config cache file |
| Log | `LOG_ENABLED` | 1 | Master log switch |
| Log | `LOG_BREAK_IF_WARNING` | 0 | Break on warnings |
| Log | `LOG_BREAK_IF_ERROR` | 0 | Break on errors |
| Log | `LOG_FORCE_PRINT_IMMEDIATELY` | 0 | Print immediately (no async) |
| Profile | `PROFILE_ENABLED` | 0 | Enable performance profiling |
| Math | `__RIGHT_HANDED__` | - | Coordinate system (default right-handed) |

### Platform Macros
Automatically defined based on compiler:
- `PLATFORM_LINUX` - Linux
- `PLATFORM_OSX` - macOS  
- `PLATFORM_WINDOWS` - Windows

### Vulkan SDK
Auto-detected at compile time using `__has_include`. If not found:
- `VULKAN_SDK` is set to 0
- Compile-time warning is shown
- Code still compiles without Vulkan support

For full Vulkan support, run the installation scripts in `Scripts/`.

## Directory Structure

```
HardBopEngine/
├── Applications/          # Application entry points
│   └── EngineTest/       # Unit test executable
├── Engine/               # Core engine modules
│   ├── Config/          # Configuration system
│   ├── Container/       # Core containers (Array, LinkedList)
│   ├── Core/            # Task system, Debug, Time, Components
│   ├── Engine/          # Main engine singleton
│   ├── HSTL/            # HardBop STL (HVector, HString, HMap)
│   ├── Log/             # Async logging system
│   ├── Math/            # Vectors, Matrices, Quaternions
│   ├── Memory/          # Custom allocators
│   ├── OSAL/            # OS Abstraction Layer
│   ├── Resource/        # Buffer, Stream, Resource Manager
│   ├── String/          # String utilities
│   └── Test/            # Unit testing framework
├── External/            # Third-party libraries (placeholder for future)
├── Scripts/             # Build and utility scripts
├── docs/                 # Documentation
├── lib/                 # Built static libraries (generated)
└── bin/                 # Built executables (generated)
```

## Module Descriptions

### Config
Thread-safe configuration parameters with atomic support. Key-value storage for runtime tweaking.

- `ConfigParam<T>` - Thread-safe config parameter
- `ConfigFile` - File-based configuration (string values only)

### Container
Core data structures with custom allocator support.

- `Array<T>` - Static array with allocator template
- `LinkedList<T>` - Linked list implementation
- `AtomicStackView<T>` - Lock-free stack

### Core
Essential engine systems.

- `TaskSystem` - Multi-threaded task scheduling with stream separation
- `Debug` - Assert macros with branch prediction hints
- `ComponentSystem` - Entity-component system
- `Time` - Time utilities

### HSTL (HardBop STL)
Thin wrappers around standard containers with custom allocators.

- `HVector<T>` - std::vector with DefaultAllocator
- `HString<T>` - std::string with custom allocator
- `HUnorderedMap<K,V>` - std::unordered_map with custom allocator

### Log
Asynchronous logging system.

- Double-buffered, lock-free writes
- Category-based filtering
- Multiple output destinations

### Math
Comprehensive math library.

- `Vector2`, `Vector3`, `Vector4` - Dimension-specific vectors
- `Matrix2x2`, `Matrix3x3`, `Matrix4x4` - Matrix classes
- `Quaternion` - Rotation representation
- `Transform` - Affine transformations
- `AABB`, `OBB` - Bounding boxes

### Memory
Custom memory management system.

- `PoolAllocator` - Fixed-size pool
- `StackAllocator` - Stack-based allocation
- `MonotonicAllocator` - Arena allocator
- `MultiPoolAllocator` - Multiple pool sizes
- `ThreadSafeMultiPoolAllocator` - Thread-safe variant

### OSAL (OS Abstraction Layer)
Platform-specific implementations.

- Thread management
- File I/O
- Memory operations
- Debug utilities

### Resource
Resource management.

- `Buffer` - Binary data container
- `BufferInputStream` / `BufferOutputStream` - Binary streams
- `ResourceManager` - Resource lifecycle management

### String
String utilities with interning.

- `StaticString` - Interned string for cheap comparison
- `String` - Main string class with full functionality
- `StringBuilder` - Dynamic string building
- `InlineStringBuilder` - Stack-allocated builder

## Usage Examples

### Basic Engine Initialization
```cpp
#include "Engine/Engine.h"

int main(int argc, const char* argv[])
{
    hbe::Engine engine;
    engine.Initialize(argc, argv);
    
    // Your game logic here
    
    engine.WaitForEnd();
    return 0;
}
```

### Custom Allocator
```cpp
#include "Memory/PoolAllocator.h"

void Example()
{
    PoolAllocator allocator("MyPool", 64, 1024); // name, block size (bytes), number of blocks
    
    void* ptr = allocator.Allocate(64);
    allocator.Deallocate(ptr, 64);
}
```

### String Operations
```cpp
#include "String/String.h"

void Example()
{
    hbe::String str("Hello World");
    
    // Replace
    auto result = str.Replace("World", "Engine"); // "Hello Engine"
    
    // ReplaceAll
    auto all = str.ReplaceAll("o", "0"); // "Hell0 W0rld"
}
```

## Documentation

### Allocator Guide
Detailed guide for using custom memory allocators:
- `docs/AllocatorGuide.md`

### Code Standard
Refer to [CodeStandard.md](docs/CodeStandard.md) for coding guidelines and conventions.

### AI Prompt Automation Scripts
Automate code review with these scripts that iterate over source files and execute prompts one-by-one:

| Script | Location | Description |
|--------|----------|-------------|
| `batch_ai_prompt.sh` | Project root (Linux/macOS) | Bash script for C/C++/Obj-C file analysis on Unix systems |
| `batch_ai_prompt.bat` | Project root (Windows) | Batch script for code review automation on Windows |

**Supported file types:** `.c`, `.h`, `.cpp`, `.hpp`, `.cc`, `.cxx`, `.m`, `.mm`

**Quick Start:**
```bash
# macOS/Linux
chmod +x batch_ai_prompt.sh
./batch_ai_prompt.sh ./Engine "/review"

# Windows
batch_ai_prompt.bat .\Engine "/review"
```

These scripts are perfect for batch code review, documentation generation, or automated analysis of the entire codebase.


### Platform-Specific
- **Linux backtrace**: `OS::GetBackTrace()` is implemented on Linux using `backtrace()` function from `<execinfo.h>`, with symbol demangling via `cxxabi`.

### Code Quality
- **ODR Violation Risk**: Platform-specific files and generic OSMemory.cpp both define memory functions - ensure only one is compiled per platform (CMakeLists.txt should use platform-conditional source selection).

### Unfinished Features
- **OSThreadTest**: Empty test placeholder.

## Contributing

This is a personal project used for learning and experimentation. Issues and pull requests are welcome but may have limited response time.

## License

See repository for license information.


## Build
cmake --fresh -B build -G "Ninja Multi-Config" -S . && cmake --build build --config Debug
