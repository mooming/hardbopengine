#!/usr/bin/env bash
# install_sdk_linux.sh - Install required SDKs and tools for HardBop Engine on Linux
# Mirrors install_sdk_macos.sh functionality using system package manager.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== HardBop Engine Linux SDK Installer ==="

# Detect package manager
if command -v apt-get &> /dev/null; then
    PKG_MGR="apt"
    UPDATE_CMD="sudo apt-get update -qq"
    INSTALL_CMD="sudo apt-get install -y -qq"
elif command -v dnf &> /dev/null; then
    PKG_MGR="dnf"
    UPDATE_CMD="sudo dnf -q makecache"
    INSTALL_CMD="sudo dnf install -y -q"
elif command -v pacman &> /dev/null; then
    PKG_MGR="pacman"
    UPDATE_CMD="sudo pacman -Sy --noconfirm"
    INSTALL_CMD="sudo pacman -S --noconfirm --needed"
else
    echo "Unsupported Linux distribution. Please install cmake, ninja, gcc/g++, and Vulkan SDK manually."
    exit 1
fi

echo "Detected package manager: $PKG_MGR"

# Install required build tools
echo "Installing required build tools (cmake, ninja, gcc, g++)..."
if [ "$PKG_MGR" = "apt" ]; then
    $UPDATE_CMD
    $INSTALL_CMD cmake ninja-build gcc g++
elif [ "$PKG_MGR" = "dnf" ]; then
    $UPDATE_CMD
    $INSTALL_CMD cmake ninja-build gcc g++
elif [ "$PKG_MGR" = "pacman" ]; then
    $UPDATE_CMD
    $INSTALL_CMD cmake ninja gcc g++
fi

# gcc and g++ were installed via the package manager

# Install Vulkan SDK via system packages
VULKAN_DIR="$PROJECT_ROOT/External/VulkanSDK"
mkdir -p "$VULKAN_DIR/include" "$VULKAN_DIR/lib"

echo "Installing Vulkan SDK via system packages..."
if [ "$PKG_MGR" = "apt" ]; then
    $INSTALL_CMD libvulkan-dev vulkan-tools glslang-tools
elif [ "$PKG_MGR" = "dnf" ]; then
    $INSTALL_CMD vulkan-loader-devel vulkan-tools glslang
elif [ "$PKG_MGR" = "pacman" ]; then
    $INSTALL_CMD vulkan-headers vulkan-tools glslang
fi

# Copy headers
if [ -d "/usr/include/vulkan" ]; then
    cp -r "/usr/include/vulkan" "$VULKAN_DIR/include/" || true
elif [ -d "/usr/local/include/vulkan" ]; then
    cp -r "/usr/local/include/vulkan" "$VULKAN_DIR/include/" || true
fi

# Copy optional vk_video headers if present
if [ -d "/usr/include/vk_video" ]; then
    cp -r "/usr/include/vk_video" "$VULKAN_DIR/include/" || true
elif [ -d "/usr/local/include/vk_video" ]; then
    cp -r "/usr/local/include/vk_video" "$VULKAN_DIR/include/" || true
fi

# Copy libraries from common locations
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

# Verify installation
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
    echo "Vulkan SDK headers installed successfully!"
else
    echo "Vulkan headers not found – attempting fallback download..."
    VULKAN_VERSION="1.4.341.0"
    LINUX_VERSION="linux"
    FILENAME="vulkan-sdk-${VULKAN_VERSION}.${LINUX_VERSION}"
    DOWNLOAD_URL="https://sdk.lunarg.com/sdk/download/${VULKAN_VERSION}/${FILENAME}.tar.gz?u="
    TMP_TAR="/tmp/${FILENAME}.tar.gz"
    rm -f "$TMP_TAR"
    if command -v curl &> /dev/null; then
        curl -L -o "$TMP_TAR" "$DOWNLOAD_URL" --fail -# || true
    elif command -v wget &> /dev/null; then
        wget -O "$TMP_TAR" "$DOWNLOAD_URL" || true
    fi
    if [ -f "$TMP_TAR" ] && [ -s "$TMP_TAR" ]; then
        echo "Extracting SDK..."
        rm -rf "$VULKAN_DIR/include"/* "$VULKAN_DIR/lib"/*
        tar -xzf "$TMP_TAR" -C "$VULKAN_DIR" --strip-components=1
        rm -f "$TMP_TAR"
        echo "Vulkan SDK installed successfully via fallback!"
    else
        echo "Fallback download failed. Please install Vulkan SDK manually."
        exit 1
    fi
fi

LIB_COUNT=$(ls "$VULKAN_DIR/lib"/libvulkan*.so* 2>/dev/null | wc -l || ls "$VULKAN_DIR/lib"/libvulkan*.a 2>/dev/null | wc -l)
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
