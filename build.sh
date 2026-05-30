#!/usr/bin/env bash
# build.sh - Build a specified target with optional flags
# Usage: build.sh <target> [-dev] [-debug] [-release] [-clean] [-notest]
# Example: build.sh Applications/TriangleExample -dev -debug -release -clean -notest

set -e

# Default settings
CLEAN=0
RUN_TESTS=1
DISCARD_TESTS=0
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
      # Discard test macros and skip test execution
      DISCARD_TESTS=1
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
  if [[ $DISCARD_TESTS -eq 1 ]]; then
    cmake -B build -S . -G "Ninja Multi-Config" -D__TEST__=0 -D__UNIT_TEST__=0
  else
    cmake -B build -S . -G "Ninja Multi-Config"
  fi
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
