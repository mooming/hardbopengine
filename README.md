# HardBop Engine

A lightweight, multiplatform C++23 game engine designed for casual applications and experimental projects.

## Overview

HardBop Engine provides a comprehensive set of modules for game development while maintaining simplicity and ease of use. It supports Windows, macOS, and Linux with a unified API across platforms.

## Prerequisites

### All Platforms
- **CMake 3.12+**
- **C++23 compatible compiler** (Clang 16+, GCC 13+, MSVC 2022+)

### Platform-Specific

#### macOS
```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install required packages
brew install glfw
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y libglfw3-dev libglfw3
```

#### Windows
```bash
# Install Chocolatey (if not installed)
choco install cmake visualstudio2022buildtools

# Install GLFW via Chocolatey
choco install glfw
```

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
# Create build directory
mkdir -p build && cd build

# Configure with CMake (Debug/Dev/Release)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (use -j for parallel compilation)
make -j$(nproc)

# Run tests
./build/Applications/EngineTest/EngineTest
```

### Build Types
- **Debug**: `-g -O0` - Full debug symbols, no optimization
- **Dev**: `-O1` - Light optimization for development
- **Release**: `-O3` - Full optimization for production

### Running Tests
```bash
# Build and run tests
cd build
make EngineTest
./Applications/EngineTest/EngineTest
```

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
├── External/            # Third-party libraries (GLFW)
├── Scripts/             # Build and utility scripts
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
    PoolAllocator allocator(1024 * 1024, 64); // 1MB pool, 64-byte blocks
    
    void* ptr = allocator.Allocate(64);
    allocator.Deallocate(ptr);
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

## Known Issues

### Platform-Specific
- **Linux backtrace**: `OS::GetBackTrace()` returns "Not Implemented" on Linux - no stack traces available.

### Code Quality
- **ODR Violation Risk**: Platform-specific files and generic OSMemory.cpp both define memory functions - ensure only one is compiled per platform (CMakeLists.txt should use platform-conditional source selection).

### Unfinished Features
- **OSThreadTest**: Empty test placeholder.

## Contributing

This is a personal project used for learning and experimentation. Issues and pull requests are welcome but may have limited response time.

## License

See repository for license information.