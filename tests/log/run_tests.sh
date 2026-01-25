#!/bin/bash
###############################################################################
# Log Framework Test Runner (Linux/macOS)
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

echo "=== Log Framework Test Runner ==="
echo "Project root: ${PROJECT_ROOT}"
echo "Build directory: ${BUILD_DIR}"
echo ""

# Parse command line arguments
RUN_UNIT=1
RUN_INTEGRATION=1
RUN_PERFORMANCE=1
RUN_THREAD_SAFETY=1
RUN_PROPERTY=1
VERBOSE=0
COVERAGE=0

while [[ $# -gt 0 ]]; do
    case $1 in
        --unit-only)
            RUN_INTEGRATION=0
            RUN_PERFORMANCE=0
            RUN_THREAD_SAFETY=0
            RUN_PROPERTY=0
            shift
            ;;
        --integration-only)
            RUN_UNIT=0
            RUN_PERFORMANCE=0
            RUN_THREAD_SAFETY=0
            RUN_PROPERTY=0
            shift
            ;;
        --performance-only)
            RUN_UNIT=0
            RUN_INTEGRATION=0
            RUN_THREAD_SAFETY=0
            RUN_PROPERTY=0
            shift
            ;;
        --thread-safety-only)
            RUN_UNIT=0
            RUN_INTEGRATION=0
            RUN_PERFORMANCE=0
            RUN_PROPERTY=0
            shift
            ;;
        --property-only)
            RUN_UNIT=0
            RUN_INTEGRATION=0
            RUN_PERFORMANCE=0
            RUN_THREAD_SAFETY=0
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --coverage)
            COVERAGE=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --unit-only           Run only unit tests"
            echo "  --integration-only    Run only integration tests"
            echo "  --performance-only    Run only performance tests"
            echo "  --thread-safety-only  Run only thread safety tests"
            echo "  --property-only       Run only property-based tests"
            echo "  --verbose, -v         Verbose output"
            echo "  --coverage            Generate coverage report"
            echo "  --help, -h            Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check if build directory exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo -e "${YELLOW}Build directory not found. Creating...${NC}"
    mkdir -p "${BUILD_DIR}"
fi

# Configure and build
echo "=== Configuring CMake ==="
cd "${BUILD_DIR}"

CMAKE_ARGS="-DENABLE_TESTS=ON"
if [ ${COVERAGE} -eq 1 ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_COVERAGE=ON"
fi

cmake ${CMAKE_ARGS} ..

echo ""
echo "=== Building Tests ==="
cmake --build . --target log_tests -j$(nproc)

echo ""
echo "=== Running Tests ==="

TEST_FILTER=""

if [ ${RUN_UNIT} -eq 1 ] && [ ${RUN_INTEGRATION} -eq 0 ] && [ ${RUN_PERFORMANCE} -eq 0 ] && [ ${RUN_THREAD_SAFETY} -eq 0 ] && [ ${RUN_PROPERTY} -eq 0 ]; then
    TEST_FILTER="--gtest_filter=LogTest.*"
elif [ ${RUN_INTEGRATION} -eq 1 ] && [ ${RUN_UNIT} -eq 0 ] && [ ${RUN_PERFORMANCE} -eq 0 ] && [ ${RUN_THREAD_SAFETY} -eq 0 ] && [ ${RUN_PROPERTY} -eq 0 ]; then
    TEST_FILTER="--gtest_filter=LogIntegrationTest.*"
elif [ ${RUN_PERFORMANCE} -eq 1 ] && [ ${RUN_UNIT} -eq 0 ] && [ ${RUN_INTEGRATION} -eq 0 ] && [ ${RUN_THREAD_SAFETY} -eq 0 ] && [ ${RUN_PROPERTY} -eq 0 ]; then
    TEST_FILTER="--gtest_filter=LogPerformanceTest.*"
elif [ ${RUN_THREAD_SAFETY} -eq 1 ] && [ ${RUN_UNIT} -eq 0 ] && [ ${RUN_INTEGRATION} -eq 0 ] && [ ${RUN_PERFORMANCE} -eq 0 ] && [ ${RUN_PROPERTY} -eq 0 ]; then
    TEST_FILTER="--gtest_filter=LogThreadSafetyTest.*"
elif [ ${RUN_PROPERTY} -eq 1 ] && [ ${RUN_UNIT} -eq 0 ] && [ ${RUN_INTEGRATION} -eq 0 ] && [ ${RUN_PERFORMANCE} -eq 0 ] && [ ${RUN_THREAD_SAFETY} -eq 0 ]; then
    TEST_FILTER="--gtest_filter=LogPropertyTest.*"
fi

GTEST_ARGS="${TEST_FILTER}"
if [ ${VERBOSE} -eq 1 ]; then
    GTEST_ARGS="${GTEST_ARGS} --gtest_print_time=1"
fi

./tests/log/log_tests ${GTEST_ARGS}

TEST_RESULT=$?

# Generate coverage report if requested
if [ ${COVERAGE} -eq 1 ] && [ ${TEST_RESULT} -eq 0 ]; then
    echo ""
    echo "=== Generating Coverage Report ==="

    # Check if lcov is installed
    if command -v lcov &> /dev/null; then
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/tests/*' '*/build/*' --output-file coverage.info
        lcov --list coverage.info

        # Generate HTML report if genhtml is available
        if command -v genhtml &> /dev/null; then
            genhtml coverage.info --output-directory coverage_html
            echo -e "${GREEN}Coverage report generated in: ${BUILD_DIR}/coverage_html/index.html${NC}"
        fi
    else
        echo -e "${YELLOW}lcov not found. Skipping coverage report.${NC}"
    fi
fi

echo ""
if [ ${TEST_RESULT} -eq 0 ]; then
    echo -e "${GREEN}=== All Tests Passed ===${NC}"
else
    echo -e "${RED}=== Tests Failed ===${NC}"
fi

exit ${TEST_RESULT}
