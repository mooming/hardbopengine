# HardBop Engine

<p>
 It's not a trivial work to produce a multiplatform software that works with Unix/Linux, Windows, and Mac OS.
Many game engines multiple platforms more than that though, they are very complex and huge to use for casual applications.
</p>

<p>
 This project is created for my personal software development and testing bed for study and experiments. Therefore, it
should be simple and easy to learn and use if users are familiar with standard C/C++ library.
</p>

# Prerequisites

## Required Dependencies

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

### Vulkan SDK (for running tests)
Run the appropriate script for your platform:
```bash
# macOS
./Scripts/InstallVulkanMacOS.sh

# Linux
./Scripts/InstallVulkanLinux.sh

# Windows
Scripts\InstallVulkanWindows.bat
```

# Building

```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests
./bin/EngineTest
```

# Directory Structure

Hardbop Engine<br>
|<br>
|- Applications: Base directory for applications based on the hardbop engine.<br>
|- Engine: Base engine directory<br>
|- External: Base directory for external solutions<br>
|- Scripts: Build and utility scripts<br>
|- lib: Built static libraries<br>
|- bin: Built executables<br>


# Modules

<p>
Basically, each module is an individual library.
</p>

## Config

This module provides a simple key-value database for all tweakable config values.
ConfigParam could be thread-safe if IsAtomic=true.
ConfigFile represents a config file. It can have only string values.


