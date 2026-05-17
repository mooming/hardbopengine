#!/bin/bash

# Usage: ./batch_ai_prompt.sh <directory> "<prompt>" [output_file]
# Example: ./batch_ai_prompt.sh ./Engine "Check naming conventions"
# Example: ./batch_ai_prompt.sh ./Engine "Review for coding standards" review_output.md

TARGET_DIR=$1
PROMPT=$2
OUTPUT_FILE=${3:-review_output.md}

if [ -z "$TARGET_DIR" ] || [ -z "$PROMPT" ]; then
    echo "Usage: $0 <directory> \"<prompt>\" [output_file]"
    echo "  output_file  File to append all reviews into (default: review_output.md)"
    exit 1
fi

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory '$TARGET_DIR' does not exist."
    exit 1
fi

if ! command -v pi &>/dev/null; then
    echo "Error: 'pi' command not found."
    exit 1
fi

# Initialize empty output file
> "$OUTPUT_FILE"

echo "Starting batch processing in: $TARGET_DIR"
echo "Prompt template: $PROMPT"
echo "Output file: $OUTPUT_FILE"
echo "------------------------------------------"

COUNT=0
FAIL=0

# Iterate over all C/C++/Obj-C source files, handling spaces in filenames
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

    # Append a separator and header for this file
    {
        echo ""
        echo "========================================"
        echo "File: $file"
        echo "========================================"
        echo ""
    } >> "$OUTPUT_FILE"

    # Run pi in non-interactive mode (-p) and append output
    if pi -p $PROMPT "$file" >> "$OUTPUT_FILE" 2>&1; then
        echo "    -> Done"
    else
        echo "    -> FAILED"
        {
            echo ""
            echo "**FAILED**"
        } >> "$OUTPUT_FILE"
        FAIL=$((FAIL + 1))
    fi

    COUNT=$((COUNT + 1))
    echo "------------------------------------------"
done

echo ""
echo "Batch processing complete. $COUNT files processed, $FAIL failures."
echo "Accumulated review written to: $OUTPUT_FILE"