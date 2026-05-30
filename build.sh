#!/usr/bin/env bash
# build.sh - Build a specified target with optional flags
# Usage: build.sh <target> [-dev] [-clean] [-notest]
# Example: build.sh Applications/TriangleExample -dev -clean -notest

set -e

# Default settings
CLEAN=0
RUN_TESTS=1
TARGET=""

# Configuration flags (can specify multiple)
CONFIGS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    -dev)
      CONFIGS+=("Dev")
      ;;
    -debug)
      CONFIGS+=("Debug")
      ;;
    -release)
      CONFIGS+=("Release")
      ;;
    -clean)
      CLEAN=1
      ;;
    -notest)
      RUN_TESTS=0
      ;;
    *)
      if [[ -z "$TARGET" ]]; then
        TARGET="$1"
      else
        echo "Unexpected argument: $1"
        exit 1
      fi
      ;;
  esac
  shift
done

# If no explicit config flags were given, default to Dev
if [[ ${#CONFIGS[@]} -eq 0 ]]; then
  CONFIGS+=("Dev")
fi

if [[ -z "$TARGET" ]]; then
  echo "Usage: $0 <target> [-dev] [-debug] [-release] [-clean] [-notest]"
  exit 1
fi

# Ensure the build directory is generated (CMake configuration)
if [[ ! -d build ]]; then
  echo "Generating build files..."
  cmake -B build -S . -G "Ninja Multi-Config"
fi

# Iterate over each requested configuration
for CONFIG in "${CONFIGS[@]}"; do
  # Clean if requested
  if [[ $CLEAN -eq 1 ]]; then
    echo "Cleaning build for $TARGET (config $CONFIG)"
    cmake --build build --config "$CONFIG" --target clean || true
  fi

  # Build the specified target
  TARGET_NAME=$(basename "$TARGET")
  echo "Building target $TARGET_NAME with configuration $CONFIG"
  cmake --build build --config "$CONFIG" --target "$TARGET_NAME"

  # Run Engine tests unless suppressed
  if [[ $RUN_TESTS -eq 1 ]]; then
    echo "Running Engine tests (config $CONFIG)"
    TEST_BIN="./build/Applications/EngineTest/$CONFIG/EngineTest"
    if [[ -x $TEST_BIN ]]; then
      "$TEST_BIN"
    else
      echo "Test binary not found: $TEST_BIN"
    fi
  fi
done
