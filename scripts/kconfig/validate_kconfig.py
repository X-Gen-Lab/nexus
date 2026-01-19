#!/usr/bin/env python3
r"""
\file            validate_kconfig.py
\brief           Kconfig validation script
\author          Nexus Team
\version         1.0.0
\date            2026-01-17

\copyright       Copyright (c) 2026 Nexus Team

\details         Validates Kconfig files for syntax errors, dependency issues,
                 and range constraints.
"""

import os
import sys
import argparse
from pathlib import Path
from typing import List, Tuple, Dict, Set, Optional
import re


class KconfigValidator:
    """Kconfig file validator"""

    def __init__(self, root_dir: str = "."):
        self.root_dir = Path(root_dir).resolve()
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self.symbols: Dict[str, Dict] = {}  # symbol_name -> {file, line, type, ...}
        self.dependencies: Dict[str, Set[str]] = {}  # symbol -> set of dependencies
        self.selects: Dict[str, Set[str]] = {}  # symbol -> set of selected symbols

    def validate_file(self, kconfig_path: Path, is_included: bool = False) -> Tuple[List[str], List[str]]:
        r"""
        \brief           Validate a single Kconfig file
        \param[in]       kconfig_path: Path to Kconfig file
        \param[in]       is_included: Whether this file is included via source/rsource
        \return          Tuple of (errors, warnings)
        """
        file_errors = []
        file_warnings = []

        if not kconfig_path.exists():
            file_errors.append(f"File not found: {kconfig_path}")
            return file_errors, file_warnings

        try:
            with open(kconfig_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
        except Exception as e:
            file_errors.append(f"Failed to read {kconfig_path}: {e}")
            return file_errors, file_warnings

        # Validate syntax
        syntax_errors, syntax_warnings = self._validate_syntax(kconfig_path, lines, is_included)
        file_errors.extend(syntax_errors)
        file_warnings.extend(syntax_warnings)

        # Parse symbols and dependencies
        self._parse_symbols(kconfig_path, lines)

        return file_errors, file_warnings

    def _validate_syntax(self, kconfig_path: Path, lines: List[str], is_included: bool = False) -> Tuple[List[str], List[str]]:
        r"""
        \brief           Validate Kconfig syntax
        \param[in]       is_included: Whether this file is included via source/rsource
        \details         Checks for block structure, source paths, and basic syntax
        """
        errors = []
        warnings = []
        stack = []  # Track if/choice/menu blocks
        current_config = None
        in_choice = False  # Track if we're inside a choice block

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Skip empty lines and comments
            if not stripped or stripped.startswith('#'):
                continue

            # Check block start
            if stripped.startswith('if '):
                stack.append(('if', line_num))
                # Don't reset current_config - we might be in a config block with conditions
            elif stripped.startswith('menu '):
                stack.append(('menu', line_num))
                current_config = None  # Reset when entering a new menu
            elif stripped == 'choice' or stripped.startswith('choice'):
                stack.append(('choice', line_num))
                in_choice = True
                current_config = None  # Reset when entering a choice
            elif stripped.startswith('menuconfig '):
                # menuconfig is a config that also opens a menu
                parts = stripped.split()
                if len(parts) >= 2:
                    current_config = parts[1]

            # Check block end
            elif stripped == 'endif':
                if not stack:
                    errors.append(f"{kconfig_path}:{line_num}: Unexpected 'endif'")
                else:
                    block_type, _ = stack.pop()
                    if block_type != 'if':
                        errors.append(f"{kconfig_path}:{line_num}: Expected 'end{block_type}', got 'endif'")

            elif stripped == 'endmenu':
                if not stack:
                    errors.append(f"{kconfig_path}:{line_num}: Unexpected 'endmenu'")
                else:
                    block_type, _ = stack.pop()
                    if block_type != 'menu':
                        errors.append(f"{kconfig_path}:{line_num}: Expected 'end{block_type}', got 'endmenu'")

            elif stripped == 'endchoice':
                if not stack:
                    errors.append(f"{kconfig_path}:{line_num}: Unexpected 'endchoice'")
                else:
                    block_type, _ = stack.pop()
                    if block_type != 'choice':
                        errors.append(f"{kconfig_path}:{line_num}: Expected 'end{block_type}', got 'endchoice'")
                    in_choice = False
            # Check source directives
            elif stripped.startswith('source ') or stripped.startswith('rsource '):
                parts = stripped.split(None, 1)
                if len(parts) < 2:
                    errors.append(f"{kconfig_path}:{line_num}: Invalid source directive")
                    continue

                source_file = parts[1].strip('"\'')

                # For rsource, resolve relative to current file's directory
                if stripped.startswith('rsource '):
                    source_path = kconfig_path.parent / source_file
                else:
                    source_path = self.root_dir / source_file

                # Check if source file exists
                if not source_path.exists():
                    warnings.append(f"{kconfig_path}:{line_num}: Source file not found: {source_file}")

            # Track current config symbol
            elif stripped.startswith('config '):
                parts = stripped.split()
                if len(parts) >= 2:
                    current_config = parts[1]

            # Check for common syntax errors
            elif stripped.startswith('default ') and current_config is None and not in_choice:
                warnings.append(f"{kconfig_path}:{line_num}: 'default' outside of config block")

            elif stripped.startswith('range ') and current_config is None:
                warnings.append(f"{kconfig_path}:{line_num}: 'range' outside of config block")

        # Check for unclosed blocks
        if stack and not is_included:
            # Only report unclosed blocks for root files
            # Included files may have blocks that are closed in the parent file
            for block_type, line_num in stack:
                errors.append(f"{kconfig_path}:{line_num}: Unclosed '{block_type}' block")

        return errors, warnings

    def _parse_symbols(self, kconfig_path: Path, lines: List[str]):
        r"""
        \brief           Parse symbols and dependencies from Kconfig file
        \details         Extracts config symbols, their types, dependencies, and selects
        """
        current_symbol = None
        current_type = None

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Skip empty lines and comments
            if not stripped or stripped.startswith('#'):
                continue

            # Parse config/menuconfig
            if stripped.startswith('config ') or stripped.startswith('menuconfig '):
                parts = stripped.split()
                if len(parts) >= 2:
                    current_symbol = parts[1]
                    if current_symbol not in self.symbols:
                        self.symbols[current_symbol] = {
                            'file': str(kconfig_path),
                            'line': line_num,
                            'type': None,
                            'range': None,
                            'default': None
                        }

            # Parse type
            elif current_symbol and stripped in ['bool', 'tristate', 'string', 'int', 'hex']:
                self.symbols[current_symbol]['type'] = stripped
                current_type = stripped

            elif current_symbol and stripped.startswith('bool '):
                self.symbols[current_symbol]['type'] = 'bool'
                current_type = 'bool'

            elif current_symbol and stripped.startswith('int '):
                self.symbols[current_symbol]['type'] = 'int'
                current_type = 'int'

            elif current_symbol and stripped.startswith('hex '):
                self.symbols[current_symbol]['type'] = 'hex'
                current_type = 'hex'

            elif current_symbol and stripped.startswith('string '):
                self.symbols[current_symbol]['type'] = 'string'
                current_type = 'string'

            # Parse range
            elif current_symbol and stripped.startswith('range '):
                parts = stripped.split()
                if len(parts) >= 3:
                    try:
                        min_val = int(parts[1], 0)  # Support hex with 0x prefix
                        max_val = int(parts[2], 0)
                        self.symbols[current_symbol]['range'] = (min_val, max_val)
                    except ValueError:
                        # Range might reference other symbols
                        self.symbols[current_symbol]['range'] = (parts[1], parts[2])

            # Parse default
            elif current_symbol and stripped.startswith('default '):
                parts = stripped.split(None, 1)
                if len(parts) >= 2:
                    default_value = parts[1].split('#')[0].strip()  # Remove inline comments
                    # Remove 'if' conditions
                    if ' if ' in default_value:
                        default_value = default_value.split(' if ')[0].strip()
                    self.symbols[current_symbol]['default'] = default_value

            # Parse depends on
            elif stripped.startswith('depends on '):
                deps = stripped[11:].strip()  # Remove 'depends on '
                # Parse dependencies (simplified - doesn't handle complex expressions)
                dep_symbols = re.findall(r'\b[A-Z_][A-Z0-9_]*\b', deps)
                if current_symbol:
                    if current_symbol not in self.dependencies:
                        self.dependencies[current_symbol] = set()
                    self.dependencies[current_symbol].update(dep_symbols)

            # Parse select
            elif stripped.startswith('select '):
                parts = stripped.split()
                if len(parts) >= 2:
                    selected = parts[1]
                    if current_symbol:
                        if current_symbol not in self.selects:
                            self.selects[current_symbol] = set()
                        self.selects[current_symbol].add(selected)

    def validate_dependencies(self) -> Tuple[List[str], List[str]]:
        r"""
        \brief           Validate dependencies and selects
        \return          Tuple of (errors, warnings)
        """
        errors = []
        warnings = []

        # Check for undefined symbols in dependencies
        for symbol, deps in self.dependencies.items():
            for dep in deps:
                if dep not in self.symbols and not dep.startswith('!'):
                    warnings.append(f"Symbol '{symbol}' depends on undefined symbol '{dep}'")

        # Check for undefined symbols in selects
        for symbol, sels in self.selects.items():
            for sel in sels:
                if sel not in self.symbols:
                    warnings.append(f"Symbol '{symbol}' selects undefined symbol '{sel}'")

        # Check for circular dependencies
        circular = self._detect_circular_dependencies()
        if circular:
            for cycle in circular:
                errors.append(f"Circular dependency detected: {' -> '.join(cycle)}")

        return errors, warnings

    def _detect_circular_dependencies(self) -> List[List[str]]:
        r"""
        \brief           Detect circular dependencies using DFS
        \return          List of circular dependency chains
        """
        circular = []
        visited = set()
        rec_stack = set()

        def dfs(symbol: str, path: List[str]) -> bool:
            visited.add(symbol)
            rec_stack.add(symbol)
            path.append(symbol)

            # Check dependencies
            if symbol in self.dependencies:
                for dep in self.dependencies[symbol]:
                    if dep.startswith('!'):
                        dep = dep[1:]  # Remove negation

                    if dep not in visited:
                        if dfs(dep, path.copy()):
                            return True
                    elif dep in rec_stack:
                        # Found a cycle
                        cycle_start = path.index(dep)
                        circular.append(path[cycle_start:] + [dep])
                        return True

            rec_stack.remove(symbol)
            return False

        for symbol in self.symbols:
            if symbol not in visited:
                dfs(symbol, [])

        return circular

    def validate_ranges(self) -> Tuple[List[str], List[str]]:
        r"""
        \brief           Validate range constraints and default values
        \return          Tuple of (errors, warnings)
        """
        errors = []
        warnings = []

        for symbol, info in self.symbols.items():
            # Check if default value is within range
            if info.get('range') and info.get('default'):
                range_val = info['range']
                default_val = info['default']

                # Only validate if range and default are numeric
                if isinstance(range_val, tuple) and isinstance(range_val[0], int):
                    try:
                        # Parse default value
                        if default_val.startswith('0x'):
                            default_num = int(default_val, 16)
                        else:
                            default_num = int(default_val)

                        min_val, max_val = range_val
                        if not (min_val <= default_num <= max_val):
                            errors.append(
                                f"Symbol '{symbol}': Default value {default_num} "
                                f"out of range [{min_val}, {max_val}] "
                                f"({info['file']}:{info['line']})"
                            )
                    except ValueError:
                        # Default might be a symbol reference or conditional
                        pass

        return errors, warnings

    def validate_all(self, kconfig_file: Path) -> bool:
        r"""
        \brief           Validate Kconfig file and all included files
        \param[in]       kconfig_file: Root Kconfig file path
        \return          True if validation passed, False otherwise
        """
        # Validate root file
        errors, warnings = self.validate_file(kconfig_file)
        self.errors.extend(errors)
        self.warnings.extend(warnings)

        # Recursively validate included files
        self._validate_included_files(kconfig_file)

        # Validate dependencies
        dep_errors, dep_warnings = self.validate_dependencies()
        self.errors.extend(dep_errors)
        self.warnings.extend(dep_warnings)

        # Validate ranges
        range_errors, range_warnings = self.validate_ranges()
        self.errors.extend(range_errors)
        self.warnings.extend(range_warnings)

        return len(self.errors) == 0

    def _validate_included_files(self, kconfig_path: Path):
        r"""
        \brief           Recursively validate included Kconfig files
        """
        if not kconfig_path.exists():
            return

        try:
            with open(kconfig_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
        except Exception:
            return

        for line in lines:
            stripped = line.strip()
            if stripped.startswith('source ') or stripped.startswith('rsource '):
                parts = stripped.split(None, 1)
                if len(parts) < 2:
                    continue

                source_file = parts[1].strip('"\'')

                # Resolve path
                if stripped.startswith('rsource '):
                    source_path = kconfig_path.parent / source_file
                else:
                    source_path = self.root_dir / source_file

                # Validate included file
                if source_path.exists():
                    errors, warnings = self.validate_file(source_path, is_included=True)
                    self.errors.extend(errors)
                    self.warnings.extend(warnings)

                    # Recursively validate
                    self._validate_included_files(source_path)

    def print_results(self):
        r"""
        \brief           Print validation results
        """
        if self.warnings:
            print("\nWarnings:")
            for warning in self.warnings:
                print(f"  {warning}")

        if self.errors:
            print("\nErrors:")
            for error in self.errors:
                print(f"  {error}")
            print(f"\nValidation failed with {len(self.errors)} error(s)")
        else:
            print("\nValidation passed!")
            if self.warnings:
                print(f"  ({len(self.warnings)} warning(s))")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Validate Kconfig files')
    parser.add_argument('kconfig', nargs='?', default='Kconfig',
                        help='Root Kconfig file (default: Kconfig)')
    parser.add_argument('--root', default='.',
                        help='Root directory for resolving paths (default: .)')
    parser.add_argument('--strict', action='store_true',
                        help='Treat warnings as errors')

    args = parser.parse_args()

    # Create validator
    validator = KconfigValidator(args.root)

    # Validate
    kconfig_path = Path(args.kconfig)
    if not kconfig_path.is_absolute():
        kconfig_path = Path(args.root) / kconfig_path

    success = validator.validate_all(kconfig_path)

    # Print results
    validator.print_results()

    # Exit with appropriate code
    if not success:
        sys.exit(1)
    elif args.strict and validator.warnings:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == '__main__':
    main()
