#!/bin/bash

# Script to compile all top-level tools under Tools/ and copy executables to bin/

set -e

# Create bin directory if it doesn't exist
mkdir -p bin

# Define the root tools directory
TOOLS_ROOT="Tools"

# We only want to build top-level tools, not submodules.
# A simple heuristic: build directories that are directly under TOOLS_ROOT.
for tool_dir in "$TOOLS_ROOT"/*; do
    if [ -d "$tool_dir" ] && [ -f "$tool_dir/CMakeLists.txt" ]; then
        echo "Building top-level tool in: $tool_dir"
        
        # Build directory for the tool
        BUILD_DIR="$tool_dir/build"
        
        # Configure and build using Ninja
        cmake -B "$BUILD_DIR" -S "$tool_dir" -G "Ninja Multi-Config"
        cmake --build "$BUILD_DIR" --config Release

        # Find executables in the build directory to copy them to bin/
        # We look for files in 'Release' subfolders that are executables (no extension)
        find "$BUILD_DIR" -path "*/Release/*" -type f ! -name "*.*" | while read -r exe; do
            echo "Copying $exe as $(basename "$exe") to bin/"
            cp "$exe" bin/$(basename "$exe")
        done
    fi
done

echo "All top-level tools built and copied to bin/"

