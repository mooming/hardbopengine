#!/usr/bin/env bash
# generate_cmake_files.sh - Run MakeBuild to generate CMake files for the whole project

# Resolve the project root (directory of this script)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Path to the MakeBuild executable (copied to Tools/bin by Tools/build_all_tools.sh)
MAKEBUILD_BIN="$PROJECT_ROOT/Tools/bin/makebuild"

if [[ ! -x "$MAKEBUILD_BIN" ]]; then
    echo "MakeBuild not built - run Tools/build_all_tools.sh first."
    exit 1
fi

# Run MakeBuild on the project root, forwarding any extra arguments
exec "$MAKEBUILD_BIN" "$PROJECT_ROOT" "$@"
