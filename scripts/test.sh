#!/bin/bash
# Nexus Test Script for Linux/macOS
# Usage: ./test.sh [filter] [verbose]

set -e

BUILD_DIR="build-Debug"
FILTER="*"
VERBOSE=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        verbose)
            VERBOSE=1
            ;;
        *)
            FILTER="$arg"
            ;;
    esac
done

TEST_EXE="${BUILD_DIR}/tests/nexus_tests"

echo "========================================"
echo "Nexus Test Runner"
echo "Filter: ${FILTER}"
echo "========================================"

if [ ! -f "${TEST_EXE}" ]; then
    echo "Test executable not found!"
    echo "Please run ./build.sh first."
    exit 1
fi

if [ $VERBOSE -eq 1 ]; then
    "${TEST_EXE}" --gtest_filter="${FILTER}" --gtest_color=yes
else
    "${TEST_EXE}" --gtest_filter="${FILTER}" --gtest_color=yes --gtest_brief=1
fi

echo "========================================"
echo "All tests passed!"
echo "========================================"
