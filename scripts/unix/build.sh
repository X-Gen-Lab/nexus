#!/bin/bash
# Nexus Build Script for Linux/macOS
# Usage: ./build.sh [debug|release] [clean]

set -e

BUILD_TYPE="Debug"
CLEAN=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        debug|Debug)
            BUILD_TYPE="Debug"
            ;;
        release|Release)
            BUILD_TYPE="Release"
            ;;
        clean)
            CLEAN=1
            ;;
    esac
done

BUILD_DIR="build-${BUILD_TYPE}"

echo "========================================"
echo "Nexus Build Script"
echo "Build Type: ${BUILD_TYPE}"
echo "Build Dir:  ${BUILD_DIR}"
echo "========================================"

if [ $CLEAN -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Creating build directory..."
    mkdir -p "${BUILD_DIR}"
fi

cd "${BUILD_DIR}"

echo "Configuring CMake..."
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DNEXUS_BUILD_TESTS=ON ..

echo "Building..."
cmake --build . --config "${BUILD_TYPE}" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "========================================"
echo "Build completed successfully!"
echo "========================================"

cd ..
