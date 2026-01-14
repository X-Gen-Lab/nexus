#!/usr/bin/env python3
"""
Nexus Code Formatter
Cross-platform code formatting script using clang-format.

Usage:
    python format.py [options]

Options:
    --check, -c     Check only, don't modify files
    --verbose, -v   Verbose output
    --config, -f    Path to format config file (default: .clang-format-dirs)
    --help, -h      Show this help message
"""

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Set, Tuple


def get_project_root() -> Path:
    """Get the project root directory."""
    return Path(__file__).parent.parent.parent.resolve()


def parse_format_config(config_path: Path) -> Tuple[List[str], List[str], List[str]]:
    """
    Parse the format configuration file.
    
    Returns:
        Tuple of (include_dirs, exclude_patterns, extensions)
    """
    include_dirs = []
    exclude_patterns = []
    extensions = [".c", ".h", ".cpp", ".hpp"]  # defaults
    
    if not config_path.exists():
        # Default configuration if no config file exists
        return (
            ["hal", "osal", "platforms", "tests", "applications", "framework"],
            ["ext", "vendors", "build", "build-*", "_build", "docs", "scripts"],
            extensions
        )
    
    in_extensions_section = False
    
    with open(config_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue
            
            # Check for extensions section
            if line == '[extensions]':
                in_extensions_section = True
                extensions = []  # Reset to read from config
                continue
            
            # Handle extensions section
            if in_extensions_section:
                if line.startswith('['):
                    in_extensions_section = False
                elif line.startswith('.'):
                    extensions.append(line)
                continue
            
            # Handle exclusion patterns (lines starting with !)
            if line.startswith('!'):
                exclude_patterns.append(line[1:].strip())
            else:
                include_dirs.append(line)
    
    return include_dirs, exclude_patterns, extensions


def should_exclude(file_path: Path, root: Path, exclude_patterns: List[str]) -> bool:
    """Check if a file should be excluded based on patterns."""
    try:
        rel_path = file_path.relative_to(root)
        rel_str = str(rel_path).replace('\\', '/')
        
        for pattern in exclude_patterns:
            pattern = pattern.replace('\\', '/')
            
            # Handle wildcard patterns
            if pattern.endswith('/*'):
                base_pattern = pattern[:-2]
                if rel_str.startswith(base_pattern + '/') or rel_str == base_pattern:
                    return True
            elif pattern.endswith('*'):
                base_pattern = pattern[:-1]
                if rel_str.startswith(base_pattern):
                    return True
            else:
                # Exact match or directory prefix
                if rel_str.startswith(pattern + '/') or rel_str == pattern:
                    return True
                # Check if any parent directory matches
                parts = rel_path.parts
                if parts and parts[0] == pattern:
                    return True
        
        return False
    except ValueError:
        return False


def find_source_files(root: Path, config_path: Path = None) -> List[Path]:
    """Find all source files to format based on configuration."""
    if config_path is None:
        config_path = root / '.clang-format-dirs'
    
    include_dirs, exclude_patterns, extensions = parse_format_config(config_path)
    
    files = []
    for dir_name in include_dirs:
        dir_path = root / dir_name
        if dir_path.exists() and dir_path.is_dir():
            for ext in extensions:
                for file_path in dir_path.rglob(f"*{ext}"):
                    if not should_exclude(file_path, root, exclude_patterns):
                        files.append(file_path)
    
    return sorted(set(files))


def check_clang_format() -> bool:
    """Check if clang-format is available."""
    try:
        result = subprocess.run(
            ["clang-format", "--version"],
            capture_output=True, 
            check=True
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def get_clang_format_version() -> str:
    """Get clang-format version string."""
    try:
        result = subprocess.run(
            ["clang-format", "--version"],
            capture_output=True,
            text=True
        )
        return result.stdout.strip()
    except Exception:
        return "unknown"


def format_file(file_path: Path, check_only: bool, verbose: bool) -> bool:
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


def print_config_info(root: Path, config_path: Path):
    """Print configuration information."""
    include_dirs, exclude_patterns, extensions = parse_format_config(config_path)
    
    print(f"Configuration file: {config_path}")
    print(f"Include directories: {', '.join(include_dirs)}")
    print(f"Exclude patterns: {', '.join(exclude_patterns)}")
    print(f"File extensions: {', '.join(extensions)}")


def main():
    parser = argparse.ArgumentParser(description="Nexus Code Formatter")
    parser.add_argument("-c", "--check", action="store_true",
                        help="Check only, don't modify files")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose output")
    parser.add_argument("-f", "--config", type=str, default=None,
                        help="Path to format config file (default: .clang-format-dirs)")
    parser.add_argument("--show-config", action="store_true",
                        help="Show configuration and exit")
    args = parser.parse_args()

    project_root = get_project_root()
    config_path = Path(args.config) if args.config else project_root / '.clang-format-dirs'

    print("=" * 60)
    print("Nexus Code Formatter")
    print("=" * 60)

    if args.show_config:
        print_config_info(project_root, config_path)
        return 0

    if not check_clang_format():
        print("ERROR: clang-format not found!")
        print("Please install LLVM/clang-format:")
        print("  Windows: winget install LLVM.LLVM")
        print("  Linux:   sudo apt-get install clang-format")
        print("  macOS:   brew install clang-format")
        return 1

    print(f"clang-format: {get_clang_format_version()}")
    print(f"Mode: {'Check' if args.check else 'Format'}")
    
    if args.verbose:
        print_config_info(project_root, config_path)
    
    print("-" * 60)

    files = find_source_files(project_root, config_path)

    if not files:
        print("No source files found!")
        return 0

    print(f"Found {len(files)} source files to process\n")

    failed_files = []
    formatted_count = 0
    
    for i, file_path in enumerate(files, 1):
        if args.verbose or (i % 20 == 0):
            print(f"Progress: {i}/{len(files)} files processed", end='\r')
        
        if format_file(file_path, args.check, args.verbose):
            formatted_count += 1
        else:
            failed_files.append(file_path)

    print(f"\nProgress: {len(files)}/{len(files)} files processed")
    print("\n" + "=" * 60)
    
    if failed_files:
        if args.check:
            print(f"Format check FAILED for {len(failed_files)} files:")
            for f in failed_files[:10]:
                rel_path = f.relative_to(project_root)
                print(f"  - {rel_path}")
            if len(failed_files) > 10:
                print(f"  ... and {len(failed_files) - 10} more")
            print("\nRun without --check to fix formatting issues.")
        else:
            print(f"Failed to format {len(failed_files)} files")
        return 1
    else:
        if args.check:
            print(f"All {len(files)} files are properly formatted!")
        else:
            print(f"Successfully formatted {formatted_count} files!")
    
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
