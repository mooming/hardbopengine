#!/bin/bash

# Usage: ./ai_prompt_files.sh <directory> "<prompt_template_or_text>"
# Example: ./ai_prompt_files.sh ./src "/review"
# Example: ./ai_prompt_files.sh ./Engine "Summarize this file:"

TARGET_DIR=$1
PROMPT=$2

if [ -z "$TARGET_DIR" ] || [ -z "$PROMPT" ]; then
    echo "Usage: $0 <directory> \"<prompt>\""
    exit 1
fi

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist."
    exit 1
fi

if ! command -v pi &>/dev/null; then
    echo "Error: 'pi' command not found. Install opencode's CLI tool first."
    exit 1
fi

echo "Starting batch processing in: $TARGET_DIR"
echo "Prompt template: $PROMPT"
echo "------------------------------------------"

# 1. find: Searches recursively for files
# 2. \( ... \): Groups the name patterns
# 3. -name: Matches extensions for C, C++, Obj-C, and Obj-C++
# 4. -print0: Uses a null character to separate filenames (handles spaces/special chars)
# 5. while IFS= read -r -d '': Reads the null-terminated strings correctly
find "$TARGET_DIR" -type f \( \
    -name "*.c" -o \
    -name "*.h" -o \
    -name "*.cpp" -o \
    -name "*.hpp" -o \
    -name "*.cc" -o \
    -name "*.cxx" -o \
    -name "*.m" -o \
    -name "*.mm" \
\) -print0 | while IFS= read -r -d '' file; do
    
    echo ">>> Processing: $file"
    
    # Executes pi with the prompt followed by the specific file path
    # This treats each file as a new command/turn for the CLI
    pi $PROMPT "$file"
    
    echo "------------------------------------------"
done

echo "Batch processing complete."
