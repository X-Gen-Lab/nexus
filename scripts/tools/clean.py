#!/usr/bin/env python3
"""
Nexus Clean Script
Cross-platform clean script using Python.

Usage:
    python clean.py [options]

Options:
    --all, -a       Clean all (including docs and test artifacts)
    --verbose, -v   Verbose output
    --help, -h      Show this help message
"""

import argparse
import shutil
import sys
from pathlib import Path


def get_project_root():
    """Get the project root directory."""
    return Path(__file__).parent.parent.parent.resolve()


def remove_dir(path, verbose):
    """Remove a directory if it exists."""
    if path.exists():
        if verbose:
            print(f"Removing: {path}")
        shutil.rmtree(path)
        return True
    return False


def remove_file(path, verbose):
    """Remove a file if it exists."""
    if path.exists():
        if verbose:
            print(f"Removing: {path}")
        path.unlink()
        return True
    return False


def main():
    parser = argparse.ArgumentParser(description="Nexus Clean Script")
    parser.add_argument("-a", "--all", action="store_true",
                        help="Clean all including docs and test artifacts")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output")
    args = parser.parse_args()

    print("=" * 50)
    print("Nexus Clean Script")
    print(f"Mode: {'Full' if args.all else 'Build only'}")
    print("=" * 50)

    project_root = get_project_root()
    removed_count = 0

    # Build directories
    build_dirs = [
        "build", "build-Debug", "build-Release",
        "build-test", "build-check", "build-verify",
        "out", "cmake-build-debug", "cmake-build-release"
    ]
    
    print("\nRemoving build directories...")
    for dir_name in build_dirs:
        if remove_dir(project_root / dir_name, args.verbose):
            removed_count += 1

    if args.all:
        # Documentation
        print("\nRemoving generated documentation...")
        doc_dirs = [
            "docs/api/html", "docs/api/xml", "docs/api/latex",
            "docs/sphinx/_build"
        ]
        for dir_name in doc_dirs:
            if remove_dir(project_root / dir_name, args.verbose):
                removed_count += 1

        # Test artifacts
        print("\nRemoving test artifacts...")
        if remove_dir(project_root / "Testing", args.verbose):
            removed_count += 1
        if remove_file(project_root / "test_results.xml", args.verbose):
            removed_count += 1

    print("\n" + "=" * 50)
    print(f"Clean complete! Removed {removed_count} items.")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    sys.exit(main())
