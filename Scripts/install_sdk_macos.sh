#!/bin/bash
# instal_sdk_macos.sh - Install required SDKs and tools for HardBop Engine on macOS
# Includes Vulkan SDK installation (embedded) and other build dependencies.
# This script is idempotent and can be re-run safely.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== HardBop Engine macOS SDK Installer ==="

echo "Ensuring Homebrew is installed..."
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "Homebrew already installed."
fi

# Update Homebrew
brew update

# Install common build tools
echo "Installing required build tools (cmake, ninja, clang) via Homebrew..."
brew install cmake ninja llvm || true

# Ensure clang is available (llvm provides clang as llvm-clang)
if ! command -v clang &> /dev/null; then
    echo "clang not found, linking llvm clang as clang..."
    sudo ln -sf $(brew --prefix llvm)/bin/clang $(brew --prefix)/bin/clang || true
fi

# Install Vulkan SDK (embedded)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VULKAN_DIR="$PROJECT_ROOT/External/VulkanSDK"

echo "=== Vulkan SDK Installer for macOS ==="

echo "Installing to: $VULKAN_DIR"

mkdir -p "$VULKAN_DIR/include" "$VULKAN_DIR/lib"

BREW_PREFIX=""
if command -v brew &> /dev/null; then
    BREW_PREFIX=$(brew --prefix)
fi

# Check if already fully installed
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
    LIB_COUNT=$(ls "$VULKAN_DIR/lib"/libvulkan*.dylib 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libvulkan*.a 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libMoltenVK*.dylib 2>/dev/null | wc -l)
    if [ "$LIB_COUNT" -gt 0 ]; then
        echo "Vulkan SDK already installed at $VULKAN_DIR"
        # continue
    fi
fi

# Method 1: Use Homebrew (primary source)
echo "Installing Vulkan SDK from Homebrew..."
if [ -n "$BREW_PREFIX" ]; then
    brew install vulkan-headers vulkan-loader 2>/dev/null || true
    if [ -d "$BREW_PREFIX/include/vulkan" ]; then
        cp -r "$BREW_PREFIX/include/vulkan" "$VULKAN_DIR/include/" 2>/dev/null || true
    fi
    if [ -f "$BREW_PREFIX/lib/libvulkan.dylib" ]; then
        cp "$BREW_PREFIX/lib/libvulkan.dylib" "$VULKAN_DIR/lib/" 2>/dev/null || true
    fi
    if [ -f "$BREW_PREFIX/lib/libvulkan.1.dylib" ]; then
        cp "$BREW_PREFIX/lib/libvulkan.1.dylib" "$VULKAN_DIR/lib/" 2>/dev/null || true
    fi
    if [ -f "$BREW_PREFIX/lib/libvulkan.a" ]; then
        cp "$BREW_PREFIX/lib/libvulkan.a" "$VULKAN_DIR/lib/" 2>/dev/null || true
    fi
fi

# Method 2: Check system installation
if [ ! -f "$VULKAN_DIR/include/vulkan/vulkan.h" ] && [ -f /opt/homebrew/include/vulkan/vulkan.h ]; then
    echo "Using system Homebrew Vulkan installation..."
    cp -r /opt/homebrew/include/vulkan "$VULKAN_DIR/include/" 2>/dev/null || true
fi

# Method 3: Try Homebrew's MoltenVK as fallback
if [ ! -f "$VULKAN_DIR/include/vulkan/vulkan.h" ] || [ ! -f "$VULKAN_DIR/lib/libvulkan.dylib" ]; then
    echo "Trying Homebrew MoltenVK as fallback..."
    BREW_PREFIX=$(brew --prefix molten-vk 2>/dev/null || echo "")
    if [ -n "$BREW_PREFIX" ] && [ -f "$BREW_PREFIX/lib/libMoltenVK.dylib" ]; then
        echo "Found Homebrew MoltenVK at $BREW_PREFIX"
        cp "$BREW_PREFIX/lib/libMoltenVK.dylib" "$VULKAN_DIR/lib/" 2>/dev/null || true
        if [ -f "$BREW_PREFIX/lib/libMoltenVK.a" ]; then
            cp "$BREW_PREFIX/lib/libMoltenVK.a" "$VULKAN_DIR/lib/" 2>/dev/null || true
        fi
        if [ -d "$BREW_PREFIX/libexec/include" ]; then
            cp -r "$BREW_PREFIX/libexec/include/"* "$VULKAN_DIR/include/" 2>/dev/null || true
        fi
    fi
fi

# Verify headers
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
    echo "Vulkan SDK headers installed successfully!"
else
    echo "Error: Vulkan headers not found. Installation failed."
    exit 1
fi

# Verify libraries
LIB_COUNT=$(ls "$VULKAN_DIR/lib"/libvulkan*.dylib 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libvulkan*.a 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libMoltenVK*.dylib 2>/dev/null | wc -l)
if [ "$LIB_COUNT" -gt 0 ]; then
    echo "Vulkan SDK libraries installed successfully! ($LIB_COUNT files)"
else
    echo "Warning: No Vulkan libraries found in $VULKAN_DIR/lib."
fi

echo "All required SDKs and tools are installed. You can now build the project:"
echo "  mkdir -p build && cd build"
echo "  cmake .. -G \"Ninja Multi-Config\""
echo "  cmake --build . --config Debug"

exit 0
