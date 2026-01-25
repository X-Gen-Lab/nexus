#!/bin/bash
##############################################################################
# Config Manager Test Runner Script
##############################################################################

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Default options
ENABLE_COVERAGE=0
ENABLE_VALGRIND=0
ENABLE_SANITIZERS=0
VERBOSE=0
TEST_FILTER=""
PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)

##############################################################################
# Functions
##############################################################################

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
    -c, --coverage          Enable code coverage analysis
    -v, --valgrind          Run tests with Valgrind
    -s, --sanitizers        Enable AddressSanitizer and UBSanitizer
    -f, --filter PATTERN    Run only tests matching PATTERN
    -j, --jobs N            Number of parallel jobs (default: ${PARALLEL_JOBS})
    -V, --verbose           Verbose output
    -h, --help              Show this help message

Examples:
    $0                      # Run all tests
    $0 -c                   # Run with coverage
    $0 -f "ConfigStore*"    # Run only ConfigStore tests
    $0 -c -v                # Run with coverage and Valgrind

EOF
}

##############################################################################
# Parse Arguments
##############################################################################

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--coverage)
            ENABLE_COVERAGE=1
            shift
            ;;
        -v|--valgrind)
            ENABLE_VALGRIND=1
            shift
            ;;
        -s|--sanitizers)
            ENABLE_SANITIZERS=1
            shift
            ;;
        -f|--filter)
            TEST_FILTER="$2"
            shift 2
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -V|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

##############################################################################
# Build Tests
##############################################################################

print_header "Building Config Manager Tests"

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# CMake options
CMAKE_OPTS="-DCMAKE_BUILD_TYPE=Debug"

if [ ${ENABLE_COVERAGE} -eq 1 ]; then
    CMAKE_OPTS="${CMAKE_OPTS} -DENABLE_COVERAGE=ON"
    print_info "Coverage enabled"
fi

if [ ${ENABLE_SANITIZERS} -eq 1 ]; then
    CMAKE_OPTS="${CMAKE_OPTS} -DENABLE_SANITIZERS=ON"
    print_info "Sanitizers enabled"
fi

# Configure
print_info "Configuring..."
cmake ${CMAKE_OPTS} .. || {
    print_error "CMake configuration failed"
    exit 1
}

# Build
print_info "Building..."
cmake --build . --target config_tests -j ${PARALLEL_JOBS} || {
    print_error "Build failed"
    exit 1
}

print_success "Build completed"

##############################################################################
# Run Tests
##############################################################################

print_header "Running Config Manager Tests"

# Test options
TEST_OPTS=""

if [ -n "${TEST_FILTER}" ]; then
    TEST_OPTS="${TEST_OPTS} --gtest_filter=${TEST_FILTER}"
    print_info "Filter: ${TEST_FILTER}"
fi

if [ ${VERBOSE} -eq 1 ]; then
    TEST_OPTS="${TEST_OPTS} --gtest_print_time=1"
fi

# Run with Valgrind if requested
if [ ${ENABLE_VALGRIND} -eq 1 ]; then
    print_info "Running with Valgrind..."

    if ! command -v valgrind &> /dev/null; then
        print_error "Valgrind not found"
        exit 1
    fi

    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --error-exitcode=1 \
        ./tests/config_tests ${TEST_OPTS} || {
        print_error "Tests failed with Valgrind"
        exit 1
    }
else
    # Run tests normally
    ./tests/config_tests ${TEST_OPTS} || {
        print_error "Tests failed"
        exit 1
    }
fi

print_success "All tests passed"

##############################################################################
# Generate Coverage Report
##############################################################################

if [ ${ENABLE_COVERAGE} -eq 1 ]; then
    print_header "Generating Coverage Report"

    if ! command -v lcov &> /dev/null; then
        print_warning "lcov not found, skipping coverage report"
    else
        # Capture coverage data
        print_info "Capturing coverage data..."
        lcov --capture \
             --directory . \
             --output-file coverage.info \
             --rc lcov_branch_coverage=1 || {
            print_error "Failed to capture coverage data"
            exit 1
        }

        # Filter out system headers and test files
        print_info "Filtering coverage data..."
        lcov --remove coverage.info \
             '/usr/*' \
             '*/tests/*' \
             '*/googletest/*' \
             --output-file coverage_filtered.info \
             --rc lcov_branch_coverage=1

        # Generate HTML report
        print_info "Generating HTML report..."
        genhtml coverage_filtered.info \
                --output-directory coverage_html \
                --title "Config Manager Coverage" \
                --legend \
                --rc lcov_branch_coverage=1 || {
            print_error "Failed to generate HTML report"
            exit 1
        }

        # Print summary
        print_info "Coverage summary:"
        lcov --summary coverage_filtered.info --rc lcov_branch_coverage=1

        print_success "Coverage report generated: ${BUILD_DIR}/coverage_html/index.html"
    fi
fi

##############################################################################
# Summary
##############################################################################

print_header "Test Summary"

echo ""
echo "Build directory: ${BUILD_DIR}"
echo "Test executable: ${BUILD_DIR}/tests/config_tests"

if [ ${ENABLE_COVERAGE} -eq 1 ]; then
    echo "Coverage report: ${BUILD_DIR}/coverage_html/index.html"
fi

echo ""
print_success "All operations completed successfully"
