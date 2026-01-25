#!/bin/bash
# Coverage comparison script for pull requests
# Compares current coverage with base branch coverage

set -e

CURRENT_COVERAGE=$1
BASE_COVERAGE=$2
OUTPUT_FILE=$3

if [ -z "$CURRENT_COVERAGE" ] || [ -z "$BASE_COVERAGE" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 <current_coverage> <base_coverage> <output_file>"
    exit 1
fi

# Calculate difference
DIFF=$(echo "$CURRENT_COVERAGE - $BASE_COVERAGE" | bc -l)

# Format output
{
    echo "### Coverage Comparison"
    echo ""
    echo "| Metric | Value |"
    echo "|--------|-------|"
    echo "| Current Coverage | ${CURRENT_COVERAGE}% |"
    echo "| Base Coverage | ${BASE_COVERAGE}% |"
    echo "| Difference | ${DIFF}% |"
    echo ""

    if (( $(echo "$DIFF > 0" | bc -l) )); then
        echo "✅ Coverage improved by ${DIFF}%"
    elif (( $(echo "$DIFF < 0" | bc -l) )); then
        echo "⚠️ Coverage decreased by ${DIFF}%"
    else
        echo "➡️ Coverage unchanged"
    fi
} > "$OUTPUT_FILE"

echo "Coverage comparison written to $OUTPUT_FILE"
