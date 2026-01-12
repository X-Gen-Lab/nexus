#!/usr/bin/env python3
"""
Nexus Project Manager - Python Version

Unified entry point for all Nexus project operations.
Provides easy access to build, test, format, clean, docs, and CI operations.

Usage:
    python nexus.py <command> [arguments...]

Commands:
    setup     - Environment setup and configuration
    build     - Build the project
    test      - Run tests
    format    - Format source code
    clean     - Clean build artifacts
    docs      - Generate documentation
    ci        - Run CI pipeline
    help      - Show help information

Examples:
    python nexus.py setup --dev --docs
    python nexus.py build --type release --platform stm32f4
    python nexus.py test --filter "*Math*" --verbose
    python nexus.py ci --stage all --coverage
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


# Project information
PROJECT_INFO = {
    'name': 'Nexus Embedded Platform',
    'version': '1.0.0',
    'description': 'Cross-platform embedded development framework',
    'repository': 'https://github.com/nexus-platform/nexus',
    'python_version': '1.0.0'
}

# Colors for terminal output
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'


def print_header(text):
    """Print a formatted header."""
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{text:^60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.END}")


def print_success(text):
    """Print success message."""
    print(f"{Colors.GREEN}√ {text}{Colors.END}")


def print_warning(text):
    """Print warning message."""
    print(f"{Colors.YELLOW}! {text}{Colors.END}")


def print_error(text):
    """Print error message."""
    print(f"{Colors.RED}X {text}{Colors.END}")


def get_project_root():
    """Get the project root directory."""
    return Path(__file__).parent.parent.resolve()


def show_version():
    """Show version information."""
    print_header("Nexus Project Manager")

    print(f"{Colors.CYAN}Project Information:{Colors.END}")
    print(f"  Name: {PROJECT_INFO['name']}")
    print(f"  Version: {PROJECT_INFO['version']}")
    print(f"  Description: {PROJECT_INFO['description']}")
    print(f"  Repository: {PROJECT_INFO['repository']}")
    print()
    print(f"Python Scripts Version: {PROJECT_INFO['python_version']}")
    print(f"Python Version: {sys.version}")
    print(f"Platform: {sys.platform}")


def show_help():
    """Show help information."""
    print_header("Nexus Project Manager - Help")

    help_text = f"""
Usage: python nexus.py <command> [arguments...]

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
  python nexus.py setup --dev --docs         # Setup development environment
  python nexus.py build --type release       # Build in Release mode
  python nexus.py test --verbose             # Run tests with verbose output
  python nexus.py format --check             # Check code formatting
  python nexus.py clean --all                # Clean all artifacts
  python nexus.py docs --target doxygen      # Generate API docs
  python nexus.py ci --stage all --coverage  # Run full CI with coverage

For detailed help on a specific command:
  python nexus.py <command> --help

For more information, visit: {PROJECT_INFO['repository']}
"""
    print(help_text)


def show_command_list():
    """Show list of available commands."""
    print_header("Available Commands")

    commands = [
        {'name': 'setup', 'description': 'Environment setup and configuration', 'script': 'scripts/setup/setup.py'},
        {'name': 'build', 'description': 'Build the project', 'script': 'scripts/building/build.py'},
        {'name': 'test', 'description': 'Run tests', 'script': 'scripts/test/test.py'},
        {'name': 'format', 'description': 'Format source code', 'script': 'scripts/tools/format.py'},
        {'name': 'clean', 'description': 'Clean build artifacts', 'script': 'scripts/tools/clean.py'},
        {'name': 'docs', 'description': 'Generate documentation', 'script': 'scripts/tools/docs.py'},
        {'name': 'ci', 'description': 'Run CI pipeline', 'script': 'scripts/ci/ci_build.py'},
    ]

    print(f"{'Command':<12}{'Description':<40}{'Script Location'}")
    print("-" * 80)

    project_root = get_project_root()
    for cmd in commands:
        script_path = project_root / cmd['script']
        exists = script_path.exists()
        status = "√" if exists else "X"
        status_color = Colors.GREEN if exists else Colors.RED

        print(f"{cmd['name']:<12}{cmd['description']:<40}{status_color}{status}{Colors.END} {cmd['script']}")

    print()
    print(f"{Colors.CYAN}Usage: python nexus.py <command> [arguments...]{Colors.END}")
    print(f"{Colors.CYAN}For command-specific help: python nexus.py <command> --help{Colors.END}")


def test_python_version():
    """Test if Python version is adequate."""
    required_version = (3, 7)
    current_version = sys.version_info[:2]

    if current_version < required_version:
        print_warning(f"Python version {'.'.join(map(str, current_version))} detected")
        print_warning(f"Python {'.'.join(map(str, required_version))} or higher is recommended")
        print(f"{Colors.YELLOW}Consider upgrading Python for best compatibility{Colors.END}")
        return False

    return True


def test_project_structure():
    """Test if project structure is valid."""
    project_root = get_project_root()

    # Check for essential directories
    required_dirs = ['scripts', 'hal', 'osal', 'platforms']
    missing_dirs = []

    for dir_name in required_dirs:
        dir_path = project_root / dir_name
        if not dir_path.exists():
            missing_dirs.append(dir_name)

    if missing_dirs:
        print_warning(f"Missing project directories: {', '.join(missing_dirs)}")
        print(f"{Colors.YELLOW}Please ensure you're running this script from the project root{Colors.END}")
        return False

    # Check for Python scripts
    script_map = {
        'setup': 'scripts/setup/setup.py',
        'build': 'scripts/building/build.py',
        'test': 'scripts/test/test.py',
        'format': 'scripts/tools/format.py',
        'clean': 'scripts/tools/clean.py',
        'docs': 'scripts/tools/docs.py',
        'ci': 'scripts/ci/ci_build.py',
    }

    missing_scripts = []
    for script_name, script_path in script_map.items():
        full_path = project_root / script_path
        if not full_path.exists():
            missing_scripts.append(script_name)

    if missing_scripts:
        print_warning(f"Missing Python scripts: {', '.join(missing_scripts)}")
        print(f"{Colors.YELLOW}Some commands may not be available{Colors.END}")
        return False

    return True


def invoke_command(command_name, command_arguments):
    """Invoke a specific command script."""
    project_root = get_project_root()

    # Map commands to script paths
    script_map = {
        'setup': 'scripts/setup/setup.py',
        'build': 'scripts/building/build.py',
        'test': 'scripts/test/test.py',
        'format': 'scripts/tools/format.py',
        'clean': 'scripts/tools/clean.py',
        'docs': 'scripts/tools/docs.py',
        'ci': 'scripts/ci/ci_build.py',
    }

    if command_name not in script_map:
        print_error(f"Unknown command: {command_name}")
        print(f"{Colors.YELLOW}Use 'python nexus.py --list' to see available commands{Colors.END}")
        return 1

    script_path = project_root / script_map[command_name]

    if not script_path.exists():
        print_error(f"Script not found: {script_path}")
        print(f"{Colors.YELLOW}Please ensure all Python scripts are properly installed{Colors.END}")
        return 1

    print(f"{Colors.BLUE}Executing: {command_name}{Colors.END}")
    print(f"{Colors.WHITE}Script: {script_path}{Colors.END}")
    if command_arguments:
        print(f"{Colors.WHITE}Arguments: {' '.join(command_arguments)}{Colors.END}")
    print()

    try:
        # Execute the script with arguments
        cmd = [sys.executable, str(script_path)] + command_arguments
        result = subprocess.run(cmd, cwd=project_root)
        return result.returncode
    except Exception as e:
        print_error(f"Failed to execute command '{command_name}': {e}")
        return 1


def main():
    """Main function."""
    parser = argparse.ArgumentParser(
        description="Nexus Project Manager - Python Version",
        add_help=False  # We'll handle help ourselves
    )

    parser.add_argument('command', nargs='?', default='help',
                        choices=['setup', 'build', 'test', 'format', 'clean', 'docs', 'ci', 'help'],
                        help='Command to execute')
    parser.add_argument('--list', action='store_true',
                        help='List all available commands')
    parser.add_argument('--version', action='store_true',
                        help='Show version information')
    parser.add_argument('--help', action='store_true',
                        help='Show help information')

    # Parse known args to handle command-specific arguments
    args, remaining_args = parser.parse_known_args()

    # Handle special flags first
    if args.version:
        show_version()
        return 0

    if args.list:
        show_command_list()
        return 0

    if args.help or args.command == 'help':
        show_help()
        return 0

    # Validate environment
    test_python_version()

    if not test_project_structure():
        print_error("Project structure validation failed")
        return 1

    # Execute command
    return invoke_command(args.command, remaining_args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}Operation cancelled by user{Colors.END}")
        sys.exit(1)
    except Exception as e:
        print_error(f"An error occurred: {e}")
        if '--verbose' in sys.argv:
            import traceback
            print(f"{Colors.RED}Stack trace:{Colors.END}")
            traceback.print_exc()
        sys.exit(1)
