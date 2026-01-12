#!/usr/bin/env python3
"""
Nexus Documentation Generator
Cross-platform documentation generation script.

Usage:
    python docs.py [options]

Options:
    --target, -t    Target: doxygen, sphinx, or all (default)
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


def check_command(cmd):
    """Check if a command is available."""
    try:
        subprocess.run([cmd, "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def generate_doxygen(project_root, verbose):
    """Generate Doxygen documentation."""
    print("Generating Doxygen documentation...")
    
    if not check_command("doxygen"):
        print("Doxygen not found! Please install Doxygen.")
        return False
    
    cmd = ["doxygen", "Doxyfile"]
    if verbose:
        result = subprocess.run(cmd, cwd=project_root)
    else:
        result = subprocess.run(cmd, cwd=project_root, capture_output=True)
    
    if result.returncode == 0:
        print("Doxygen documentation generated in docs/api/html")
        return True
    else:
        print("Doxygen generation failed!")
        return False


def generate_sphinx(project_root, verbose):
    """Generate Sphinx documentation."""
    print("Generating Sphinx documentation...")
    
    if not check_command("sphinx-build"):
        print("Sphinx not found! Please install Sphinx.")
        return False
    
    sphinx_dir = project_root / "docs" / "sphinx"
    output_dir = sphinx_dir / "_build" / "html"
    
    cmd = ["sphinx-build", "-b", "html", ".", "_build/html"]
    if verbose:
        result = subprocess.run(cmd, cwd=sphinx_dir)
    else:
        result = subprocess.run(cmd, cwd=sphinx_dir, capture_output=True)
    
    if result.returncode == 0:
        print(f"Sphinx documentation generated in {output_dir}")
        return True
    else:
        print("Sphinx generation failed!")
        return False


def main():
    parser = argparse.ArgumentParser(description="Nexus Documentation Generator")
    parser.add_argument("-t", "--target", choices=["doxygen", "sphinx", "all"],
                        default="all", help="Documentation target")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output")
    args = parser.parse_args()

    print("=" * 50)
    print("Nexus Documentation Generator")
    print(f"Target: {args.target}")
    print("=" * 50)

    project_root = get_project_root()
    success = True

    if args.target in ["doxygen", "all"]:
        if not generate_doxygen(project_root, args.verbose):
            success = False

    if args.target in ["sphinx", "all"]:
        if not generate_sphinx(project_root, args.verbose):
            success = False

    print("\n" + "=" * 50)
    if success:
        print("Documentation generation complete!")
    else:
        print("Documentation generation had errors!")
    print("=" * 50)

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
