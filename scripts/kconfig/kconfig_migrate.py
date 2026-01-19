#!/usr/bin/env python3
r"""
\file            kconfig_migrate.py
\brief           Kconfig configuration migration tool
\author          Nexus Team
\version         1.0.0
\date            2026-01-17

\copyright       Copyright (c) 2026 Nexus Team

\details         Migrates Kconfig configuration files between versions,
                 handling symbol renames and deprecated options.
"""

import argparse
import os
import re
import sys
from typing import Dict, List, Tuple, Optional


class ConfigVersion:
    """Configuration version information"""

    def __init__(self, major: int, minor: int, patch: int):
        self.major = major
        self.minor = minor
        self.patch = patch

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}"

    def __eq__(self, other) -> bool:
        return (self.major == other.major and
                self.minor == other.minor and
                self.patch == other.patch)

    def __lt__(self, other) -> bool:
        if self.major != other.major:
            return self.major < other.major
        if self.minor != other.minor:
            return self.minor < other.minor
        return self.patch < other.patch

    def __le__(self, other) -> bool:
        return self == other or self < other

    @staticmethod
    def parse(version_str: str) -> 'ConfigVersion':
        """Parse version string (e.g., '1.0.0')"""
        parts = version_str.split('.')
        if len(parts) != 3:
            raise ValueError(f"Invalid version format: {version_str}")
        return ConfigVersion(int(parts[0]), int(parts[1]), int(parts[2]))


class SymbolMapping:
    """Symbol mapping for configuration migration"""

    def __init__(self):
        self.mappings: Dict[str, str] = {}
        self.deprecated: Dict[str, str] = {}
        self.removed: List[str] = []

    def add_rename(self, old_name: str, new_name: str):
        """Add a symbol rename mapping"""
        self.mappings[old_name] = new_name

    def add_deprecated(self, symbol: str, message: str):
        """Add a deprecated symbol with warning message"""
        self.deprecated[symbol] = message

    def add_removed(self, symbol: str):
        """Add a removed symbol"""
        self.removed.append(symbol)

    def get_new_name(self, old_name: str) -> Optional[str]:
        """Get new name for a symbol, or None if not renamed"""
        return self.mappings.get(old_name)

    def is_deprecated(self, symbol: str) -> bool:
        """Check if symbol is deprecated"""
        return symbol in self.deprecated

    def is_removed(self, symbol: str) -> bool:
        """Check if symbol is removed"""
        return symbol in self.removed

    def get_deprecation_message(self, symbol: str) -> Optional[str]:
        """Get deprecation message for a symbol"""
        return self.deprecated.get(symbol)


class ConfigMigrator:
    """Configuration file migrator"""

    # Version history with migration mappings
    VERSION_MAPPINGS = {
        ('1.0.0', '1.1.0'): {
            'renames': {
                'CONFIG_HAL_UART_ENABLE': 'CONFIG_UART_ENABLE',
                'CONFIG_HAL_SPI_ENABLE': 'CONFIG_SPI_ENABLE',
                'CONFIG_HAL_I2C_ENABLE': 'CONFIG_I2C_ENABLE',
            },
            'deprecated': {
                'CONFIG_OLD_FEATURE': 'This feature is deprecated, use CONFIG_NEW_FEATURE instead',
            },
            'removed': [
                'CONFIG_OBSOLETE_OPTION',
            ],
        },
        ('1.1.0', '1.2.0'): {
            'renames': {
                'CONFIG_PLATFORM_PC': 'CONFIG_PLATFORM_NATIVE',
            },
            'deprecated': {},
            'removed': [],
        },
    }

    def __init__(self):
        self.warnings: List[str] = []
        self.errors: List[str] = []

    def detect_version(self, config_path: str) -> Optional[ConfigVersion]:
        """Detect configuration file version"""
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                for line in f:
                    # Look for version comment
                    match = re.match(r'#\s*Version:\s*(\d+\.\d+\.\d+)', line)
                    if match:
                        return ConfigVersion.parse(match.group(1))

                    # Look for version config
                    match = re.match(r'CONFIG_VERSION="(\d+\.\d+\.\d+)"', line)
                    if match:
                        return ConfigVersion.parse(match.group(1))
        except FileNotFoundError:
            self.errors.append(f"Config file not found: {config_path}")
            return None
        except Exception as e:
            self.errors.append(f"Error detecting version: {e}")
            return None

        # Default to oldest version if not specified
        return ConfigVersion(1, 0, 0)

    def build_mapping(self, from_version: ConfigVersion,
                     to_version: ConfigVersion) -> SymbolMapping:
        """Build symbol mapping for version migration"""
        mapping = SymbolMapping()

        # Apply all mappings between versions
        current = from_version
        for (v_from, v_to), changes in self.VERSION_MAPPINGS.items():
            v_from_obj = ConfigVersion.parse(v_from)
            v_to_obj = ConfigVersion.parse(v_to)

            # Apply if this mapping is in our migration path
            if current <= v_from_obj and v_to_obj <= to_version:
                for old, new in changes.get('renames', {}).items():
                    mapping.add_rename(old, new)

                for symbol, msg in changes.get('deprecated', {}).items():
                    mapping.add_deprecated(symbol, msg)

                for symbol in changes.get('removed', []):
                    mapping.add_removed(symbol)

        return mapping

    def migrate_config(self, input_path: str, output_path: str,
                      target_version: ConfigVersion) -> bool:
        """Migrate configuration file to target version"""
        # Detect current version
        current_version = self.detect_version(input_path)
        if current_version is None:
            return False

        print(f"Current version: {current_version}")
        print(f"Target version: {target_version}")

        if current_version == target_version:
            print("Configuration is already at target version")
            return True

        if current_version > target_version:
            self.errors.append("Cannot downgrade configuration version")
            return False

        # Build migration mapping
        mapping = self.build_mapping(current_version, target_version)

        # Read input config
        try:
            with open(input_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
        except FileNotFoundError:
            self.errors.append(f"Input file not found: {input_path}")
            return False

        # Migrate configuration
        migrated_lines = []
        migrated_lines.append(f"# Version: {target_version}\n")
        migrated_lines.append(f"# Migrated from version {current_version}\n")
        migrated_lines.append("#\n")

        for line in lines:
            # Skip old version comments
            if re.match(r'#\s*Version:', line):
                continue

            # Process config lines
            match = re.match(r'(CONFIG_\w+)(=.+)?', line.strip())
            if match:
                symbol = match.group(1)
                value = match.group(2) or ''

                # Check if removed
                if mapping.is_removed(symbol):
                    migrated_lines.append(f"# {symbol} has been removed\n")
                    self.warnings.append(f"Removed symbol: {symbol}")
                    continue

                # Check if deprecated
                if mapping.is_deprecated(symbol):
                    msg = mapping.get_deprecation_message(symbol)
                    migrated_lines.append(f"# WARNING: {symbol} is deprecated - {msg}\n")
                    migrated_lines.append(line)
                    self.warnings.append(f"Deprecated symbol: {symbol} - {msg}")
                    continue

                # Check if renamed
                new_name = mapping.get_new_name(symbol)
                if new_name:
                    migrated_lines.append(f"# {symbol} renamed to {new_name}\n")
                    migrated_lines.append(f"{new_name}{value}\n")
                    self.warnings.append(f"Renamed: {symbol} -> {new_name}")
                    continue

                # Keep unchanged
                migrated_lines.append(line)
            else:
                # Keep comments and other lines
                migrated_lines.append(line)

        # Write output config
        try:
            os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
            with open(output_path, 'w', encoding='utf-8') as f:
                f.writelines(migrated_lines)
        except Exception as e:
            self.errors.append(f"Error writing output file: {e}")
            return False

        print(f"\nMigration complete: {output_path}")

        if self.warnings:
            print(f"\nWarnings ({len(self.warnings)}):")
            for warning in self.warnings:
                print(f"  - {warning}")

        return True

    def print_errors(self):
        """Print all errors"""
        if self.errors:
            print("\nErrors:", file=sys.stderr)
            for error in self.errors:
                print(f"  - {error}", file=sys.stderr)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Migrate Kconfig configuration files between versions'
    )
    parser.add_argument(
        'input',
        help='Input configuration file'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output configuration file (default: input file with .migrated suffix)'
    )
    parser.add_argument(
        '-t', '--target-version',
        default='1.2.0',
        help='Target version (default: 1.2.0)'
    )
    parser.add_argument(
        '-f', '--force',
        action='store_true',
        help='Overwrite output file if it exists'
    )

    args = parser.parse_args()

    # Determine output path
    if args.output:
        output_path = args.output
    else:
        base, ext = os.path.splitext(args.input)
        output_path = f"{base}.migrated{ext}"

    # Check if output exists
    if os.path.exists(output_path) and not args.force:
        print(f"Error: Output file exists: {output_path}", file=sys.stderr)
        print("Use -f/--force to overwrite", file=sys.stderr)
        return 1

    # Parse target version
    try:
        target_version = ConfigVersion.parse(args.target_version)
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    # Migrate configuration
    migrator = ConfigMigrator()
    success = migrator.migrate_config(args.input, output_path, target_version)

    if not success:
        migrator.print_errors()
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
