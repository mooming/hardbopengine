#!/bin/bash
# Install Vulkan SDK for Linux under project directory
# Downloads LunarG Vulkan SDK and extracts to External/VulkanSDK

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VULKAN_DIR="$PROJECT_ROOT/External/VulkanSDK"

echo "=== Vulkan SDK Installer for Linux ==="
echo "Installing to: $VULKAN_DIR"

# Create directories
mkdir -p "$VULKAN_DIR/include" "$VULKAN_DIR/lib"

# Check if already fully installed
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ] && [ -d "$VULKAN_DIR/lib" ]; then
    LIB_COUNT=$(ls "$VULKAN_DIR/lib"/libvulkan*.so* 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libvulkan*.a 2>/dev/null | wc -l)
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
LINUX_VERSION="linux"

# Try to install via package manager first (most reliable on Linux)
echo "Checking for system Vulkan packages..."

if command -v apt-get &> /dev/null; then
    echo "Detected Debian/Ubuntu-based system"
    PKG_MGR="apt"
elif command -v dnf &> /dev/null; then
    echo "Detected Fedora/RHEL-based system"
    PKG_MGR="dnf"
elif command -v pacman &> /dev/null; then
    echo "Detected Arch Linux"
    PKG_MGR="pacman"
fi

if [ -n "$PKG_MGR" ]; then
    echo "Attempting to install via $PKG_MGR..."
    
    if [ "$PKG_MGR" = "apt" ]; then
        sudo apt-get update -qq && sudo apt-get install -y -qq libvulkan-dev vulkan-tools glslang-tools 2>/dev/null && SYSTEM_VULKAN=true
    elif [ "$PKG_MGR" = "dnf" ]; then
        sudo dnf install -y -q vulkan-loader-devel vulkan-tools 2>/dev/null && SYSTEM_VULKAN=true
    elif [ "$PKG_MGR" = "pacman" ]; then
        sudo pacman -S --noconfirm vulkan-headers vulkan-tools 2>/dev/null && SYSTEM_VULKAN=true
    fi
    
    if [ "$SYSTEM_VULKAN" = "true" ]; then
        echo "Installing system Vulkan packages..."
        
        # Copy system headers
        mkdir -p "$VULKAN_DIR/include/vulkan"
        if [ -f /usr/include/vulkan/vulkan.h ]; then
            cp -r /usr/include/vulkan/* "$VULKAN_DIR/include/vulkan/"
        elif [ -f /usr/local/include/vulkan/vulkan.h ]; then
            cp -r /usr/local/include/vulkan/* "$VULKAN_DIR/include/vulkan/"
        fi
        
        # Copy vk_video headers
        if [ -d /usr/include/vk_video ]; then
            cp -r /usr/include/vk_video "$VULKAN_DIR/include/"
        elif [ -d /usr/local/include/vk_video ]; then
            cp -r /usr/local/include/vk_video "$VULKAN_DIR/include/"
        fi
        
        # Copy system libraries
        mkdir -p "$VULKAN_DIR/lib"
        
        # Try common library locations
        for LIB_PATH in /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib /lib /lib/x86_64-linux-gnu; do
            if [ -f "$LIB_PATH/libvulkan.so.1" ]; then
                cp "$LIB_PATH/libvulkan.so"* "$VULKAN_DIR/lib/" 2>/dev/null || true
            fi
            if [ -f "$LIB_PATH/libvulkan.so" ]; then
                cp "$LIB_PATH/libvulkan.so"* "$VULKAN_DIR/lib/" 2>/dev/null || true
            fi
            if [ -f "$LIB_PATH/libvulkan.a" ]; then
                cp "$LIB_PATH/libvulkan.a" "$VULKAN_DIR/lib/" 2>/dev/null || true
            fi
        done
        
        if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
            echo "Vulkan SDK installed from system packages!"
            echo ""
            echo "To build with Vulkan SDK:"
            echo "  cd build"
            echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
            echo "  make -j\$(nproc) EngineTest"
            exit 0
        fi
    fi
fi

# Download Vulkan SDK from LunarG as fallback
echo "Downloading Vulkan SDK from LunarG..."
FILENAME="vulkan-sdk-${VULKAN_VERSION}.${LINUX_VERSION}"
DOWNLOAD_URL="https://sdk.lunarg.com/sdk/download/${VULKAN_VERSION}/${FILENAME}.tar.gz?u="

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
    echo "Vulkan SDK installed successfully!"
else
    echo "Download failed. Please install Vulkan SDK manually."
    exit 1
fi

# Verify
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
    echo "Vulkan SDK headers installed successfully!"
else
    echo "Error: Vulkan headers not found. Installation failed."
    exit 1
fi

LIB_COUNT=$(ls "$VULKAN_DIR/lib"/libvulkan*.so* 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libvulkan*.a 2>/dev/null | wc -l)
if [ "$LIB_COUNT" -gt 0 ]; then
    echo "Vulkan SDK libraries installed successfully!"
    ls -la "$VULKAN_DIR/lib"/
else
    echo "Warning: No Vulkan libraries found."
    ls -la "$VULKAN_DIR/lib"/
fi

echo ""
echo "To build with Vulkan SDK:"
echo "  cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc) EngineTest"
