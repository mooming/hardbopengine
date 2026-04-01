#!/bin/bash
# Install Vulkan SDK on macOS using Homebrew

set -e

echo "=== Vulkan SDK Installer for macOS ==="

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Check if Vulkan is already available
if ls /usr/local/include/vulkan/vulkan.h &> /dev/null || \
   ls /opt/homebrew/include/vulkan/vulkan.h &> /dev/null; then
    echo "Vulkan SDK is already installed!"
    exit 0
fi

echo "Vulkan SDK not found. Installing..."

# Try to install via Homebrew (LunarG SDK via MoltenVK)
# Note: Full LunarG SDK may not be available via brew, so we install MoltenVK for validation
echo "Attempting to install via Homebrew..."

if brew install moltenvk; then
    echo "MoltenVK installed successfully"
    echo "Note: For full Vulkan validation, download LunarG SDK from:"
    echo "      https://www.lunarg.com/vulkan-sdk/"
else
    echo "Failed to install via Homebrew"
    echo "Please download Vulkan SDK manually from:"
    echo "  https://www.lunarg.com/vulkan-sdk/"
    exit 1
fi

# Verify installation
if ls /usr/local/include/vulkan/vulkan.h &> /dev/null || \
   ls /opt/homebrew/include/vulkan/vulkan.h &> /dev/null; then
    echo "Vulkan SDK installed successfully!"
else
    echo "Installation completed but vulkan.h not found in standard locations"
    echo "Please manually add Vulkan SDK path to include directories"
fi