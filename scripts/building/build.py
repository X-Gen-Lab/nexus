#!/usr/bin/env python3
"""
Nexus Build Script
Cross-platform build script using Python.

Usage:
    python build.py [options]

Options:
    --type, -t      Build type: debug (default) or release
    --clean, -c     Clean build directory before building
    --jobs, -j      Number of parallel jobs (default: auto)
    --platform, -p  Target platform: native (default), stm32f4
    --help, -h      Show this help message
"""

import argparse
import os
import subprocess
import sys
import shutil
from pathlib import Path


def get_project_root():
    """Get the project root directory."""
    return Path(__file__).parent.parent.parent.resolve()


def get_cpu_count():
    """Get the number of CPUs for parallel builds."""
    try:
        return os.cpu_count() or 4
    except:
        return 4


def run_command(cmd, cwd=None):
    """Run a command and return the result."""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode == 0


def configure(build_dir, build_type, platform):
    """Configure the build with CMake."""
    cmake_args = [
        "cmake",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DNEXUS_BUILD_TESTS=ON",
        f"-DNEXUS_PLATFORM={platform}",
        ".."
    ]
    return run_command(cmake_args, cwd=build_dir)


def build(build_dir, build_type, jobs):
    """Build the project."""
    cmake_args = [
        "cmake",
        "--build", ".",
        "--config", build_type,
        "-j", str(jobs)
    ]
    return run_command(cmake_args, cwd=build_dir)


def main():
    parser = argparse.ArgumentParser(description="Nexus Build Script")
    parser.add_argument("-t", "--type", choices=["debug", "release"],
                        default="debug", help="Build type")
    parser.add_argument("-c", "--clean", action="store_true",
                        help="Clean build before building")
    parser.add_argument("-j", "--jobs", type=int, default=get_cpu_count(),
                        help="Number of parallel jobs")
    parser.add_argument("-p", "--platform", default="native",
                        help="Target platform")
    args = parser.parse_args()

    project_root = get_project_root()
    build_type = args.type.capitalize()
    build_dir = project_root / f"build-{build_type}"

    print("=" * 50)
    print("Nexus Build Script")
    print(f"Build Type: {build_type}")
    print(f"Platform:   {args.platform}")
    print(f"Build Dir:  {build_dir}")
    print(f"Jobs:       {args.jobs}")
    print("=" * 50)

    # Clean if requested
    if args.clean and build_dir.exists():
        print("Cleaning build directory...")
        shutil.rmtree(build_dir)

    # Create build directory
    build_dir.mkdir(parents=True, exist_ok=True)

    # Configure
    print("\nConfiguring CMake...")
    if not configure(build_dir, build_type, args.platform):
        print("Configuration failed!")
        return 1

    # Build
    print("\nBuilding...")
    if not build(build_dir, build_type, args.jobs):
        print("Build failed!")
        return 1

    print("\n" + "=" * 50)
    print("Build completed successfully!")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    sys.exit(main())
