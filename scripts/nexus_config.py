#!/usr/bin/env python3
r"""
\file            nexus_config.py
\brief           Nexus configuration management command-line tool
\author          Nexus Team
\version         1.0.0
\date            2026-01-17

\copyright       Copyright (c) 2026 Nexus Team

\details         Unified command-line interface for Nexus Kconfig configuration
                 management, integrating generation, validation, migration, and
                 comparison tools.
"""

import argparse
import os
import subprocess
import sys
from typing import List, Optional


class NexusConfig:
    """Nexus configuration management tool"""

    def __init__(self):
        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.project_root = os.path.dirname(self.script_dir)

    def run_script(self, script_name: str, args: List[str]) -> int:
        """Run a Python script with arguments"""
        # Check if script is in kconfig subdirectory
        if script_name in ['generate_config.py', 'validate_kconfig.py',
                          'kconfig_migrate.py', 'kconfig_diff.py',
                          'generate_config_docs.py']:
            script_path = os.path.join(self.script_dir, 'kconfig', script_name)
        else:
            script_path = os.path.join(self.script_dir, script_name)

        if not os.path.exists(script_path):
            print(f"Error: Script not found: {script_path}", file=sys.stderr)
            return 1

        try:
            result = subprocess.run(
                [sys.executable, script_path] + args,
                cwd=self.project_root
            )
            return result.returncode
        except Exception as e:
            print(f"Error running script: {e}", file=sys.stderr)
            return 1

    def generate(self, args: argparse.Namespace) -> int:
        """Generate configuration header file"""
        script_args = []

        if args.config:
            script_args.extend(['--config', args.config])

        if args.output:
            script_args.extend(['--output', args.output])

        if args.default:
            script_args.append('--default')

        if args.platform:
            script_args.extend(['--platform', args.platform])

        print("Generating configuration header...")
        return self.run_script('generate_config.py', script_args)

    def validate(self, args: argparse.Namespace) -> int:
        """Validate Kconfig files"""
        script_args = []

        # Add kconfig file as positional argument
        if hasattr(args, 'kconfig') and args.kconfig:
            script_args.append(args.kconfig)

        print("Validating Kconfig configuration...")
        return self.run_script('validate_kconfig.py', script_args)

    def migrate(self, args: argparse.Namespace) -> int:
        """Migrate configuration file"""
        script_args = [args.input]

        if args.output:
            script_args.extend(['--output', args.output])

        if args.target_version:
            script_args.extend(['--target-version', args.target_version])

        if args.force:
            script_args.append('--force')

        print("Migrating configuration...")
        return self.run_script('kconfig_migrate.py', script_args)

    def diff(self, args: argparse.Namespace) -> int:
        """Compare configuration files"""
        script_args = [args.old_config, args.new_config]

        if args.output:
            script_args.extend(['--output', args.output])

        if args.format:
            script_args.extend(['--format', args.format])

        print("Comparing configurations...")
        return self.run_script('kconfig_diff.py', script_args)

    def info(self, args: argparse.Namespace) -> int:
        """Display configuration information"""
        print("=" * 80)
        print("Nexus Configuration Management Tool")
        print("=" * 80)
        print()
        print(f"Project root: {self.project_root}")
        print(f"Script directory: {self.script_dir}")
        print()

        # Check for configuration files
        config_file = os.path.join(self.project_root, '.config')
        if os.path.exists(config_file):
            print(f"Configuration file: {config_file}")
            print(f"  Size: {os.path.getsize(config_file)} bytes")
        else:
            print("Configuration file: Not found")

        # Check for generated header
        header_file = os.path.join(self.project_root, 'hal', 'include', 'hal',
                                   'nexus_config.h')
        if os.path.exists(header_file):
            print(f"Generated header: {header_file}")
            print(f"  Size: {os.path.getsize(header_file)} bytes")
        else:
            print("Generated header: Not found")

        # Check for Kconfig files
        kconfig_file = os.path.join(self.project_root, 'Kconfig')
        if os.path.exists(kconfig_file):
            print(f"Root Kconfig: {kconfig_file}")
        else:
            print("Root Kconfig: Not found")

        print()
        print("Available commands:")
        print("  generate  - Generate configuration header file")
        print("  validate  - Validate Kconfig files")
        print("  migrate   - Migrate configuration to new version")
        print("  diff      - Compare configuration files")
        print("  info      - Display configuration information")
        print()

        return 0


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Nexus configuration management tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate configuration header
  %(prog)s generate
  %(prog)s generate --config .config --output nexus_config.h

  # Validate Kconfig files
  %(prog)s validate
  %(prog)s validate --check-deps --check-ranges

  # Migrate configuration
  %(prog)s migrate old.config --target-version 1.2.0

  # Compare configurations
  %(prog)s diff old.config new.config
  %(prog)s diff old.config new.config --format html --output report.html

  # Display information
  %(prog)s info
        """
    )

    subparsers = parser.add_subparsers(dest='command', help='Command to execute')

    # Generate command
    generate_parser = subparsers.add_parser(
        'generate',
        help='Generate configuration header file'
    )
    generate_parser.add_argument(
        '-c', '--config',
        help='Input configuration file (default: .config)'
    )
    generate_parser.add_argument(
        '-o', '--output',
        help='Output header file (default: nexus_config.h)'
    )
    generate_parser.add_argument(
        '-d', '--default',
        action='store_true',
        help='Generate default configuration'
    )
    generate_parser.add_argument(
        '-p', '--platform',
        help='Platform name for default configuration'
    )

    # Validate command
    validate_parser = subparsers.add_parser(
        'validate',
        help='Validate Kconfig files'
    )
    validate_parser.add_argument(
        'kconfig',
        nargs='?',
        default='Kconfig',
        help='Root Kconfig file (default: Kconfig)'
    )

    # Migrate command
    migrate_parser = subparsers.add_parser(
        'migrate',
        help='Migrate configuration file'
    )
    migrate_parser.add_argument(
        'input',
        help='Input configuration file'
    )
    migrate_parser.add_argument(
        '-o', '--output',
        help='Output configuration file'
    )
    migrate_parser.add_argument(
        '-t', '--target-version',
        help='Target version (default: 1.2.0)'
    )
    migrate_parser.add_argument(
        '-f', '--force',
        action='store_true',
        help='Overwrite output file if exists'
    )

    # Diff command
    diff_parser = subparsers.add_parser(
        'diff',
        help='Compare configuration files'
    )
    diff_parser.add_argument(
        'old_config',
        help='Old configuration file'
    )
    diff_parser.add_argument(
        'new_config',
        help='New configuration file'
    )
    diff_parser.add_argument(
        '-o', '--output',
        help='Output report file'
    )
    diff_parser.add_argument(
        '-f', '--format',
        choices=['text', 'html'],
        default='text',
        help='Report format (default: text)'
    )

    # Info command
    info_parser = subparsers.add_parser(
        'info',
        help='Display configuration information'
    )

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1

    # Execute command
    tool = NexusConfig()

    if args.command == 'generate':
        return tool.generate(args)
    elif args.command == 'validate':
        return tool.validate(args)
    elif args.command == 'migrate':
        return tool.migrate(args)
    elif args.command == 'diff':
        return tool.diff(args)
    elif args.command == 'info':
        return tool.info(args)
    else:
        print(f"Error: Unknown command: {args.command}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
