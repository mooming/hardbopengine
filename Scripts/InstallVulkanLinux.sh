#!/bin/bash
# Install Vulkan SDK for Linux under project directory
# Downloads LunarG Vulkan SDK and extracts to External/VulkanSDK

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VULKAN_DIR="$PROJECT_ROOT/External/VulkanSDK"

echo "=== Vulkan SDK Installer for Linux ==="
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

# Detect package manager
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

# Try to install via package manager first
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
        # Copy system headers to project directory for build
        echo "Copying system Vulkan headers to project directory..."
        mkdir -p "$VULKAN_DIR/include/vulkan"
        
        # Try common system header locations
        if [ -f /usr/include/vulkan/vulkan.h ]; then
            cp -r /usr/include/vulkan/* "$VULKAN_DIR/include/vulkan/"
            echo "Vulkan SDK installed from system packages!"
            echo ""
            echo "To build with Vulkan SDK:"
            echo "  cd build"
            echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
            echo "  make -j\$(nproc) EngineTest"
            exit 0
        elif [ -f /usr/local/include/vulkan/vulkan.h ]; then
            cp -r /usr/local/include/vulkan/* "$VULKAN_DIR/include/vulkan/"
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

# Download Vulkan SDK for Linux
VULKAN_VERSION="1.3.280"
LINUX_VERSION="linux"
FILENAME="vulkan-sdk-${VULKAN_VERSION}.${LINUX_VERSION}"

echo "Downloading Vulkan SDK ${VULKAN_VERSION} for Linux..."
echo "This may take a few minutes..."

# Create external directory
mkdir -p "$VULKAN_DIR"

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
    echo "Please install Vulkan SDK via your system's package manager:"
    echo "  Ubuntu/Debian: sudo apt-get install libvulkan-dev"
    echo "  Fedora: sudo dnf install vulkan-loader-devel"
    echo "  Arch: sudo pacman -S vulkan-headers"
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