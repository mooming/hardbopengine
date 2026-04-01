#!/bin/bash
# Install Vulkan SDK for macOS under project directory
# Downloads LunarG Vulkan SDK and extracts to External/VulkanSDK

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VULKAN_DIR="$PROJECT_ROOT/External/VulkanSDK"

echo "=== Vulkan SDK Installer for macOS ==="
echo "Installing to: $VULKAN_DIR"

# Check if already installed
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
    echo "Vulkan SDK already installed at $VULKAN_DIR"
    echo "To rebuild with Vulkan SDK:"
    echo "  cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  make -j\$(nproc) EngineTest"
    exit 0
fi

# Create external directory
mkdir -p "$VULKAN_DIR"

# Download Vulkan SDK for macOS
# Note: LunarG provides macOS .tar.gz packages
VULKAN_VERSION="1.3.280"
MACOS_VERSION="mac"
FILENAME="vulkan-sdk-${VULKAN_VERSION}.${MACOS_VERSION}"

echo "Downloading Vulkan SDK ${VULKAN_VERSION} for macOS..."
echo "This may take a few minutes..."

# Try to download from LunarG
DOWNLOAD_URL="https://sdk.lunarg.com/sdk/download/${VULKAN_VERSION}/${FILENAME}.tar.gz?u="

if command -v curl &> /dev/null; then
    curl -L -o "/tmp/${FILENAME}.tar.gz" "$DOWNLOAD_URL" --fail || true
elif command -v wget &> /dev/null; then
    wget -O "/tmp/${FILENAME}.tar.gz" "$DOWNLOAD_URL" || true
fi

# Check if download succeeded
if [ ! -f "/tmp/${FILENAME}.tar.gz" ] || [ ! -s "/tmp/${FILENAME}.tar.gz" ]; then
    echo "Download failed or not available."
    echo ""
    echo "Alternative: Use Homebrew's MoltenVK (provides headers only)"
    echo "  brew install molten-vk"
    echo ""
    echo "Then manually copy headers:"
    echo "  mkdir -p $VULKAN_DIR/include/vulkan"
    echo "  cp /opt/homebrew/Cellar/molten-vk/*/libexec/include/vulkan/* $VULKAN_DIR/include/vulkan/"
    exit 1
fi

# Extract
echo "Extracting..."
tar -xzf "/tmp/${FILENAME}.tar.gz" -C "$VULKAN_DIR" --strip-components=1

# Cleanup
rm -f "/tmp/${FILENAME}.tar.gz"

# Verify
if [ -f "$VULKAN_DIR/include/vulkan/vulkan.h" ]; then
    echo "Vulkan SDK installed successfully!"
    echo ""
    echo "To build with Vulkan SDK:"
    echo "  cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  make -j\$(nproc) EngineTest"
else
    echo "Installation verification failed."
    exit 1
fi