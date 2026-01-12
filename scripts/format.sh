#!/bin/bash
# Nexus Code Formatter Script for Linux/macOS
# Usage: ./format.sh [check]

set -e

CHECK_ONLY=0

if [ "$1" = "check" ]; then
    CHECK_ONLY=1
fi

echo "========================================"
echo "Nexus Code Formatter"
echo "========================================"

if ! command -v clang-format &> /dev/null; then
    echo "clang-format not found! Please install LLVM."
    exit 1
fi

# Find all source files
SOURCES=$(find hal osal platforms tests applications -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" 2>/dev/null || true)

if [ -z "$SOURCES" ]; then
    echo "No source files found!"
    exit 0
fi

if [ $CHECK_ONLY -eq 1 ]; then
    echo "Checking code format..."
    FAILED=0
    for file in $SOURCES; do
        if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
            echo "Format check failed for: $file"
            FAILED=1
        fi
    done
    if [ $FAILED -eq 1 ]; then
        echo "Some files need formatting!"
        exit 1
    fi
    echo "All files are properly formatted!"
else
    echo "Formatting code..."
    for file in $SOURCES; do
        echo "Formatting: $file"
        clang-format -i "$file"
    done
    echo "Code formatting complete!"
fi

echo "========================================"
