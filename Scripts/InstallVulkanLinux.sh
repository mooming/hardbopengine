#!/bin/bash
# Install Vulkan SDK on Linux (Ubuntu/Debian)

set -e

echo "=== Vulkan SDK Installer for Linux ==="

# Check if Vulkan is already available
if ls /usr/include/vulkan/vulkan.h &> /dev/null || \
   ls /usr/local/include/vulkan/vulkan.h &> /dev/null; then
    echo "Vulkan SDK is already installed!"
    exit 0
fi

echo "Vulkan SDK not found. Installing..."

# Detect package manager
if command -v apt-get &> /dev/null; then
    echo "Detected Debian/Ubuntu-based system"
    
    # Update package list
    sudo apt-get update
    
    # Install Vulkan SDK packages
    echo "Installing Vulkan development packages..."
    sudo apt-get install -y \
        libvulkan-dev \
        vulkan-tools \
        glslang-tools
    
elif command -v dnf &> /dev/null; then
    echo "Detected Fedora/RHEL-based system"
    
    echo "Installing Vulkan development packages..."
    sudo dnf install -y \
        vulkan-loader-devel \
        vulkan-tools
    
elif command -v pacman &> /dev/null; then
    echo "Detected Arch Linux"
    
    echo "Installing Vulkan development packages..."
    sudo pacman -S --noconfirm \
        vulkan-headers \
        vulkan-tools
    
else
    echo "Unknown package manager. Please install Vulkan SDK manually."
    echo "Download from: https://www.lunarg.com/vulkan-sdk/"
    exit 1
fi

# Verify installation
if ls /usr/include/vulkan/vulkan.h &> /dev/null || \
   ls /usr/local/include/vulkan/vulkan.h &> /dev/null; then
    echo "Vulkan SDK installed successfully!"
    
    # Show installed version
    echo "Installed vulkan version:"
    vulkan-version 2>/dev/null || echo "  (vulkan-tools installed)"
else
    echo "Installation completed but vulkan.h not found"
    echo "Please manually install Vulkan SDK from:"
    echo "  https://www.lunarg.com/vulkan-sdk/"
    exit 1
fi