#!/bin/bash
#-----------------------------------------------------------------------------
# Nexus Build Script
#-----------------------------------------------------------------------------
# This script automatically configures and builds the project based on
# Kconfig configuration.
#
# Usage:
#   ./scripts/build.sh [options]
#
# Options:
#   --config        Run menuconfig before building
#   --clean         Clean build directory before building
#   --preset NAME   Use specific CMake preset (overrides auto-detection)
#   --help          Show this help message
#
# Author: Nexus Team
#-----------------------------------------------------------------------------

set -e  # Exit on error

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

# Default options
RUN_CONFIG=false
CLEAN_BUILD=false
PRESET_NAME=""

#-----------------------------------------------------------------------------
# Functions
#-----------------------------------------------------------------------------

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    cat << EOF
Nexus Build Script

Usage: $0 [options]

Options:
    --config        Run menuconfig before building
    --clean         Clean build directory before building
    --preset NAME   Use specific CMake preset (overrides auto-detection)
    --help          Show this help message

Examples:
    # Configure and build
    $0 --config

    # Clean and build
    $0 --clean

    # Use specific preset
    $0 --preset windows-arm-debug

    # Configure, clean, and build
    $0 --config --clean

EOF
}

#-----------------------------------------------------------------------------
# Parse arguments
#-----------------------------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case $1 in
        --config)
            RUN_CONFIG=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --preset)
            PRESET_NAME="$2"
            shift 2
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

#-----------------------------------------------------------------------------
# Main script
#-----------------------------------------------------------------------------

cd "$PROJECT_ROOT"

# Step 1: Run menuconfig if requested
if [ "$RUN_CONFIG" = true ]; then
    print_header "Step 1: Configuration"
    print_info "Running menuconfig..."

    if command -v menuconfig &> /dev/null; then
        menuconfig
    else
        print_error "menuconfig not found. Please install it first."
        exit 1
    fi

    echo ""
fi

# Step 2: Detect preset from Kconfig
if [ -z "$PRESET_NAME" ]; then
    print_header "Step 2: Detect CMake Preset"

    if [ ! -f ".config" ]; then
        print_error ".config file not found. Please run 'menuconfig' first."
        exit 1
    fi

    print_info "Detecting preset from Kconfig..."

    if [ -f "scripts/generate_cmake_preset.py" ]; then
        PRESET_NAME=$(python3 scripts/generate_cmake_preset.py --config .config --output .cmake_preset)

        if [ $? -ne 0 ]; then
            print_error "Failed to detect preset from Kconfig"
            exit 1
        fi

        # Read preset name from file
        if [ -f ".cmake_preset" ]; then
            PRESET_NAME=$(cat .cmake_preset)
            rm .cmake_preset
        fi
    else
        print_error "generate_cmake_preset.py not found"
        exit 1
    fi

    echo ""
fi

print_info "Using preset: $PRESET_NAME"

# Step 3: Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_header "Step 3: Clean Build"

    BUILD_DIR="build/$PRESET_NAME"

    if [ -d "$BUILD_DIR" ]; then
        print_info "Cleaning build directory: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    else
        print_info "Build directory does not exist, skipping clean"
    fi

    echo ""
fi

# Step 4: Configure with CMake
print_header "Step 4: CMake Configuration"
print_info "Configuring with preset: $PRESET_NAME"

if ! cmake --preset "$PRESET_NAME"; then
    print_error "CMake configuration failed"
    exit 1
fi

echo ""

# Step 5: Build
print_header "Step 5: Build"
print_info "Building project..."

if ! cmake --build "build/$PRESET_NAME"; then
    print_error "Build failed"
    exit 1
fi

echo ""

# Step 6: Success
print_header "Build Complete"
print_info "Build artifacts are in: build/$PRESET_NAME"

# Show binary files
if [ -d "build/$PRESET_NAME" ]; then
    print_info "Generated files:"
    find "build/$PRESET_NAME" -type f \( -name "*.elf" -o -name "*.bin" -o -name "*.hex" -o -name "*.exe" \) 2>/dev/null | while read file; do
        echo "  - $file"
    done
fi

echo ""
print_info "Build completed successfully!"

exit 0
