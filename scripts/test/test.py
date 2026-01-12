#!/usr/bin/env python3
"""
Nexus Test Script
Cross-platform test runner using Python.

Usage:
    python test.py [options]

Options:
    --filter, -f    Test filter pattern (default: *)
    --verbose, -v   Verbose output
    --xml           Generate XML report
    --build-dir     Build directory (default: build-Debug)
    --help, -h      Show this help message
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def get_project_root():
    """Get the project root directory."""
    return Path(__file__).parent.parent.parent.resolve()


def find_test_executable(build_dir):
    """Find the test executable."""
    # Windows paths
    win_paths = [
        build_dir / "tests" / "Debug" / "nexus_tests.exe",
        build_dir / "tests" / "Release" / "nexus_tests.exe",
        build_dir / "tests" / "nexus_tests.exe",
    ]
    # Unix paths
    unix_paths = [
        build_dir / "tests" / "nexus_tests",
    ]
    
    for path in win_paths + unix_paths:
        if path.exists():
            return path
    return None


def run_tests(test_exe, filter_pattern, verbose, xml_output):
    """Run the tests."""
    cmd = [str(test_exe)]
    
    if filter_pattern and filter_pattern != "*":
        cmd.append(f"--gtest_filter={filter_pattern}")
    
    cmd.append("--gtest_color=yes")
    
    if not verbose:
        cmd.append("--gtest_brief=1")
    
    if xml_output:
        cmd.append(f"--gtest_output=xml:{xml_output}")
    
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description="Nexus Test Runner")
    parser.add_argument("-f", "--filter", default="*",
                        help="Test filter pattern")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output")
    parser.add_argument("--xml", metavar="FILE",
                        help="Generate XML report")
    parser.add_argument("--build-dir", default="build-Debug",
                        help="Build directory")
    args = parser.parse_args()

    project_root = get_project_root()
    build_dir = project_root / args.build_dir

    print("=" * 50)
    print("Nexus Test Runner")
    print(f"Filter: {args.filter}")
    print("=" * 50)

    # Find test executable
    test_exe = find_test_executable(build_dir)
    if not test_exe:
        print(f"Test executable not found in {build_dir}")
        print("Please run build.py first.")
        return 1

    print(f"Test executable: {test_exe}\n")

    # Run tests
    return_code = run_tests(test_exe, args.filter, args.verbose, args.xml)

    print("\n" + "=" * 50)
    if return_code == 0:
        print("All tests passed!")
    else:
        print("Some tests failed!")
    print("=" * 50)

    return return_code


if __name__ == "__main__":
    sys.exit(main())
