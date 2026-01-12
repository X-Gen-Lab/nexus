#!/bin/bash
# Nexus Documentation Generator for Linux/macOS
# Usage: ./docs.sh [doxygen|sphinx|all]

set -e

TARGET="${1:-all}"

echo "========================================"
echo "Nexus Documentation Generator"
echo "Target: ${TARGET}"
echo "========================================"

generate_doxygen() {
    echo "Generating Doxygen documentation..."
    if ! command -v doxygen &> /dev/null; then
        echo "Doxygen not found! Please install Doxygen."
        exit 1
    fi
    doxygen Doxyfile
    echo "Doxygen documentation generated in docs/api/html"
}

generate_sphinx() {
    echo "Generating Sphinx documentation..."
    if ! command -v sphinx-build &> /dev/null; then
        echo "Sphinx not found! Please install Sphinx."
        exit 1
    fi
    cd docs/sphinx
    sphinx-build -b html . _build/html
    cd ../..
    echo "Sphinx documentation generated in docs/sphinx/_build/html"
}

case $TARGET in
    doxygen)
        generate_doxygen
        ;;
    sphinx)
        generate_sphinx
        ;;
    all)
        generate_doxygen
        generate_sphinx
        ;;
    *)
        echo "Unknown target: ${TARGET}"
        exit 1
        ;;
esac

echo "========================================"
echo "Documentation generation complete!"
echo "========================================"
