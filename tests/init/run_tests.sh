#!/bin/bash
###############################################################################
# Init Framework Test Runner (Linux/macOS)
###############################################################################

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Test options
RUN_UNIT_TESTS=1
RUN_INTEGRATION_TESTS=1
RUN_PERFORMANCE_TESTS=1
GENERATE_COVERAGE=0
VERBOSE=0

###############################################################################
# Functions
###############################################################################

print_header() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}$1${NC}"
    echo -e "${GREEN}========================================${NC}"
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

print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
    -u, --unit              Run unit tests only
    -i, --integration       Run integration tests only
    -p, --performance       Run performance tests only
    -c, --coverage          Generate coverage report
    -v, --verbose           Verbose output
    -h, --help              Show this help message

Examples:
    $0                      Run all tests
    $0 -u                   Run unit tests only
    $0 -c                   Run all tests with coverage
    $0 -u -c                Run unit tests with coverage

EOF
}

###############################################################################
# Parse Arguments
###############################################################################

while [[ $# -gt 0 ]]; do
    case $1 in
        -u|--unit)
            RUN_INTEGRATION_TESTS=0
            RUN_PERFORMANCE_TESTS=0
            shift
            ;;
        -i|--integration)
            RUN_UNIT_TESTS=0
            RUN_PERFORMANCE_TESTS=0
            shift
            ;;
        -p|--performance)
            RUN_UNIT_TESTS=0
            RUN_INTEGRATION_TESTS=0
            shift
            ;;
        -c|--coverage)
            GENERATE_COVERAGE=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

###############################################################################
# Main
###############################################################################

print_header "Init Framework Test Runner"

# Check if build directory exists
if [ ! -d "${BUILD_DIR}" ]; then
    print_error "Build directory not found: ${BUILD_DIR}"
    print_info "Please run CMake to configure the project first"
    exit 1
fi

# Check if test executable exists
TEST_EXECUTABLE="${BUILD_DIR}/tests/init/init_tests"
if [ ! -f "${TEST_EXECUTABLE}" ]; then
    print_error "Test executable not found: ${TEST_EXECUTABLE}"
    print_info "Please build the project first"
    exit 1
fi

# Prepare test arguments
TEST_ARGS=""
if [ ${VERBOSE} -eq 1 ]; then
    TEST_ARGS="${TEST_ARGS} --gtest_print_time=1"
fi

# Run tests
print_info "Running Init Framework tests..."
echo ""

if [ ${RUN_UNIT_TESTS} -eq 1 ]; then
    print_header "Unit Tests"
    "${TEST_EXECUTABLE}" --gtest_filter="NxInit*:NxStartup*:NxFirmwareInfo*" ${TEST_ARGS}
    echo ""
fi

if [ ${RUN_INTEGRATION_TESTS} -eq 1 ]; then
    print_header "Integration Tests"
    "${TEST_EXECUTABLE}" --gtest_filter="InitIntegration*" ${TEST_ARGS}
    echo ""
fi

if [ ${RUN_PERFORMANCE_TESTS} -eq 1 ]; then
    print_header "Performance Tests"
    "${TEST_EXECUTABLE}" --gtest_filter="InitPerformance*" ${TEST_ARGS}
    echo ""
fi

# Generate coverage report
if [ ${GENERATE_COVERAGE} -eq 1 ]; then
    print_header "Generating Coverage Report"

    # Check if lcov is installed
    if ! command -v lcov &> /dev/null; then
        print_warning "lcov not found, skipping coverage report"
    else
        cd "${BUILD_DIR}"

        # Capture coverage data
        print_info "Capturing coverage data..."
        lcov --capture --directory . --output-file coverage.info

        # Remove system files
        print_info "Filtering coverage data..."
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --remove coverage.info '*/tests/*' --output-file coverage.info
        lcov --remove coverage.info '*/ext/*' --output-file coverage.info

        # Generate HTML report
        print_info "Generating HTML report..."
        genhtml coverage.info --output-directory coverage_html

        print_info "Coverage report generated: ${BUILD_DIR}/coverage_html/index.html"
    fi
fi

print_header "Test Run Complete"
print_info "All tests passed successfully!"

exit 0
