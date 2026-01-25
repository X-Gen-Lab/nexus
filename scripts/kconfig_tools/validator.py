r"""
\file            validator.py
\brief           Kconfig file validator
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Provides tools for validating Kconfig files against
                 Nexus naming standards.
"""

from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple
from pathlib import Path
import re
from .naming_rules import NamingRules


@dataclass
class ValidationIssue:
    r"""
    \brief           Validation issue definition
    """
    file: str  # File path
    line: int  # Line number
    severity: str  # Severity: error, warning, info
    rule: str  # Violated rule
    message: str  # Issue description
    suggestion: str = ""  # Fix suggestion


@dataclass
class ConfigItem:
    r"""
    \brief           Parsed configuration item

    \details         Represents a single config item extracted from Kconfig file
    """
    name: str  # Config symbol name
    type: str  # Config type: bool, int, hex, string
    line: int  # Line number where config is defined
    prompt: str = ""  # Prompt text
    default: str = ""  # Default value
    depends_on: List[str] = None  # List of dependencies
    help_text: str = ""  # Help text
    is_menuconfig: bool = False  # Whether this is a menuconfig item

    def __post_init__(self):
        if self.depends_on is None:
            self.depends_on = []


@dataclass
class ChoiceItem:
    r"""
    \brief           Parsed choice item

    \details         Represents a choice block extracted from Kconfig file
    """
    line: int  # Line number where choice starts
    prompt: str = ""  # Choice prompt
    default: str = ""  # Default option
    depends_on: List[str] = None  # List of dependencies
    options: List[str] = None  # List of option config names
    end_line: int = 0  # Line number where choice ends

    def __post_init__(self):
        if self.depends_on is None:
            self.depends_on = []
        if self.options is None:
            self.options = []


class KconfigValidator:
    r"""
    \brief           Kconfig file validator

    \details         Validates Kconfig files against Nexus naming standards
                     and reports issues with fix suggestions.
    """

    def __init__(self, rules: NamingRules = None):
        r"""
        \brief           Initialize validator with naming rules
        \param[in]       rules: Naming rules instance (optional)
        """
        self.rules = rules if rules is not None else NamingRules()

    def _parse_kconfig(self, content: str) -> Tuple[List[ConfigItem], List[ChoiceItem]]:
        r"""
        \brief           Parse Kconfig file content

        \details         Extracts configuration items, choice blocks, and their
                         dependencies from Kconfig file content.

        \param[in]       content: Kconfig file content
        \return          Tuple of (config_items, choice_items)
        """
        config_items = []
        choice_items = []
        lines = content.split('\n')

        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()

            # Parse config or menuconfig items
            if stripped.startswith('config ') or stripped.startswith('menuconfig '):
                is_menuconfig = stripped.startswith('menuconfig ')
                config_name = stripped.split()[1]
                config_type = ""
                prompt = ""
                default = ""
                depends_on = []
                help_text = ""

                # Look ahead for config details
                j = i + 1
                in_help = False
                while j < len(lines):
                    detail_line = lines[j]
                    detail_stripped = detail_line.strip()

                    # Stop at next config, choice, or endif
                    if (detail_stripped.startswith('config ') or
                        detail_stripped.startswith('menuconfig ') or
                        detail_stripped.startswith('choice') or
                        detail_stripped.startswith('endif') or
                        detail_stripped.startswith('endchoice')):
                        break

                    # Parse type
                    if detail_stripped.startswith('bool '):
                        config_type = "bool"
                        prompt = detail_stripped[5:].strip('"')
                    elif detail_stripped.startswith('int '):
                        config_type = "int"
                        prompt = detail_stripped[4:].strip('"')
                    elif detail_stripped.startswith('hex '):
                        config_type = "hex"
                        prompt = detail_stripped[4:].strip('"')
                    elif detail_stripped.startswith('string '):
                        config_type = "string"
                        prompt = detail_stripped[7:].strip('"')

                    # Parse default
                    if detail_stripped.startswith('default '):
                        default = detail_stripped[8:]

                    # Parse depends on
                    if detail_stripped.startswith('depends on '):
                        dep = detail_stripped[11:].strip()
                        depends_on.append(dep)

                    # Parse help text
                    if detail_stripped == 'help':
                        in_help = True
                    elif in_help:
                        if detail_stripped and not detail_stripped.startswith(('config', 'menuconfig', 'choice', 'if', 'endif')):
                            help_text += detail_stripped + " "
                        else:
                            in_help = False

                    j += 1

                config_items.append(ConfigItem(
                    name=config_name,
                    type=config_type,
                    line=i + 1,
                    prompt=prompt,
                    default=default,
                    depends_on=depends_on,
                    help_text=help_text.strip(),
                    is_menuconfig=is_menuconfig
                ))

                i = j - 1

            # Parse choice blocks
            elif stripped.startswith('choice'):
                choice_prompt = ""
                choice_default = ""
                choice_depends = []
                choice_options = []
                choice_start = i

                # Look ahead for choice details
                j = i + 1
                while j < len(lines):
                    choice_line = lines[j]
                    choice_stripped = choice_line.strip()

                    # Stop at endchoice
                    if choice_stripped.startswith('endchoice'):
                        break

                    # Parse prompt
                    if choice_stripped.startswith('prompt '):
                        choice_prompt = choice_stripped[7:].strip('"')

                    # Parse default
                    if choice_stripped.startswith('default '):
                        choice_default = choice_stripped[8:].strip()

                    # Parse depends on
                    if choice_stripped.startswith('depends on '):
                        dep = choice_stripped[11:].strip()
                        choice_depends.append(dep)

                    # Parse config options within choice
                    if choice_stripped.startswith('config '):
                        option_name = choice_stripped.split()[1]
                        choice_options.append(option_name)

                    j += 1

                choice_items.append(ChoiceItem(
                    line=choice_start + 1,
                    prompt=choice_prompt,
                    default=choice_default,
                    depends_on=choice_depends,
                    options=choice_options,
                    end_line=j + 1
                ))

                i = j

            i += 1

        return config_items, choice_items

    def validate_file(self, file_path: str) -> List[ValidationIssue]:
        r"""
        \brief           Validate single Kconfig file
        \param[in]       file_path: Path to Kconfig file
        \return          List of validation issues
        """
        pass

    def validate_directory(self, dir_path: str) -> Dict[str, List[ValidationIssue]]:
        r"""
        \brief           Validate all Kconfig files in directory
        \param[in]       dir_path: Directory path
        \return          Dictionary mapping file paths to issue lists
        """
        pass

    def check_platform_naming(self, content: str, file_path: str = "") -> List[ValidationIssue]:
        r"""
        \brief           Check platform-level naming
        \param[in]       content: File content
        \param[in]       file_path: File path for reporting
        \return          List of validation issues
        """
        issues = []
        lines = content.split('\n')

        for i, line in enumerate(lines):
            stripped = line.strip()

            # Check for config declarations
            if stripped.startswith('config '):
                config_name = stripped.split()[1]

                # Check platform enable pattern
                if '_ENABLE' in config_name and config_name.count('_') == 1:
                    if not self.rules.validate_pattern(config_name, self.rules.PLATFORM_ENABLE_PATTERN):
                        issues.append(ValidationIssue(
                            file=file_path,
                            line=i + 1,
                            severity="error",
                            rule="platform_enable_naming",
                            message=f"Platform enable config '{config_name}' does not match pattern {self.rules.PLATFORM_ENABLE_PATTERN}",
                            suggestion=f"Use format: {{PLATFORM}}_ENABLE (e.g., NATIVE_ENABLE)"
                        ))

                # Check platform name pattern
                if '_PLATFORM_NAME' in config_name:
                    if not self.rules.validate_pattern(config_name, self.rules.PLATFORM_NAME_PATTERN):
                        issues.append(ValidationIssue(
                            file=file_path,
                            line=i + 1,
                            severity="error",
                            rule="platform_name_naming",
                            message=f"Platform name config '{config_name}' does not match pattern {self.rules.PLATFORM_NAME_PATTERN}",
                            suggestion=f"Use format: {{PLATFORM}}_PLATFORM_NAME (e.g., NATIVE_PLATFORM_NAME)"
                        ))

                # Check platform version pattern
                if '_PLATFORM_VERSION' in config_name:
                    if not self.rules.validate_pattern(config_name, self.rules.PLATFORM_VERSION_PATTERN):
                        issues.append(ValidationIssue(
                            file=file_path,
                            line=i + 1,
                            severity="error",
                            rule="platform_version_naming",
                            message=f"Platform version config '{config_name}' does not match pattern {self.rules.PLATFORM_VERSION_PATTERN}",
                            suggestion=f"Use format: {{PLATFORM}}_PLATFORM_VERSION (e.g., NATIVE_PLATFORM_VERSION)"
                        ))

        return issues

    def check_peripheral_naming(self, content: str, file_path: str = "") -> List[ValidationIssue]:
        r"""
        \brief           Check peripheral-level naming
        \param[in]       content: File content
        \param[in]       file_path: File path for reporting
        \return          List of validation issues
        """
        issues = []
        lines = content.split('\n')

        for i, line in enumerate(lines):
            stripped = line.strip()

            # Check for config declarations
            if stripped.startswith('config '):
                config_name = stripped.split()[1]

                # Check peripheral enable pattern (PLATFORM_PERIPHERAL_ENABLE)
                if '_ENABLE' in config_name and config_name.count('_') >= 2:
                    # Skip instance-level enables (INSTANCE_NX_*)
                    if not config_name.startswith('INSTANCE_NX_'):
                        if not self.rules.validate_pattern(config_name, self.rules.PERIPHERAL_ENABLE_PATTERN):
                            issues.append(ValidationIssue(
                                file=file_path,
                                line=i + 1,
                                severity="error",
                                rule="peripheral_enable_naming",
                                message=f"Peripheral enable config '{config_name}' does not match pattern {self.rules.PERIPHERAL_ENABLE_PATTERN}",
                                suggestion=f"Use format: {{PLATFORM}}_{{PERIPHERAL}}_ENABLE (e.g., NATIVE_UART_ENABLE)"
                            ))

                # Check peripheral max instances pattern
                if '_MAX_INSTANCES' in config_name:
                    if not self.rules.validate_pattern(config_name, self.rules.PERIPHERAL_MAX_INSTANCES_PATTERN):
                        issues.append(ValidationIssue(
                            file=file_path,
                            line=i + 1,
                            severity="error",
                            rule="peripheral_max_instances_naming",
                            message=f"Peripheral max instances config '{config_name}' does not match pattern {self.rules.PERIPHERAL_MAX_INSTANCES_PATTERN}",
                            suggestion=f"Use format: {{PLATFORM}}_{{PERIPHERAL}}_MAX_INSTANCES (e.g., NATIVE_UART_MAX_INSTANCES)"
                        ))

        return issues

    def check_instance_naming(self, content: str, file_path: str = "") -> List[ValidationIssue]:
        r"""
        \brief           Check instance-level naming
        \param[in]       content: File content
        \param[in]       file_path: File path for reporting
        \return          List of validation issues
        """
        issues = []
        lines = content.split('\n')

        for i, line in enumerate(lines):
            stripped = line.strip()

            # Check for config declarations
            if stripped.startswith('config ') or stripped.startswith('menuconfig '):
                config_name = stripped.split()[1]

                # Check instance enable pattern (INSTANCE_NX_*)
                if config_name.startswith('INSTANCE_NX_'):
                    if not self.rules.validate_pattern(config_name, self.rules.INSTANCE_ENABLE_PATTERN):
                        issues.append(ValidationIssue(
                            file=file_path,
                            line=i + 1,
                            severity="error",
                            rule="instance_enable_naming",
                            message=f"Instance enable config '{config_name}' does not match pattern {self.rules.INSTANCE_ENABLE_PATTERN}",
                            suggestion=f"Use format: INSTANCE_NX_{{PERIPHERAL}}_{{N}} (e.g., INSTANCE_NX_UART_0, INSTANCE_NX_GPIOA)"
                        ))

                # Check instance parameter pattern (PERIPHERAL{N}_PARAMETER)
                # Must not start with INSTANCE_NX_ or NX_, and must contain a digit or letter before underscore
                elif (not config_name.startswith('NX_') and
                      not config_name.startswith('INSTANCE_') and
                      not config_name.endswith('_ENABLE') and
                      not config_name.endswith('_MAX_INSTANCES') and
                      not config_name.endswith('_PLATFORM_NAME') and
                      not config_name.endswith('_PLATFORM_VERSION') and
                      '_VALUE' not in config_name):
                    # Check if it looks like an instance parameter (has digit or ends with letter before underscore)
                    match = re.search(r'[A-Z]+[0-9A-Z]+_[A-Z]', config_name)
                    if match:
                        if not self.rules.validate_pattern(config_name, self.rules.INSTANCE_PARAM_PATTERN):
                            issues.append(ValidationIssue(
                                file=file_path,
                                line=i + 1,
                                severity="warning",
                                rule="instance_param_naming",
                                message=f"Instance parameter config '{config_name}' may not match pattern {self.rules.INSTANCE_PARAM_PATTERN}",
                                suggestion=f"Use format: {{PERIPHERAL}}{{N}}_{{PARAMETER}} (e.g., UART0_BAUDRATE, GPIOA_PIN0_MODE)"
                            ))

        return issues

    def check_choice_naming(self, content: str, file_path: str = "") -> List[ValidationIssue]:
        r"""
        \brief           Check choice naming
        \param[in]       content: File content
        \param[in]       file_path: File path for reporting
        \return          List of validation issues
        """
        issues = []
        lines = content.split('\n')

        for i, line in enumerate(lines):
            stripped = line.strip()

            # Check for config declarations
            if stripped.startswith('config '):
                config_name = stripped.split()[1]

                # Check choice option pattern (NX_PERIPHERAL{N}_CATEGORY_OPTION)
                if config_name.startswith('NX_') and not config_name.startswith('NX_HAL'):
                    # Skip if it's a value config
                    if not config_name.endswith('_VALUE'):
                        if not self.rules.validate_pattern(config_name, self.rules.CHOICE_OPTION_PATTERN):
                            issues.append(ValidationIssue(
                                file=file_path,
                                line=i + 1,
                                severity="error",
                                rule="choice_option_naming",
                                message=f"Choice option config '{config_name}' does not match pattern {self.rules.CHOICE_OPTION_PATTERN}",
                                suggestion=f"Use format: NX_{{PERIPHERAL}}{{N}}_{{CATEGORY}}_{{OPTION}} (e.g., NX_UART0_PARITY_NONE)"
                            ))

                # Check choice value pattern (PERIPHERAL{N}_CATEGORY_VALUE)
                if config_name.endswith('_VALUE') and not config_name.startswith('NX_'):
                    if not self.rules.validate_pattern(config_name, self.rules.CHOICE_VALUE_PATTERN):
                        issues.append(ValidationIssue(
                            file=file_path,
                            line=i + 1,
                            severity="error",
                            rule="choice_value_naming",
                            message=f"Choice value config '{config_name}' does not match pattern {self.rules.CHOICE_VALUE_PATTERN}",
                            suggestion=f"Use format: {{PERIPHERAL}}{{N}}_{{CATEGORY}}_VALUE (e.g., UART0_PARITY_VALUE)"
                        ))

        return issues

    def check_structure(self, content: str, file_path: str = "") -> List[ValidationIssue]:
        r"""
        \brief           Check file structure
        \param[in]       content: File content
        \param[in]       file_path: File path for reporting
        \return          List of validation issues
        """
        issues = []
        config_items, choice_items = self._parse_kconfig(content)

        # Check help text existence for all config items
        for item in config_items:
            if item.type in ['bool', 'int', 'hex', 'string'] and not item.help_text:
                issues.append(ValidationIssue(
                    file=file_path,
                    line=item.line,
                    severity="warning",
                    rule="help_text_missing",
                    message=f"Config '{item.name}' is missing help text",
                    suggestion="Add 'help' keyword followed by descriptive text"
                ))

        # Check choice completeness
        for choice in choice_items:
            # Check if choice has prompt
            if not choice.prompt:
                issues.append(ValidationIssue(
                    file=file_path,
                    line=choice.line,
                    severity="warning",
                    rule="choice_prompt_missing",
                    message=f"Choice at line {choice.line} is missing prompt",
                    suggestion="Add 'prompt' keyword with descriptive text"
                ))

            # Check if choice has default
            if not choice.default:
                issues.append(ValidationIssue(
                    file=file_path,
                    line=choice.line,
                    severity="warning",
                    rule="choice_default_missing",
                    message=f"Choice at line {choice.line} is missing default option",
                    suggestion="Add 'default' keyword with one of the choice options"
                ))

            # Check if choice has at least one option
            if not choice.options:
                issues.append(ValidationIssue(
                    file=file_path,
                    line=choice.line,
                    severity="error",
                    rule="choice_options_missing",
                    message=f"Choice at line {choice.line} has no options",
                    suggestion="Add at least one 'config' option within the choice block"
                ))

            # Check if choice has corresponding VALUE config
            if choice.options:
                # Extract peripheral and instance from first option
                # Format: NX_PERIPHERAL{N}_CATEGORY_OPTION
                first_option = choice.options[0]
                if first_option.startswith('NX_'):
                    # Extract the value config name pattern
                    # NX_UART0_PARITY_NONE -> UART0_PARITY_VALUE
                    parts = first_option.split('_')
                    if len(parts) >= 4:
                        # NX_UART0_PARITY_NONE -> UART0_PARITY
                        value_prefix = '_'.join(parts[1:-1])
                        expected_value_config = f"{value_prefix}_VALUE"

                        # Check if this VALUE config exists
                        value_config_found = any(
                            item.name == expected_value_config
                            for item in config_items
                        )

                        if not value_config_found:
                            issues.append(ValidationIssue(
                                file=file_path,
                                line=choice.end_line,
                                severity="error",
                                rule="choice_value_config_missing",
                                message=f"Choice at line {choice.line} is missing corresponding VALUE config '{expected_value_config}'",
                                suggestion=f"Add 'config {expected_value_config}' with int type and default values for each option"
                            ))

        # Check file structure order (peripheral enable should come before instances)
        peripheral_enable_line = None
        first_instance_line = None

        for item in config_items:
            if '_ENABLE' in item.name and not item.name.startswith('INSTANCE_NX_'):
                if peripheral_enable_line is None:
                    peripheral_enable_line = item.line
            elif item.name.startswith('INSTANCE_NX_'):
                if first_instance_line is None:
                    first_instance_line = item.line

        if peripheral_enable_line and first_instance_line:
            if peripheral_enable_line > first_instance_line:
                issues.append(ValidationIssue(
                    file=file_path,
                    line=first_instance_line,
                    severity="warning",
                    rule="structure_order_violation",
                    message=f"Instance configuration at line {first_instance_line} appears before peripheral enable at line {peripheral_enable_line}",
                    suggestion="Move peripheral enable configuration before instance configurations"
                ))

        # Check instance numbering continuity
        instance_numbers = {}
        for item in config_items:
            if item.name.startswith('INSTANCE_NX_'):
                # Extract peripheral and instance number
                # INSTANCE_NX_UART_0 -> UART, 0
                parts = item.name.split('_')
                if len(parts) >= 4:
                    peripheral = parts[2]
                    instance = parts[3]
                    if instance.isdigit():
                        if peripheral not in instance_numbers:
                            instance_numbers[peripheral] = []
                        instance_numbers[peripheral].append((int(instance), item.line))

        # Check for gaps in instance numbering
        for peripheral, instances in instance_numbers.items():
            instances.sort()  # Sort by instance number
            numbers = [num for num, _ in instances]
            if numbers:
                expected = list(range(numbers[0], numbers[-1] + 1))
                if numbers != expected:
                    missing = set(expected) - set(numbers)
                    issues.append(ValidationIssue(
                        file=file_path,
                        line=instances[0][1],
                        severity="warning",
                        rule="instance_numbering_gap",
                        message=f"Peripheral '{peripheral}' has gaps in instance numbering: missing {missing}",
                        suggestion=f"Instance numbers should be continuous starting from 0"
                    ))

        return issues

    def validate_file(self, file_path: str) -> List[ValidationIssue]:
        r"""
        \brief           Validate single Kconfig file
        \param[in]       file_path: Path to Kconfig file
        \return          List of validation issues
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()

            issues = []
            issues.extend(self.check_platform_naming(content, file_path))
            issues.extend(self.check_peripheral_naming(content, file_path))
            issues.extend(self.check_instance_naming(content, file_path))
            issues.extend(self.check_choice_naming(content, file_path))
            issues.extend(self.check_structure(content, file_path))

            return issues

        except FileNotFoundError:
            return [ValidationIssue(
                file=file_path,
                line=0,
                severity="error",
                rule="file_not_found",
                message=f"File not found: {file_path}",
                suggestion="Check if the file path is correct"
            )]
        except Exception as e:
            return [ValidationIssue(
                file=file_path,
                line=0,
                severity="error",
                rule="validation_error",
                message=f"Error validating file: {str(e)}",
                suggestion="Check if the file is a valid Kconfig file"
            )]

    def validate_directory(self, dir_path: str) -> Dict[str, List[ValidationIssue]]:
        r"""
        \brief           Validate all Kconfig files in directory
        \param[in]       dir_path: Directory path
        \return          Dictionary mapping file paths to issue lists
        """
        results = {}
        dir_path_obj = Path(dir_path)

        if not dir_path_obj.exists():
            return {dir_path: [ValidationIssue(
                file=dir_path,
                line=0,
                severity="error",
                rule="directory_not_found",
                message=f"Directory not found: {dir_path}",
                suggestion="Check if the directory path is correct"
            )]}

        # Find all Kconfig files recursively
        kconfig_files = list(dir_path_obj.rglob('Kconfig*'))

        for kconfig_file in kconfig_files:
            file_path = str(kconfig_file)
            issues = self.validate_file(file_path)
            if issues:
                results[file_path] = issues

        return results

    def generate_report(self, issues: Dict[str, List[ValidationIssue]]) -> str:
        r"""
        \brief           Generate validation report
        \param[in]       issues: Dictionary of issues by file
        \return          Formatted report string
        """
        if not issues:
            return "✓ No validation issues found.\n"

        report_lines = []
        report_lines.append("=" * 80)
        report_lines.append("Kconfig Validation Report")
        report_lines.append("=" * 80)
        report_lines.append("")

        # Count issues by severity
        total_errors = 0
        total_warnings = 0
        total_info = 0

        for file_issues in issues.values():
            for issue in file_issues:
                if issue.severity == "error":
                    total_errors += 1
                elif issue.severity == "warning":
                    total_warnings += 1
                elif issue.severity == "info":
                    total_info += 1

        # Summary
        report_lines.append(f"Total files with issues: {len(issues)}")
        report_lines.append(f"Total errors: {total_errors}")
        report_lines.append(f"Total warnings: {total_warnings}")
        report_lines.append(f"Total info: {total_info}")
        report_lines.append("")
        report_lines.append("=" * 80)
        report_lines.append("")

        # Detailed issues by file
        for file_path, file_issues in sorted(issues.items()):
            report_lines.append(f"File: {file_path}")
            report_lines.append("-" * 80)

            # Sort issues by line number
            sorted_issues = sorted(file_issues, key=lambda x: x.line)

            for issue in sorted_issues:
                severity_symbol = {
                    "error": "✗",
                    "warning": "⚠",
                    "info": "ℹ"
                }.get(issue.severity, "•")

                report_lines.append(f"  {severity_symbol} Line {issue.line}: [{issue.severity.upper()}] {issue.rule}")
                report_lines.append(f"    Message: {issue.message}")
                if issue.suggestion:
                    report_lines.append(f"    Suggestion: {issue.suggestion}")
                report_lines.append("")

            report_lines.append("")

        report_lines.append("=" * 80)
        report_lines.append("End of Report")
        report_lines.append("=" * 80)

        return "\n".join(report_lines)

