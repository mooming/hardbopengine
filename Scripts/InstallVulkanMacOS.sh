#!/bin/bash
# Install Vulkan SDK on macOS using Homebrew
# Note: Requires sudo for system-wide installation, or use --dry-run to see options

set -e

echo "=== Vulkan SDK Installer for macOS ==="

# Check if Vulkan is already available in standard locations
if ls /usr/local/include/vulkan/vulkan.h &> /dev/null || \
   ls /opt/homebrew/include/vulkan/vulkan.h &> /dev/null; then
    echo "Vulkan SDK is already installed in system include paths!"
    exit 0
fi

echo "Vulkan SDK not found in standard locations."
echo ""

# Check for MoltenVK via Homebrew (provides Vulkan headers)
MOLTENVK_PATH=""
if command -v brew &> /dev/null; then
    if brew list molten-vk &> /dev/null 2>&1; then
        # Find MoltenVK installation
        MOLTENVK_PATH=$(brew --prefix)/Cellar/molten-vk/$(brew list --versions molten-vk | awk '{print $2}')/libexec/include
        echo "Found MoltenVK via Homebrew at: $MOLTENVK_PATH"
    fi
fi

echo ""
echo "Options to enable Vulkan SDK:"
echo ""
echo "Option 1: Create a symlink (requires sudo)"
echo "  sudo ln -s $MOLTENVK_PATH /usr/local/include/vulkan"
echo ""
echo "Option 2: Use Xcode Command Line Tools (may include Vulkan)"
echo "  xcode-select --install"
echo ""
echo "Option 3: Download LunarG Vulkan SDK manually"
echo "  https://www.lunarg.com/vulkan-sdk/"
echo ""
echo "Note: The build system expects Vulkan headers in:"
echo "  /usr/local/include/vulkan/vulkan.h"
echo "  or /opt/homebrew/include/vulkan/vulkan.h"