#!/bin/bash
# Install Vulkan SDK for macOS under project directory
# Downloads LunarG Vulkan SDK and extracts to External/VulkanSDK

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VULKAN_DIR="$PROJECT_ROOT/External/VulkanSDK"

echo "=== Vulkan SDK Installer for macOS ==="
echo "Installing to: $VULKAN_DIR"

# Create directories
mkdir -p "$VULKAN_DIR/include" "$VULKAN_DIR/lib"

# Check if already fully installed
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ] && [ -d "$VULKAN_DIR/lib" ]; then
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

# Vulkan SDK version
VULKAN_VERSION="1.4.341.0"
MACOS_VERSION="mac"

# Method 1: Try to download Vulkan SDK from LunarG (tar.gz)
echo "Downloading Vulkan SDK ${VULKAN_VERSION} for macOS..."
echo "This may take a few minutes..."

FILENAME="vulkan-sdk-${VULKAN_VERSION}.${MACOS_VERSION}"
DOWNLOAD_URL="https://sdk.lunarg.com/sdk/download/${VULKAN_VERSION}/${FILENAME}.tar.gz?u="

# Clean up any existing partial downloads
rm -f "/tmp/${FILENAME}.tar.gz"

if command -v curl &> /dev/null; then
    curl -L -o "/tmp/${FILENAME}.tar.gz" "$DOWNLOAD_URL" --fail -# || true
elif command -v wget &> /dev/null; then
    wget -O "/tmp/${FILENAME}.tar.gz" "$DOWNLOAD_URL" || true
fi

if [ -f "/tmp/${FILENAME}.tar.gz" ] && [ -s "/tmp/${FILENAME}.tar.gz" ]; then
    echo "Extracting SDK..."
    rm -rf "$VULKAN_DIR/include"/* "$VULKAN_DIR/lib"/*
    tar -xzf "/tmp/${FILENAME}.tar.gz" -C "$VULKAN_DIR" --strip-components=1
    rm -f "/tmp/${FILENAME}.tar.gz"
    echo "Vulkan SDK extracted successfully!"
else
    echo "Download from LunarG failed or not available."
    echo ""
    
    # Method 2: Try Homebrew's MoltenVK as fallback (macOS Vulkan implementation)
    echo "Trying Homebrew MoltenVK as fallback..."
    if command -v brew &> /dev/null; then
        BREW_PREFIX=$(brew --prefix molten-vk 2>/dev/null || echo "")
        if [ -n "$BREW_PREFIX" ] && [ -f "$BREW_PREFIX/lib/libMoltenVK.dylib" ]; then
            echo "Found Homebrew MoltenVK at $BREW_PREFIX"
            cp "$BREW_PREFIX/lib/libMoltenVK.dylib" "$VULKAN_DIR/lib/"
            if [ -f "$BREW_PREFIX/lib/libMoltenVK.a" ]; then
                cp "$BREW_PREFIX/lib/libMoltenVK.a" "$VULKAN_DIR/lib/"
            fi
            # Copy headers if available
            if [ -d "$BREW_PREFIX/libexec/include" ]; then
                cp -r "$BREW_PREFIX/libexec/include/"* "$VULKAN_DIR/include/" 2>/dev/null || true
            fi
            echo "Installed MoltenVK from Homebrew"
        fi
    fi
    
    # Method 3: Copy from system installation as last resort
    if [ ! -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
        echo "Trying to copy from system installation..."
        if [ -f /usr/local/include/vulkan/vulkan.h ]; then
            cp -r /usr/local/include/vulkan/* "$VULKAN_DIR/include/vulkan/" 2>/dev/null || true
            cp -r /usr/local/include/vk_video "$VULKAN_DIR/include/" 2>/dev/null || true
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
