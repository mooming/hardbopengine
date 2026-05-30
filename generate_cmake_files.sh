#!/usr/bin/env bash
# generate_cmake_files.sh - Run MakeBuild to generate CMake files for the whole project

# Resolve the project root (directory of this script)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Path to the MakeBuild executable (built in Debug configuration)
MAKEBUILD_BIN="$PROJECT_ROOT/Tools/MakeBuild/build/Application/MakeBuild/Debug/makebuild"

if [[ ! -x "$MAKEBUILD_BIN" ]]; then
    echo "MakeBuild not built – run install_sdk_* scripts first."
    exit 1
fi

# Forward all arguments to MakeBuild
exec "$MAKEBUILD_BIN" .
