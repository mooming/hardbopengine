#!/usr/bin/env bash
# build_all_tools.sh - Build every tool under Tools/ and copy executables to Tools/bin/.

set -e

TOOLS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$TOOLS_ROOT/bin"

mkdir -p "$BIN_DIR"

for tool_dir in "$TOOLS_ROOT"/*/; do
    if [ -f "${tool_dir}CMakeLists.txt" ]; then
        echo "Building tool: $(basename "$tool_dir")"

        BUILD_DIR="${tool_dir}build"
        cmake -B "$BUILD_DIR" -S "${tool_dir%/}" -G "Ninja Multi-Config"
        cmake --build "$BUILD_DIR" --config Release

        while IFS= read -r -d '' exe; do
            if file "$exe" | grep -qE '(Mach-O|ELF).*executable'; then
                echo "Copying $(basename "$exe") to Tools/bin/"
                cp "$exe" "$BIN_DIR/$(basename "$exe")"
            fi
        done < <(find "$BUILD_DIR" -path "*/Release/*" -type f -perm -u+x \
            ! -path "*/CMakeFiles/*" -print0)
    fi
done

echo "All tools built and copied to $BIN_DIR"
