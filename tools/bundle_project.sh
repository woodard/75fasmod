#!/bin/bash

OUTPUT="ai_prompt_bundle_full.txt"
echo "Generating AI prompt bundle in $OUTPUT..."

# Clear or create the output file
> "$OUTPUT"

# ------------------------------------------------------------------------------
# 1. Append Directory Structure
# ------------------------------------------------------------------------------
echo "================================================================================" >> "$OUTPUT"
echo "PROJECT DIRECTORY STRUCTURE" >> "$OUTPUT"
echo "================================================================================" >> "$OUTPUT"
echo "" >> "$OUTPUT"

# Use 'tree' if available for better readability, otherwise fallback to 'find'
if command -v tree >/dev/null 2>&1; then
    tree -a -I '.git|.libs|.deps|autom4te.cache|build|*.o|*.lo|*.la|*.so' >> "$OUTPUT"
else
    find . -type f \
        -not -path '*/\.git/*' \
        -not -path '*/\.libs/*' \
        -not -path '*/\.deps/*' \
        -not -path '*/autom4te.cache/*' | sort >> "$OUTPUT"
fi
echo -e "\n\n" >> "$OUTPUT"

# ------------------------------------------------------------------------------
# 2. Append File Contents
# ------------------------------------------------------------------------------
# Find all relevant source, build, and documentation files
find . -type f \
    \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \
       -o -name "*.sh" -o -name "*.am" -o -name "*.ac" -o -name "*.md" \) \
    -not -path '*/\.git/*' \
    -not -path '*/\.libs/*' \
    -not -path '*/\.deps/*' \
    -not -path '*/autom4te.cache/*' | sort | while read -r FILE; do

    # Strip the leading './' from the find output for cleaner paths
    CLEAN_NAME="${FILE#./}"

    # Exclude massive generated Autotools scripts that waste context window
    if [[ "$CLEAN_NAME" == "ltmain.sh" || "$CLEAN_NAME" == "config.sub" || "$CLEAN_NAME" == "config.guess" ]]; then
        continue
    fi

    echo "Adding: $CLEAN_NAME"

    echo "================================================================================" >> "$OUTPUT"
    echo "FILE: $CLEAN_NAME" >> "$OUTPUT"
    echo "================================================================================" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
    
    # Append the file content
    cat "$FILE" >> "$OUTPUT"
    
    echo -e "\n\n" >> "$OUTPUT"
done

echo "Done! Bundle created at: $OUTPUT"