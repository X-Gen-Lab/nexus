#!/usr/bin/env python3
"""
Nexus Code Formatter
Cross-platform code formatting script using clang-format.

Usage:
    python format.py [options]

Options:
    --check, -c     Check only, don't modify files
    --verbose, -v   Verbose output
    --help, -h      Show this help message
"""

import argparse
import subprocess
import sys
from pathlib import Path


def get_project_root():
    """Get the project root directory."""
    return Path(__file__).parent.parent.parent.resolve()


def find_source_files(root):
    """Find all source files to format."""
    extensions = [".c", ".h", ".cpp", ".hpp"]
    directories = ["hal", "osal", "platforms", "tests", "applications"]
    
    files = []
    for dir_name in directories:
        dir_path = root / dir_name
        if dir_path.exists():
            for ext in extensions:
                files.extend(dir_path.rglob(f"*{ext}"))
    return files


def check_clang_format():
    """Check if clang-format is available."""
    try:
        subprocess.run(["clang-format", "--version"],
                      capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def format_file(file_path, check_only, verbose):
    """Format a single file."""
    if verbose:
        print(f"Processing: {file_path}")
    
    if check_only:
        result = subprocess.run(
            ["clang-format", "--dry-run", "--Werror", str(file_path)],
            capture_output=True
        )
        return result.returncode == 0
    else:
        result = subprocess.run(
            ["clang-format", "-i", str(file_path)],
            capture_output=True
        )
        return result.returncode == 0


def main():
    parser = argparse.ArgumentParser(description="Nexus Code Formatter")
    parser.add_argument("-c", "--check", action="store_true",
                        help="Check only, don't modify files")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output")
    args = parser.parse_args()

    print("=" * 50)
    print("Nexus Code Formatter")
    print(f"Mode: {'Check' if args.check else 'Format'}")
    print("=" * 50)

    if not check_clang_format():
        print("clang-format not found! Please install LLVM.")
        return 1

    project_root = get_project_root()
    files = find_source_files(project_root)

    if not files:
        print("No source files found!")
        return 0

    print(f"Found {len(files)} files\n")

    failed_files = []
    for file_path in files:
        if not format_file(file_path, args.check, args.verbose):
            failed_files.append(file_path)

    print("\n" + "=" * 50)
    if failed_files:
        if args.check:
            print(f"Format check failed for {len(failed_files)} files:")
            for f in failed_files[:10]:
                print(f"  - {f}")
            if len(failed_files) > 10:
                print(f"  ... and {len(failed_files) - 10} more")
        else:
            print(f"Failed to format {len(failed_files)} files")
        return 1
    else:
        if args.check:
            print("All files are properly formatted!")
        else:
            print("Code formatting complete!")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    sys.exit(main())
