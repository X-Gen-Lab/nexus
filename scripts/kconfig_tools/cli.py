r"""
\file            cli.py
\brief           Command-line interface for Kconfig tools
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Provides command-line interface for generating and validating
                 Kconfig files according to Nexus naming standards.
"""

import argparse
import sys
import json
import yaml
from pathlib import Path
from typing import Optional

from .naming_rules import NamingRules
from .generator import KconfigGenerator, PeripheralTemplate, ParameterConfig, ChoiceConfig
from .validator import KconfigValidator, ValidationIssue
from .templates import get_template, list_templates


def cmd_generate(args):
    r"""
    \brief           Handle generate command
    \param[in]       args: Command-line arguments
    """
    try:
        # Get peripheral template
        template = get_template(args.peripheral)

        # Override platform if specified
        if args.platform != "NATIVE":
            template.platform = args.platform.upper()

        # Override max_instances if specified
        if args.instances > template.max_instances:
            print(
                f"Warning: Requested {args.instances} instances, "
                f"but template maximum is {template.max_instances}. "
                f"Using {template.max_instances} instances.",
                file=sys.stderr
            )
            instances = template.max_instances
        else:
            instances = args.instances

        # Update template max_instances
        template.max_instances = instances

        # Create generator
        generator = KconfigGenerator(template)

        # Determine output path
        if args.output:
            output_path = args.output
        else:
            peripheral_lower = template.name.lower()
            output_path = f"Kconfig_{peripheral_lower}"

        # Generate file
        generator.generate_file(output_path)

        print(f"Successfully generated Kconfig file: {output_path}")
        print(f"  Peripheral: {template.name}")
        print(f"  Platform: {template.platform}")
        print(f"  Instances: {instances}")

    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        print(f"\nAvailable peripheral templates: {', '.join(list_templates())}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error generating Kconfig file: {e}", file=sys.stderr)
        sys.exit(1)


def cmd_validate(args):
    r"""
    \brief           Handle validate command
    \param[in]       args: Command-line arguments
    """
    try:
        path = Path(args.path)

        # Check if path exists
        if not path.exists():
            print(f"Error: Path does not exist: {path}", file=sys.stderr)
            sys.exit(1)

        # Create validator
        validator = KconfigValidator()

        # Validate file or directory
        if path.is_file():
            print(f"Validating file: {path}")
            issues = validator.validate_file(str(path))
            results = {str(path): issues}
        elif path.is_dir():
            print(f"Validating directory: {path}")
            results = validator.validate_directory(str(path))
        else:
            print(f"Error: Path is neither a file nor a directory: {path}", file=sys.stderr)
            sys.exit(1)

        # Generate report
        report = validator.generate_report(results)

        # Output report
        if args.report:
            report_path = Path(args.report)
            report_path.write_text(report, encoding='utf-8')
            print(f"\nValidation report written to: {args.report}")
        else:
            print("\n" + report)

        # Count issues by severity
        total_errors = 0
        total_warnings = 0
        total_info = 0

        for file_issues in results.values():
            for issue in file_issues:
                if issue.severity == "error":
                    total_errors += 1
                elif issue.severity == "warning":
                    total_warnings += 1
                elif issue.severity == "info":
                    total_info += 1

        # Print summary
        print(f"\nValidation Summary:")
        print(f"  Files checked: {len(results)}")
        print(f"  Errors: {total_errors}")
        print(f"  Warnings: {total_warnings}")
        print(f"  Info: {total_info}")

        # Exit with error code if there are errors
        if total_errors > 0:
            sys.exit(1)

    except Exception as e:
        print(f"Error validating Kconfig file(s): {e}", file=sys.stderr)
        sys.exit(1)


def cmd_batch_generate(args):
    r"""
    \brief           Handle batch-generate command
    \param[in]       args: Command-line arguments
    """
    try:
        config_path = Path(args.config)

        # Check if config file exists
        if not config_path.exists():
            print(f"Error: Configuration file does not exist: {config_path}", file=sys.stderr)
            sys.exit(1)

        # Load configuration file
        config_content = config_path.read_text(encoding='utf-8')

        if config_path.suffix.lower() in ['.yaml', '.yml']:
            try:
                config = yaml.safe_load(config_content)
            except yaml.YAMLError as e:
                print(f"Error parsing YAML configuration: {e}", file=sys.stderr)
                sys.exit(1)
        elif config_path.suffix.lower() == '.json':
            try:
                config = json.loads(config_content)
            except json.JSONDecodeError as e:
                print(f"Error parsing JSON configuration: {e}", file=sys.stderr)
                sys.exit(1)
        else:
            print(f"Error: Unsupported configuration file format. Use .yaml, .yml, or .json", file=sys.stderr)
            sys.exit(1)

        # Determine output directory
        if args.output_dir:
            output_dir = Path(args.output_dir)
        else:
            output_dir = Path.cwd()

        # Create output directory if it doesn't exist
        output_dir.mkdir(parents=True, exist_ok=True)

        # Process each peripheral in configuration
        if not isinstance(config, dict) or 'peripherals' not in config:
            print("Error: Configuration must contain 'peripherals' key with list of peripheral definitions", file=sys.stderr)
            sys.exit(1)

        peripherals = config['peripherals']
        if not isinstance(peripherals, list):
            print("Error: 'peripherals' must be a list", file=sys.stderr)
            sys.exit(1)

        generated_files = []

        for idx, peripheral_config in enumerate(peripherals):
            try:
                # Extract peripheral configuration
                peripheral_name = peripheral_config.get('name')
                if not peripheral_name:
                    print(f"Warning: Skipping peripheral at index {idx}: missing 'name' field", file=sys.stderr)
                    continue

                platform = peripheral_config.get('platform', 'NATIVE')
                instances = peripheral_config.get('instances', 1)
                output_file = peripheral_config.get('output')

                # Get template
                template = get_template(peripheral_name)
                template.platform = platform.upper()

                # Validate instances
                if instances > template.max_instances:
                    print(
                        f"Warning: {peripheral_name} requested {instances} instances, "
                        f"but template maximum is {template.max_instances}. "
                        f"Using {template.max_instances} instances.",
                        file=sys.stderr
                    )
                    instances = template.max_instances

                template.max_instances = instances

                # Create generator
                generator = KconfigGenerator(template)

                # Determine output path
                if output_file:
                    output_path = output_dir / output_file
                else:
                    peripheral_lower = template.name.lower()
                    output_path = output_dir / f"Kconfig_{peripheral_lower}"

                # Generate file
                generator.generate_file(str(output_path))
                generated_files.append(str(output_path))

                print(f"Generated: {output_path} ({peripheral_name}, {platform}, {instances} instances)")

            except ValueError as e:
                print(f"Warning: Skipping peripheral at index {idx}: {e}", file=sys.stderr)
                continue
            except Exception as e:
                print(f"Warning: Error generating peripheral at index {idx}: {e}", file=sys.stderr)
                continue

        # Print summary
        print(f"\nBatch generation complete:")
        print(f"  Files generated: {len(generated_files)}")
        print(f"  Output directory: {output_dir}")

        if len(generated_files) == 0:
            print("Warning: No files were generated", file=sys.stderr)
            sys.exit(1)

    except Exception as e:
        print(f"Error in batch generation: {e}", file=sys.stderr)
        sys.exit(1)


def cmd_batch_validate(args):
    r"""
    \brief           Handle batch-validate command
    \param[in]       args: Command-line arguments
    """
    try:
        directory = Path(args.directory)

        # Check if directory exists
        if not directory.exists():
            print(f"Error: Directory does not exist: {directory}", file=sys.stderr)
            sys.exit(1)

        if not directory.is_dir():
            print(f"Error: Path is not a directory: {directory}", file=sys.stderr)
            sys.exit(1)

        print(f"Scanning directory tree: {directory}")

        # Create validator
        validator = KconfigValidator()

        # Recursively find all Kconfig files
        kconfig_files = []
        for pattern in ['**/Kconfig', '**/Kconfig.*', '**/*.kconfig']:
            kconfig_files.extend(directory.glob(pattern))

        # Remove duplicates and sort
        kconfig_files = sorted(set(kconfig_files))

        if not kconfig_files:
            print(f"Warning: No Kconfig files found in {directory}", file=sys.stderr)
            sys.exit(1)

        print(f"Found {len(kconfig_files)} Kconfig file(s)")

        # Validate all files
        all_results = {}
        for kconfig_file in kconfig_files:
            try:
                issues = validator.validate_file(str(kconfig_file))
                all_results[str(kconfig_file)] = issues

                # Print progress
                issue_count = len(issues)
                if issue_count > 0:
                    print(f"  {kconfig_file.relative_to(directory)}: {issue_count} issue(s)")
                else:
                    print(f"  {kconfig_file.relative_to(directory)}: OK")

            except Exception as e:
                print(f"  {kconfig_file.relative_to(directory)}: Error - {e}", file=sys.stderr)
                continue

        # Generate report
        report = validator.generate_report(all_results)

        # Output report
        if args.report:
            report_path = Path(args.report)
            report_path.write_text(report, encoding='utf-8')
            print(f"\nValidation report written to: {args.report}")
        else:
            print("\n" + "=" * 80)
            print("VALIDATION REPORT")
            print("=" * 80)
            print(report)

        # Count issues by severity
        total_errors = 0
        total_warnings = 0
        total_info = 0
        files_with_issues = 0

        for file_issues in all_results.values():
            if file_issues:
                files_with_issues += 1
            for issue in file_issues:
                if issue.severity == "error":
                    total_errors += 1
                elif issue.severity == "warning":
                    total_warnings += 1
                elif issue.severity == "info":
                    total_info += 1

        # Print summary
        print("\n" + "=" * 80)
        print("SUMMARY")
        print("=" * 80)
        print(f"  Total files checked: {len(all_results)}")
        print(f"  Files with issues: {files_with_issues}")
        print(f"  Files without issues: {len(all_results) - files_with_issues}")
        print(f"  Total errors: {total_errors}")
        print(f"  Total warnings: {total_warnings}")
        print(f"  Total info: {total_info}")

        # Exit with error code if there are errors
        if total_errors > 0:
            print(f"\nValidation failed with {total_errors} error(s)")
            sys.exit(1)
        else:
            print("\nValidation passed!")

    except Exception as e:
        print(f"Error in batch validation: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    r"""
    \brief           Main entry point for CLI
    """
    parser = argparse.ArgumentParser(
        description="Kconfig naming standard tools for Nexus project",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "--version",
        action="version",
        version="%(prog)s 1.0.0"
    )

    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # Generate command
    generate_parser = subparsers.add_parser(
        "generate",
        help="Generate Kconfig file for a peripheral"
    )
    generate_parser.add_argument(
        "peripheral",
        help="Peripheral type (e.g., UART, GPIO, SPI)"
    )
    generate_parser.add_argument(
        "--platform",
        default="NATIVE",
        help="Platform name (default: NATIVE)"
    )
    generate_parser.add_argument(
        "--instances",
        type=int,
        default=1,
        help="Number of instances (default: 1)"
    )
    generate_parser.add_argument(
        "--output",
        "-o",
        help="Output file path"
    )
    generate_parser.set_defaults(func=cmd_generate)

    # Validate command
    validate_parser = subparsers.add_parser(
        "validate",
        help="Validate Kconfig file(s)"
    )
    validate_parser.add_argument(
        "path",
        help="File or directory path to validate"
    )
    validate_parser.add_argument(
        "--report",
        "-r",
        help="Output report file path"
    )
    validate_parser.set_defaults(func=cmd_validate)

    # Batch generate command
    batch_gen_parser = subparsers.add_parser(
        "batch-generate",
        help="Generate multiple Kconfig files from configuration"
    )
    batch_gen_parser.add_argument(
        "config",
        help="Configuration file path (YAML/JSON)"
    )
    batch_gen_parser.add_argument(
        "--output-dir",
        "-o",
        help="Output directory path"
    )
    batch_gen_parser.set_defaults(func=cmd_batch_generate)

    # Batch validate command
    batch_val_parser = subparsers.add_parser(
        "batch-validate",
        help="Validate all Kconfig files in directory tree"
    )
    batch_val_parser.add_argument(
        "directory",
        help="Root directory to scan"
    )
    batch_val_parser.add_argument(
        "--report",
        "-r",
        help="Output report file path"
    )
    batch_val_parser.set_defaults(func=cmd_batch_validate)

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    try:
        args.func(args)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
