#!/usr/bin/env bash
#-----------------------------------------------------------------------------
# setup_deps.sh - One-command dependency initialization
#-----------------------------------------------------------------------------
# Nexus Dependency Setup Script
# Author: Nexus Team
#
# This script automatically initializes all required dependencies for Nexus.
# It detects the platform and only initializes necessary submodules.
#
# Usage:
#   ./scripts/setup_deps.sh [options]
#
# Options:
#   --all           Initialize all submodules (not recommended)
#   --platform=X    Initialize for specific platform (stm32, native, etc.)
#   --series=X      STM32 series (f4, h7, l4, etc.) - required for STM32
#   --parallel=N    Use N parallel jobs (default: 4)
#   --help          Show this help message
#
#-----------------------------------------------------------------------------

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default values
INIT_ALL=false
PLATFORM=""
SERIES=""
PARALLEL_JOBS=4

#-----------------------------------------------------------------------------
# Helper Functions
#-----------------------------------------------------------------------------

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    cat << EOF
Nexus Dependency Setup Script

Usage: $0 [options]

Options:
  --all              Initialize all submodules (not recommended, ~2GB)
  --platform=X       Initialize for specific platform:
                       - stm32: STM32 microcontrollers
                       - native: Native host builds (tests)
  --series=X         STM32 series (required for --platform=stm32):
                       - f4, f7, h7, l4, g4, etc.
  --parallel=N       Use N parallel jobs (default: 4)
  --help             Show this help message

Examples:
  # Initialize for STM32F4 development
  $0 --platform=stm32 --series=f4

  # Initialize for native testing
  $0 --platform=native

  # Initialize everything (not recommended)
  $0 --all

Auto-detection:
  If no options are provided, the script will try to detect the platform
  from .config file (if exists).

EOF
}

#-----------------------------------------------------------------------------
# Parse Arguments
#-----------------------------------------------------------------------------

for arg in "$@"; do
    case $arg in
        --all)
            INIT_ALL=true
            ;;
        --platform=*)
            PLATFORM="${arg#*=}"
            ;;
        --series=*)
            SERIES="${arg#*=}"
            ;;
        --parallel=*)
            PARALLEL_JOBS="${arg#*=}"
            ;;
        --help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $arg"
            show_help
            exit 1
            ;;
    esac
done

#-----------------------------------------------------------------------------
# Auto-detect Platform from .config
#-----------------------------------------------------------------------------

if [ -z "$PLATFORM" ] && [ ! "$INIT_ALL" = true ]; then
    CONFIG_FILE="${PROJECT_ROOT}/.config"

    if [ -f "$CONFIG_FILE" ]; then
        print_info "Detecting platform from .config..."

        # Extract platform from CONFIG_PLATFORM_NAME
        if grep -q "CONFIG_PLATFORM_NAME=\"stm32\"" "$CONFIG_FILE"; then
            PLATFORM="stm32"

            # Try to detect series
            if [ -z "$SERIES" ]; then
                if grep -q "CONFIG_PLATFORM_STM32_SERIES=\"\(.*\)\"" "$CONFIG_FILE"; then
                    SERIES=$(grep "CONFIG_PLATFORM_STM32_SERIES=" "$CONFIG_FILE" | sed 's/.*="\(.*\)"/\1/')
                fi
            fi
        elif grep -q "CONFIG_PLATFORM_NAME=\"native\"" "$CONFIG_FILE"; then
            PLATFORM="native"
        fi

        if [ -n "$PLATFORM" ]; then
            print_success "Detected platform: $PLATFORM"
            [ -n "$SERIES" ] && print_success "Detected series: $SERIES"
        fi
    fi
fi

#-----------------------------------------------------------------------------
# Validate Arguments
#-----------------------------------------------------------------------------

if [ ! "$INIT_ALL" = true ] && [ -z "$PLATFORM" ]; then
    print_error "No platform specified and auto-detection failed"
    echo ""
    show_help
    exit 1
fi

if [ "$PLATFORM" = "stm32" ] && [ -z "$SERIES" ]; then
    print_error "STM32 series is required for STM32 platform"
    echo ""
    echo "Available series: f0, f1, f2, f3, f4, f7, g0, g4, h5, h7, l0, l1, l4, l5, u0, u3, u5, c0"
    exit 1
fi

#-----------------------------------------------------------------------------
# Check Git
#-----------------------------------------------------------------------------

if ! command -v git &> /dev/null; then
    print_error "Git is not installed"
    echo ""
    echo "Please install Git:"
    echo "  - Ubuntu/Debian: sudo apt-get install git"
    echo "  - macOS: brew install git"
    echo "  - Windows: https://git-scm.com/download/win"
    exit 1
fi

#-----------------------------------------------------------------------------
# Initialize Submodules
#-----------------------------------------------------------------------------

cd "$PROJECT_ROOT"

print_info "Initializing dependencies..."
echo ""

init_submodule() {
    local path=$1
    local name=$2

    if [ -d "$path" ] && [ "$(ls -A $path)" ]; then
        print_success "Already initialized: $name"
        return 0
    fi

    print_info "Initializing: $name"
    if git submodule update --init --depth 1 --jobs "$PARALLEL_JOBS" -- "$path"; then
        print_success "Initialized: $name"
    else
        print_error "Failed to initialize: $name"
        return 1
    fi
}

if [ "$INIT_ALL" = true ]; then
    print_warning "Initializing ALL submodules (this may take a while and download ~2GB)..."
    git submodule update --init --recursive --depth 1 --jobs "$PARALLEL_JOBS"
    print_success "All submodules initialized"

elif [ "$PLATFORM" = "stm32" ]; then
    print_info "Initializing STM32${SERIES^^} dependencies..."
    echo ""

    # CMSIS Core (required for all STM32)
    init_submodule "vendors/arm/CMSIS_5" "CMSIS Core (ARM)"

    # CMSIS Device (series-specific)
    init_submodule "vendors/st/cmsis_device_${SERIES}" "CMSIS Device (STM32${SERIES^^})"

    # HAL Driver (series-specific)
    init_submodule "vendors/st/stm32${SERIES}xx_hal_driver" "HAL Driver (STM32${SERIES^^})"

    # FreeRTOS (if configured)
    if [ -f ".config" ] && grep -q "CONFIG_OSAL_BACKEND_FREERTOS=y" ".config"; then
        init_submodule "ext/freertos" "FreeRTOS Kernel"
    fi

    echo ""
    print_success "STM32${SERIES^^} dependencies initialized"

elif [ "$PLATFORM" = "native" ]; then
    print_info "Initializing native platform dependencies..."
    echo ""

    # GoogleTest (for unit tests)
    init_submodule "ext/googletest" "GoogleTest"

    # FreeRTOS (if configured)
    if [ -f ".config" ] && grep -q "CONFIG_OSAL_BACKEND_FREERTOS=y" ".config"; then
        init_submodule "ext/freertos" "FreeRTOS Kernel"
    fi

    echo ""
    print_success "Native platform dependencies initialized"

else
    print_error "Unknown platform: $PLATFORM"
    exit 1
fi

#-----------------------------------------------------------------------------
# Summary
#-----------------------------------------------------------------------------

echo ""
echo "========================================="
echo "  Dependency Setup Complete"
echo "========================================="
echo ""
echo "Next steps:"
echo "  1. Configure build:"
if [ "$PLATFORM" = "stm32" ]; then
    echo "       cmake --preset stm32${SERIES}"
elif [ "$PLATFORM" = "native" ]; then
    echo "       cmake --preset native"
fi
echo "  2. Build project:"
echo "       cmake --build build"
echo ""
print_success "Ready to build!"
