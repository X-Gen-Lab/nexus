#!/usr/bin/env python3
"""
Nexus Build Script
Cross-platform build script using CMake presets.

Usage:
    python build.py [options]

Options:
    --preset, -p    CMake preset name (e.g., windows-msvc-debug, linux-gcc-release)
    --list, -l      List available CMake presets
    --clean, -c     Clean build directory before building
    --jobs, -j      Number of parallel jobs (default: auto)
    --test, -t      Run tests after building
    --help, -h      Show this help message

Examples:
    python build.py --preset windows-msvc-debug
    python build.py --preset linux-gcc-release --test
    python build.py --list
"""

import argparse
import json
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


def load_presets():
    """Load CMake presets from CMakePresets.json."""
    project_root = get_project_root()
    presets_file = project_root / "CMakePresets.json"

    if not presets_file.exists():
        print(f"Error: CMakePresets.json not found at {presets_file}")
        return None

    try:
        with open(presets_file, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading CMakePresets.json: {e}")
        return None


def list_presets():
    """List all available CMake presets."""
    presets_data = load_presets()
    if not presets_data:
        return False

    print("\n=== Available CMake Presets ===\n")

    # List configure presets
    print("Configure Presets:")
    for preset in presets_data.get("configurePresets", []):
        if preset.get("hidden"):
            continue
        name = preset.get("name", "")
        display_name = preset.get("displayName", name)
        description = preset.get("description", "")
        print(f"  {name:30s} - {display_name}")
        if description:
            print(f"    {description}")

    # List build presets
    print("\nBuild Presets:")
    for preset in presets_data.get("buildPresets", []):
        name = preset.get("name", "")
        config_preset = preset.get("configurePreset", "")
        print(f"  {name:30s} (uses: {config_preset})")

    # List test presets
    print("\nTest Presets:")
    for preset in presets_data.get("testPresets", []):
        if preset.get("hidden"):
            continue
        name = preset.get("name", "")
        config_preset = preset.get("configurePreset", "")
        print(f"  {name:30s} (uses: {config_preset})")

    print("\n" + "=" * 50 + "\n")
    return True


def get_default_preset():
    """Get the default preset based on the current platform."""
    import platform
    system = platform.system()

    if system == "Windows":
        return "windows-msvc-debug"
    elif system == "Darwin":
        return "macos-clang-debug"
    else:
        return "linux-gcc-debug"


def configure(preset):
    """Configure the build with CMake preset."""
    project_root = get_project_root()
    cmake_args = [
        "cmake",
        "--preset", preset
    ]
    return run_command(cmake_args, cwd=project_root)


def build(preset, jobs):
    """Build the project using CMake preset."""
    project_root = get_project_root()
    cmake_args = [
        "cmake",
        "--build",
        "--preset", preset,
        "-j", str(jobs)
    ]
    return run_command(cmake_args, cwd=project_root)


def test(preset):
    """Run tests using CMake preset."""
    project_root = get_project_root()
    cmake_args = [
        "ctest",
        "--preset", preset,
        "--output-on-failure"
    ]
    return run_command(cmake_args, cwd=project_root)


def main():
    parser = argparse.ArgumentParser(
        description="Nexus Build Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py --preset windows-msvc-debug
  python build.py --preset linux-gcc-release --test
  python build.py --list
        """
    )
    parser.add_argument("-p", "--preset",
                        help="CMake preset name")
    parser.add_argument("-l", "--list", action="store_true",
                        help="List available CMake presets")
    parser.add_argument("-c", "--clean", action="store_true",
                        help="Clean build before building")
    parser.add_argument("-j", "--jobs", type=int, default=get_cpu_count(),
                        help="Number of parallel jobs")
    parser.add_argument("-t", "--test", action="store_true",
                        help="Run tests after building")
    args = parser.parse_args()

    # List presets if requested
    if args.list:
        return 0 if list_presets() else 1

    # Get preset name
    preset = args.preset
    if not preset:
        preset = get_default_preset()
        print(f"No preset specified, using default: {preset}")

    project_root = get_project_root()

    # Determine build directory from preset
    build_dir = project_root / "build" / preset

    print("=" * 60)
    print("Nexus Build Script")
    print(f"Preset:     {preset}")
    print(f"Build Dir:  {build_dir}")
    print(f"Jobs:       {args.jobs}")
    print(f"Run Tests:  {args.test}")
    print("=" * 60)

    # Clean if requested
    if args.clean and build_dir.exists():
        print("\nCleaning build directory...")
        shutil.rmtree(build_dir)

    # Configure
    print("\nConfiguring with CMake preset...")
    if not configure(preset):
        print("Configuration failed!")
        return 1

    # Build
    print("\nBuilding...")
    if not build(preset, args.jobs):
        print("Build failed!")
        return 1

    # Test if requested
    if args.test:
        print("\nRunning tests...")
        if not test(preset):
            print("Some tests failed!")
            return 1

    print("\n" + "=" * 60)
    print("Build completed successfully!")
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
