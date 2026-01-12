#!/bin/bash
# Nexus Clean Script for Linux/macOS
# Usage: ./clean.sh [all]

echo "========================================"
echo "Nexus Clean Script"
echo "========================================"

echo "Removing build directories..."
rm -rf build build-Debug build-Release build-test build-check build-verify out cmake-build-*

if [ "$1" = "all" ]; then
    echo "Removing generated documentation..."
    rm -rf docs/api/html docs/api/xml docs/sphinx/_build
    
    echo "Removing test artifacts..."
    rm -rf Testing
    rm -f test_results.xml
fi

echo "========================================"
echo "Clean complete!"
echo "========================================"
