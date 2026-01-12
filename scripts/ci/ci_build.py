#!/usr/bin/env python3
"""
Nexus CI Build Script
Continuous Integration build script for automated pipelines.

Usage:
    python ci_build.py [options]

Options:
    --stage         CI stage: build, test, lint, docs, all (default)
    --platform      Target platform: native (default), stm32f4
    --coverage      Enable code coverage
    --help, -h      Show this help message
"""

import argparse
import subprocess
import sys
from pathlib import Path


def get_project_root():
    """Get the project root directory."""
    return Path(__file__).parent.parent.parent.resolve()


def run_command(cmd, cwd=None, env=None):
    """Run a command and return success status."""
    print(f"\n>>> {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    return result.returncode == 0


def stage_build(project_root, platform, coverage):
    """Build stage."""
    print("\n" + "=" * 50)
    print("STAGE: Build")
    print("=" * 50)
    
    build_dir = project_root / "build-ci"
    build_dir.mkdir(exist_ok=True)
    
    cmake_args = [
        "cmake",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DNEXUS_BUILD_TESTS=ON",
        f"-DNEXUS_PLATFORM={platform}",
    ]
    if coverage:
        cmake_args.append("-DNEXUS_COVERAGE=ON")
    cmake_args.append("..")
    
    if not run_command(cmake_args, cwd=build_dir):
        return False
    
    return run_command(["cmake", "--build", ".", "-j4"], cwd=build_dir)


def stage_test(project_root):
    """Test stage."""
    print("\n" + "=" * 50)
    print("STAGE: Test")
    print("=" * 50)
    
    build_dir = project_root / "build-ci"
    
    # Find test executable
    test_exe = None
    for path in [
        build_dir / "tests" / "Debug" / "nexus_tests.exe",
        build_dir / "tests" / "nexus_tests.exe",
        build_dir / "tests" / "nexus_tests",
    ]:
        if path.exists():
            test_exe = path
            break
    
    if not test_exe:
        print("Test executable not found!")
        return False
    
    return run_command([
        str(test_exe),
        "--gtest_output=xml:test_results.xml",
        "--gtest_color=yes"
    ], cwd=project_root)


def stage_lint(project_root):
    """Lint stage (format check)."""
    print("\n" + "=" * 50)
    print("STAGE: Lint")
    print("=" * 50)
    
    format_script = project_root / "scripts" / "tools" / "format.py"
    return run_command([sys.executable, str(format_script), "--check"])


def stage_docs(project_root):
    """Documentation stage."""
    print("\n" + "=" * 50)
    print("STAGE: Documentation")
    print("=" * 50)
    
    docs_script = project_root / "scripts" / "tools" / "docs.py"
    return run_command([sys.executable, str(docs_script), "-t", "doxygen"])


def main():
    parser = argparse.ArgumentParser(description="Nexus CI Build Script")
    parser.add_argument("--stage", choices=["build", "test", "lint", "docs", "all"],
                        default="all", help="CI stage to run")
    parser.add_argument("--platform", default="native",
                        help="Target platform")
    parser.add_argument("--coverage", action="store_true",
                        help="Enable code coverage")
    args = parser.parse_args()

    print("=" * 50)
    print("Nexus CI Build")
    print(f"Stage:    {args.stage}")
    print(f"Platform: {args.platform}")
    print("=" * 50)

    project_root = get_project_root()
    success = True
    
    stages = {
        "build": lambda: stage_build(project_root, args.platform, args.coverage),
        "test": lambda: stage_test(project_root),
        "lint": lambda: stage_lint(project_root),
        "docs": lambda: stage_docs(project_root),
    }
    
    if args.stage == "all":
        for stage_name in ["lint", "build", "test"]:
            if not stages[stage_name]():
                success = False
                break
    else:
        success = stages[args.stage]()

    print("\n" + "=" * 50)
    if success:
        print("CI PASSED")
    else:
        print("CI FAILED")
    print("=" * 50)

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
