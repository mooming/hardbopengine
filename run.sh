#!/usr/bin/env bash
# run.sh - Run a built target with optional configuration flags
# Usage: run.sh <target> [-dev] [-debug] [-release]
# Example: run.sh Applications/TriangleExample -dev -debug -release

set -e

# Default configuration list (Dev if none specified)
CONFIGS=()
TARGET=""

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

if [[ -z "$TARGET" ]]; then
  echo "Usage: $0 <target> [-dev] [-debug] [-release]"
  exit 1
fi

# If no config flags given, default to Dev
if [[ ${#CONFIGS[@]} -eq 0 ]]; then
  CONFIGS+=("Dev")
fi

# Determine binary name (basename of target directory)
BIN_NAME=$(basename "$TARGET")

# Run for each requested configuration
for CONFIG in "${CONFIGS[@]}"; do
  BIN_PATH="./build/$TARGET/$CONFIG/$BIN_NAME"
  if [[ -x "$BIN_PATH" ]]; then
    echo "Running $BIN_NAME with configuration $CONFIG..."
    "$BIN_PATH"
  else
    echo "Executable not found: $BIN_PATH"
    exit 1
  fi
done
