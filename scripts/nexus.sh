#!/bin/bash
# Nexus Project Manager - Shell Version
#
# Unified entry point for all Nexus project operations.
# Provides easy access to build, test, format, clean, docs, and CI operations.
#
# Usage: ./nexus.sh <command> [arguments...]
#
# Commands:
#   setup     - Environment setup and configuration
#   build     - Build the project
#   test      - Run tests
#   format    - Format source code
#   clean     - Clean build artifacts
#   docs      - Generate documentation
#   ci        - Run CI pipeline
#   help      - Show help information
#
# Examples:
#   ./nexus.sh setup --dev --docs
#   ./nexus.sh build --type release --platform stm32f4
#   ./nexus.sh test --filter "*Math*" --verbose
#   ./nexus.sh ci --stage all --coverage

set -e

# Project information
PROJECT_NAME="Nexus Embedded Platform"
PROJECT_VERSION="1.0.0"
PROJECT_DESCRIPTION="Cross-platform embedded development framework"
PROJECT_REPOSITORY="https://github.com/nexus-platform/nexus"
SHELL_VERSION="1.0.0"

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Helper functions
print_header() {
    echo ""
    echo -e "${BOLD}${CYAN}============================================================${NC}"
    printf "${BOLD}${CYAN}%*s${NC}\n" $(((60+${#1})/2)) "$1"
    echo -e "${BOLD}${CYAN}============================================================${NC}"
}

print_success() {
    echo -e "${GREEN}√ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}! $1${NC}"
}

print_error() {
    echo -e "${RED}X $1${NC}"
}

get_project_root() {
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
}

show_version() {
    print_header "Nexus Project Manager"

    echo -e "${CYAN}Project Information:${NC}"
    echo "  Name: $PROJECT_NAME"
    echo "  Version: $PROJECT_VERSION"
    echo "  Description: $PROJECT_DESCRIPTION"
    echo "  Repository: $PROJECT_REPOSITORY"
    echo ""
    echo "Shell Scripts Version: $SHELL_VERSION"
    echo "Shell: $SHELL"
    echo "Platform: $(uname -s) $(uname -m)"
}

show_help() {
    print_header "Nexus Project Manager - Help"

    cat << EOF

Usage: ./nexus.sh <command> [arguments...]

Commands:
  setup     - Environment setup and configuration
  build     - Build the project
  test      - Run tests
  format    - Format source code
  clean     - Clean build artifacts
  docs      - Generate documentation
  ci        - Run CI pipeline
  help      - Show this help information

Options:
  --list    - List all available commands
  --version - Show version information
  --help    - Show this help information

Examples:
  ./nexus.sh setup --dev --docs         # Setup development environment
  ./nexus.sh build --type release       # Build in Release mode
  ./nexus.sh test --verbose             # Run tests with verbose output
  ./nexus.sh format --check             # Check code formatting
  ./nexus.sh clean --all                # Clean all artifacts
  ./nexus.sh docs --target doxygen      # Generate API docs
  ./nexus.sh ci --stage all --coverage  # Run full CI with coverage

For detailed help on a specific command:
  ./nexus.sh <command> --help

For more information, visit: $PROJECT_REPOSITORY
EOF
}

show_command_list() {
    print_header "Available Commands"

    local project_root
    project_root=$(get_project_root)

    # Define commands and their scripts
    declare -A commands=(
        ["setup"]="Environment setup and configuration|scripts/setup/setup.sh"
        ["build"]="Build the project|scripts/building/build.sh"
        ["test"]="Run tests|scripts/test/test.sh"
        ["format"]="Format source code|scripts/tools/format.sh"
        ["clean"]="Clean build artifacts|scripts/tools/clean.sh"
        ["docs"]="Generate documentation|scripts/tools/docs.sh"
        ["ci"]="Run CI pipeline|scripts/ci/ci_build.sh"
    )

    printf "%-12s%-40s%s\n" "Command" "Description" "Script Location"
    echo "--------------------------------------------------------------------------------"

    for cmd in setup build test format clean docs ci; do
        IFS='|' read -r description script_path <<< "${commands[$cmd]}"
        local full_path="$project_root/$script_path"

        if [[ -f "$full_path" ]]; then
            local status="${GREEN}√${NC}"
        else
            local status="${RED}X${NC}"
        fi

        printf "%-12s%-40s%s %s\n" "$cmd" "$description" "$status" "$script_path"
    done

    echo ""
    echo -e "${CYAN}Usage: ./nexus.sh <command> [arguments...]${NC}"
    echo -e "${CYAN}For command-specific help: ./nexus.sh <command> --help${NC}"
}

test_shell_compatibility() {
    # Check if we're running in bash
    if [[ -z "$BASH_VERSION" ]]; then
        print_warning "This script is designed for Bash"
        print_warning "Current shell: $SHELL"
        echo -e "${YELLOW}Consider using Bash for best compatibility${NC}"
        return 1
    fi

    # Check bash version
    local bash_major_version
    bash_major_version=$(echo "$BASH_VERSION" | cut -d. -f1)

    if [[ "$bash_major_version" -lt 4 ]]; then
        print_warning "Bash version $BASH_VERSION detected"
        print_warning "Bash 4.0 or higher is recommended"
        echo -e "${YELLOW}Consider upgrading Bash for best compatibility${NC}"
        return 1
    fi

    return 0
}

test_project_structure() {
    local project_root
    project_root=$(get_project_root)

    # Check for essential directories
    local required_dirs=("scripts" "hal" "osal" "platforms")
    local missing_dirs=()

    for dir in "${required_dirs[@]}"; do
        if [[ ! -d "$project_root/$dir" ]]; then
            missing_dirs+=("$dir")
        fi
    done

    if [[ ${#missing_dirs[@]} -gt 0 ]]; then
        print_warning "Missing project directories: ${missing_dirs[*]}"
        echo -e "${YELLOW}Please ensure you're running this script from the project root${NC}"
        return 1
    fi

    # Check for shell scripts
    declare -A script_map=(
        ["setup"]="scripts/setup/setup.sh"
        ["build"]="scripts/building/build.sh"
        ["test"]="scripts/test/test.sh"
        ["format"]="scripts/tools/format.sh"
        ["clean"]="scripts/tools/clean.sh"
        ["docs"]="scripts/tools/docs.sh"
        ["ci"]="scripts/ci/ci_build.sh"
    )

    local missing_scripts=()
    for script_name in "${!script_map[@]}"; do
        local script_path="${script_map[$script_name]}"
        if [[ ! -f "$project_root/$script_path" ]]; then
            missing_scripts+=("$script_name")
        fi
    done

    if [[ ${#missing_scripts[@]} -gt 0 ]]; then
        print_warning "Missing shell scripts: ${missing_scripts[*]}"
        echo -e "${YELLOW}Some commands may not be available${NC}"
        return 1
    fi

    return 0
}

invoke_command() {
    local command_name="$1"
    shift
    local command_arguments=("$@")

    local project_root
    project_root=$(get_project_root)

    # Map commands to script paths
    declare -A script_map=(
        ["setup"]="scripts/setup/setup.sh"
        ["build"]="scripts/building/build.sh"
        ["test"]="scripts/test/test.sh"
        ["format"]="scripts/tools/format.sh"
        ["clean"]="scripts/tools/clean.sh"
        ["docs"]="scripts/tools/docs.sh"
        ["ci"]="scripts/ci/ci_build.sh"
    )

    if [[ -z "${script_map[$command_name]}" ]]; then
        print_error "Unknown command: $command_name"
        echo -e "${YELLOW}Use './nexus.sh --list' to see available commands${NC}"
        return 1
    fi

    local script_path="$project_root/${script_map[$command_name]}"

    if [[ ! -f "$script_path" ]]; then
        print_error "Script not found: $script_path"
        echo -e "${YELLOW}Please ensure all shell scripts are properly installed${NC}"
        return 1
    fi

    if [[ ! -x "$script_path" ]]; then
        print_warning "Script is not executable, making it executable: $script_path"
        chmod +x "$script_path"
    fi

    echo -e "${BLUE}Executing: $command_name${NC}"
    echo -e "${WHITE}Script: $script_path${NC}"
    if [[ ${#command_arguments[@]} -gt 0 ]]; then
        echo -e "${WHITE}Arguments: ${command_arguments[*]}${NC}"
    fi
    echo ""

    # Execute the script with arguments
    cd "$project_root"
    "$script_path" "${command_arguments[@]}"
    return $?
}

# Parse command line arguments
parse_arguments() {
    local command=""
    local show_list=false
    local show_version=false
    local show_help=false
    local remaining_args=()

    while [[ $# -gt 0 ]]; do
        case $1 in
            --list)
                show_list=true
                shift
                ;;
            --version)
                show_version=true
                shift
                ;;
            --help)
                show_help=true
                shift
                ;;
            setup|build|test|format|clean|docs|ci|help)
                if [[ -z "$command" ]]; then
                    command="$1"
                else
                    remaining_args+=("$1")
                fi
                shift
                ;;
            *)
                remaining_args+=("$1")
                shift
                ;;
        esac
    done

    # Handle special flags first
    if [[ "$show_version" == true ]]; then
        show_version
        return 0
    fi

    if [[ "$show_list" == true ]]; then
        show_command_list
        return 0
    fi

    if [[ "$show_help" == true ]] || [[ "$command" == "help" ]] || [[ -z "$command" ]]; then
        show_help
        return 0
    fi

    # Validate environment
    test_shell_compatibility

    if ! test_project_structure; then
        print_error "Project structure validation failed"
        return 1
    fi

    # Execute command
    invoke_command "$command" "${remaining_args[@]}"
    return $?
}

# Main function
main() {
    # Handle special case where no arguments are provided
    if [[ $# -eq 0 ]]; then
        show_help
        return 0
    fi

    parse_arguments "$@"
    return $?
}

# Trap for cleanup
cleanup() {
    local exit_code=$?
    if [[ $exit_code -eq 130 ]]; then
        echo ""
        echo -e "${YELLOW}Operation cancelled by user${NC}"
    fi
    exit $exit_code
}

trap cleanup INT TERM

# Execute main function
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
