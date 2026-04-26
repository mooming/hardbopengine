#!/bin/bash
# Install Vulkan SDK for macOS under project directory
# Uses Homebrew as primary source, LunarG as fallback

set -e

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
        echo "To rebuild with Vulkan SDK:"
        echo "  cd build"
        echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
        echo "  make -j\$(nproc) EngineTest"
        exit 0
    fi
fi

# Method 1: Use Homebrew (primary source)
echo "Installing Vulkan SDK from Homebrew..."
if [ -n "$BREW_PREFIX" ]; then
    # Install packages if not already installed
    brew install vulkan-headers vulkan-loader 2>/dev/null || true
    
    # Copy headers
    if [ -d "$BREW_PREFIX/include/vulkan" ]; then
        cp -r "$BREW_PREFIX/include/vulkan" "$VULKAN_DIR/include/" 2>/dev/null || true
    fi
    
    # Copy loader library
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

# Method 3: Try Homebrew's MoltenVK as fallback (macOS Vulkan implementation)
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
    echo "Vulkan SDK libraries installed successfully!"
    ls -la "$VULKAN_DIR/lib"/
else
    echo "Warning: No Vulkan libraries found in lib directory."
    ls -la "$VULKAN_DIR/lib"/
fi

echo ""
echo "To build with Vulkan SDK:"
echo "  cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc) EngineTest"
