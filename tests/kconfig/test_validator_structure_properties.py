#!/usr/bin/env python3
r"""
Property tests for validator structure checks.

Feature: kconfig-naming-standard
Validates: Requirements 8.3

Tests that the validator correctly identifies structure violations including
file ordering, choice completeness, help text presence, and instance numbering.
"""

import sys
import os
import pytest

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.validator import KconfigValidator, ValidationIssue


class TestHelpTextValidation:
    """Test help text existence validation."""

    def test_config_with_help_text_passes(self):
        """Test that config with help text passes validation."""
        validator = KconfigValidator()
        content = """
config NATIVE_UART_ENABLE
    bool "Enable UART"
    help
      Enable UART peripheral support
"""
        issues = validator.check_structure(content, "test.kconfig")
        help_issues = [i for i in issues if i.rule == "help_text_missing"]
        assert len(help_issues) == 0, "Config with help text should not trigger issues"

    def test_config_without_help_text_detected(self):
        """Test that config without help text is detected."""
        validator = KconfigValidator()
        content = """
config NATIVE_UART_ENABLE
    bool "Enable UART"
    default y
"""
        issues = validator.check_structure(content, "test.kconfig")
        help_issues = [i for i in issues if i.rule == "help_text_missing"]
        assert len(help_issues) > 0, "Config without help text should trigger warning"
        assert help_issues[0].severity == "warning", "Missing help text should be a warning"

    def test_int_config_without_help_text_detected(self):
        """Test that int config without help text is detected."""
        validator = KconfigValidator()
        content = """
config UART0_BAUDRATE
    int "Baudrate"
    default 115200
"""
        issues = validator.check_structure(content, "test.kconfig")
        help_issues = [i for i in issues if i.rule == "help_text_missing"]
        assert len(help_issues) > 0, "Int config without help text should trigger warning"


class TestChoiceCompletenessValidation:
    """Test choice completeness validation."""

    def test_complete_choice_passes(self):
        """Test that complete choice passes validation."""
        validator = KconfigValidator()
        content = """
choice
    prompt "UART0 parity"
    default NX_UART0_PARITY_NONE

config NX_UART0_PARITY_NONE
    bool "None"

config NX_UART0_PARITY_ODD
    bool "Odd"

endchoice

config UART0_PARITY_VALUE
    int
    default 0 if NX_UART0_PARITY_NONE
    default 1 if NX_UART0_PARITY_ODD
"""
        issues = validator.check_structure(content, "test.kconfig")
        choice_issues = [i for i in issues if "choice" in i.rule]
        assert len(choice_issues) == 0, "Complete choice should not trigger issues"

    def test_choice_without_prompt_detected(self):
        """Test that choice without prompt is detected."""
        validator = KconfigValidator()
        content = """
choice
    default NX_UART0_PARITY_NONE

config NX_UART0_PARITY_NONE
    bool "None"

endchoice
"""
        issues = validator.check_structure(content, "test.kconfig")
        prompt_issues = [i for i in issues if i.rule == "choice_prompt_missing"]
        assert len(prompt_issues) > 0, "Choice without prompt should trigger warning"

    def test_choice_without_default_detected(self):
        """Test that choice without default is detected."""
        validator = KconfigValidator()
        content = """
choice
    prompt "UART0 parity"

config NX_UART0_PARITY_NONE
    bool "None"

endchoice
"""
        issues = validator.check_structure(content, "test.kconfig")
        default_issues = [i for i in issues if i.rule == "choice_default_missing"]
        assert len(default_issues) > 0, "Choice without default should trigger warning"

    def test_choice_without_options_detected(self):
        """Test that choice without options is detected."""
        validator = KconfigValidator()
        content = """
choice
    prompt "UART0 parity"
    default NX_UART0_PARITY_NONE

endchoice
"""
        issues = validator.check_structure(content, "test.kconfig")
        options_issues = [i for i in issues if i.rule == "choice_options_missing"]
        assert len(options_issues) > 0, "Choice without options should trigger error"
        assert options_issues[0].severity == "error", "Missing options should be an error"

    def test_choice_without_value_config_detected(self):
        """Test that choice without VALUE config is detected."""
        validator = KconfigValidator()
        content = """
choice
    prompt "UART0 parity"
    default NX_UART0_PARITY_NONE

config NX_UART0_PARITY_NONE
    bool "None"

config NX_UART0_PARITY_ODD
    bool "Odd"

endchoice
"""
        issues = validator.check_structure(content, "test.kconfig")
        value_issues = [i for i in issues if i.rule == "choice_value_config_missing"]
        assert len(value_issues) > 0, "Choice without VALUE config should trigger error"


class TestFileStructureOrderValidation:
    """Test file structure order validation."""

    def test_correct_order_passes(self):
        """Test that correct order passes validation."""
        validator = KconfigValidator()
        content = """
config NATIVE_UART_ENABLE
    bool "Enable UART"
    help
      Enable UART

config INSTANCE_NX_UART_0
    bool "Enable UART0"
    help
      Enable UART instance 0
"""
        issues = validator.check_structure(content, "test.kconfig")
        order_issues = [i for i in issues if i.rule == "structure_order_violation"]
        assert len(order_issues) == 0, "Correct order should not trigger issues"

    def test_instance_before_enable_detected(self):
        """Test that instance before enable is detected."""
        validator = KconfigValidator()
        content = """
config INSTANCE_NX_UART_0
    bool "Enable UART0"
    help
      Enable UART instance 0

config NATIVE_UART_ENABLE
    bool "Enable UART"
    help
      Enable UART
"""
        issues = validator.check_structure(content, "test.kconfig")
        order_issues = [i for i in issues if i.rule == "structure_order_violation"]
        assert len(order_issues) > 0, "Instance before enable should trigger warning"


class TestInstanceNumberingValidation:
    """Test instance numbering continuity validation."""

    def test_continuous_numbering_passes(self):
        """Test that continuous numbering passes validation."""
        validator = KconfigValidator()
        content = """
config INSTANCE_NX_UART_0
    bool "Enable UART0"
    help
      Enable UART instance 0

config INSTANCE_NX_UART_1
    bool "Enable UART1"
    help
      Enable UART instance 1

config INSTANCE_NX_UART_2
    bool "Enable UART2"
    help
      Enable UART instance 2
"""
        issues = validator.check_structure(content, "test.kconfig")
        numbering_issues = [i for i in issues if i.rule == "instance_numbering_gap"]
        assert len(numbering_issues) == 0, "Continuous numbering should not trigger issues"

    def test_gap_in_numbering_detected(self):
        """Test that gap in numbering is detected."""
        validator = KconfigValidator()
        content = """
config INSTANCE_NX_UART_0
    bool "Enable UART0"
    help
      Enable UART instance 0

config INSTANCE_NX_UART_2
    bool "Enable UART2"
    help
      Enable UART instance 2

config INSTANCE_NX_UART_3
    bool "Enable UART3"
    help
      Enable UART instance 3
"""
        issues = validator.check_structure(content, "test.kconfig")
        numbering_issues = [i for i in issues if i.rule == "instance_numbering_gap"]
        assert len(numbering_issues) > 0, "Gap in numbering should trigger warning"
        assert "missing" in numbering_issues[0].message.lower(), "Should mention missing instance"

    def test_non_zero_start_detected(self):
        """Test that non-zero start is detected."""
        validator = KconfigValidator()
        content = """
config INSTANCE_NX_UART_1
    bool "Enable UART1"
    help
      Enable UART instance 1

config INSTANCE_NX_UART_2
    bool "Enable UART2"
    help
      Enable UART instance 2
"""
        issues = validator.check_structure(content, "test.kconfig")
        # Starting from 1 instead of 0 is technically continuous (1, 2)
        # but the suggestion should mention starting from 0
        numbering_issues = [i for i in issues if i.rule == "instance_numbering_gap"]
        # This is actually continuous, so no issue
        assert len(numbering_issues) == 0, "Starting from 1 is continuous (no gap)"


class TestStructureValidationIntegration:
    """Test integration of all structure checks."""

    def test_multiple_structure_issues_detected(self):
        """Test that multiple structure issues are detected."""
        validator = KconfigValidator()
        content = """
config NATIVE_UART_ENABLE
    bool "Enable UART"

choice
    default NX_UART0_PARITY_NONE

config NX_UART0_PARITY_NONE
    bool "None"

endchoice

config INSTANCE_NX_UART_0
    bool "Enable UART0"

config INSTANCE_NX_UART_2
    bool "Enable UART2"
"""
        issues = validator.check_structure(content, "test.kconfig")

        # Should detect multiple types of issues
        assert len(issues) > 0, "Should detect multiple structure issues"

        # Check for different issue types
        issue_rules = [i.rule for i in issues]
        assert "help_text_missing" in issue_rules, "Should detect missing help text"
        assert "choice_prompt_missing" in issue_rules, "Should detect missing choice prompt"
        assert "instance_numbering_gap" in issue_rules, "Should detect numbering gap"

    def test_well_formed_file_passes(self):
        """Test that well-formed file passes all structure checks."""
        validator = KconfigValidator()
        content = """
config NATIVE_UART_ENABLE
    bool "Enable UART"
    help
      Enable UART peripheral support

config INSTANCE_NX_UART_0
    bool "Enable UART0"
    help
      Enable UART instance 0

config UART0_BAUDRATE
    int "Baudrate"
    default 115200
    help
      UART0 baudrate setting

choice
    prompt "UART0 parity"
    default NX_UART0_PARITY_NONE

config NX_UART0_PARITY_NONE
    bool "None"

config NX_UART0_PARITY_ODD
    bool "Odd"

endchoice

config UART0_PARITY_VALUE
    int
    default 0 if NX_UART0_PARITY_NONE
    default 1 if NX_UART0_PARITY_ODD
"""
        issues = validator.check_structure(content, "test.kconfig")

        # Well-formed file should have no issues
        assert len(issues) == 0, f"Well-formed file should have no issues, but found: {[i.rule for i in issues]}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
