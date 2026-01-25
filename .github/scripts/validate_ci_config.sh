#!/bin/bash
##############################################################################
# Validate CI/CD Configuration
#
# This script validates that the CI/CD pipeline configuration is correct
# and all required files are present.
#
# Usage:
#   ./validate_ci_config.sh
##############################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_error() {
    echo -e "${RED}❌ ERROR: $@${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $@${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  WARNING: $@${NC}"
}

print_info() {
    echo -e "${CYAN}ℹ️  $@${NC}"
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== Validating CI/CD Configuration ==="
echo ""

# Check required files
print_info "Checking required files..."

REQUIRED_FILES=(
    ".github/workflows/test.yml"
    "codecov.yml"
    ".github/CI_CD_GUIDE.md"
    "scripts/coverage/run_coverage_linux.sh"
    "scripts/coverage/run_coverage_windows.ps1"
)

ALL_FILES_PRESENT=true
for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$PROJECT_ROOT/$file" ]; then
        print_success "Found: $file"
    else
        print_error "Missing: $file"
        ALL_FILES_PRESENT=false
    fi
done

if [ "$ALL_FILES_PRESENT" = false ]; then
    print_error "Some required files are missing!"
    exit 1
fi

echo ""

# Validate YAML syntax
print_info "Validating YAML syntax..."

if command -v yamllint &> /dev/null; then
    yamllint "$PROJECT_ROOT/.github/workflows/test.yml" && \
        print_success "test.yml syntax is valid" || \
        print_error "test.yml has syntax errors"

    yamllint "$PROJECT_ROOT/codecov.yml" && \
        print_success "codecov.yml syntax is valid" || \
        print_error "codecov.yml has syntax errors"
else
    print_warning "yamllint not found, skipping YAML validation"
    print_info "Install with: pip install yamllint"
fi

echo ""

# Check coverage thresholds
print_info "Checking coverage thresholds..."

TEST_YML="$PROJECT_ROOT/.github/workflows/test.yml"
MIN_COVERAGE=$(grep "MIN_COVERAGE=" "$TEST_YML" | head -1 | sed 's/.*MIN_COVERAGE=//' | sed 's/ .*//')
TARGET_COVERAGE=$(grep "TARGET_COVERAGE=" "$TEST_YML" | head -1 | sed 's/.*TARGET_COVERAGE=//' | sed 's/ .*//')

if [ -n "$MIN_COVERAGE" ] && [ -n "$TARGET_COVERAGE" ]; then
    print_success "Minimum coverage threshold: $MIN_COVERAGE%"
    print_success "Target coverage threshold: $TARGET_COVERAGE%"

    # Validate thresholds are reasonable
    if (( $(echo "$MIN_COVERAGE < 90" | bc -l) )); then
        print_warning "Minimum coverage threshold is below 90%"
    fi

    if (( $(echo "$TARGET_COVERAGE < $MIN_COVERAGE" | bc -l) )); then
        print_error "Target coverage is less than minimum coverage!"
        exit 1
    fi
else
    print_error "Could not find coverage thresholds in test.yml"
    exit 1
fi

echo ""

# Check codecov configuration
print_info "Checking codecov configuration..."

CODECOV_YML="$PROJECT_ROOT/codecov.yml"
if grep -q "target: 100%" "$CODECOV_YML"; then
    print_success "Codecov target is set to 100%"
else
    print_warning "Codecov target is not set to 100%"
fi

if grep -q "native-hal" "$CODECOV_YML"; then
    print_success "Codecov flag 'native-hal' is configured"
else
    print_error "Codecov flag 'native-hal' is missing"
    exit 1
fi

echo ""

# Check coverage scripts are executable
print_info "Checking coverage scripts..."

COVERAGE_SCRIPT_LINUX="$PROJECT_ROOT/scripts/coverage/run_coverage_linux.sh"
if [ -x "$COVERAGE_SCRIPT_LINUX" ]; then
    print_success "Linux coverage script is executable"
else
    print_warning "Linux coverage script is not executable"
    print_info "Run: chmod +x $COVERAGE_SCRIPT_LINUX"
fi

echo ""

# Check for required tools
print_info "Checking required tools..."

TOOLS=(
    "cmake:CMake"
    "lcov:LCOV"
    "genhtml:LCOV"
    "python3:Python 3"
)

ALL_TOOLS_PRESENT=true
for tool_info in "${TOOLS[@]}"; do
    IFS=':' read -r cmd name <<< "$tool_info"
    if command -v "$cmd" &> /dev/null; then
        version=$($cmd --version 2>&1 | head -1)
        print_success "$name found: $version"
    else
        print_warning "$name not found (required for local testing)"
        ALL_TOOLS_PRESENT=false
    fi
done

if [ "$ALL_TOOLS_PRESENT" = false ]; then
    print_info "Some tools are missing but CI will still work"
fi

echo ""

# Summary
print_info "=== Validation Summary ==="
print_success "CI/CD configuration is valid!"
print_info "Coverage thresholds: Min=$MIN_COVERAGE%, Target=$TARGET_COVERAGE%"
print_info "All required files are present"
print_info "Configuration is ready for use"

echo ""
print_info "Next steps:"
echo "  1. Commit the CI/CD configuration files"
echo "  2. Push to trigger the CI pipeline"
echo "  3. Monitor the pipeline in GitHub Actions"
echo "  4. Review coverage reports"

