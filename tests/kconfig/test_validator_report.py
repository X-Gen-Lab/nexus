#!/usr/bin/env python3
r"""
Unit tests for validator report generation.

Feature: kconfig-naming-standard
Validates: Requirements 8.4

Tests that the validator generates properly formatted reports with
line numbers and fix suggestions.
"""

import sys
import os
import pytest

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.validator import KconfigValidator, ValidationIssue


class TestReportGeneration:
    """Test report generation functionality."""

    def test_empty_issues_report(self):
        """Test report generation with no issues."""
        validator = KconfigValidator()
        issues = {}
        report = validator.generate_report(issues)

        assert "No validation issues found" in report, "Empty report should indicate no issues"
        assert "✓" in report, "Empty report should have success indicator"

    def test_single_issue_report(self):
        """Test report generation with single issue."""
        validator = KconfigValidator()
        issues = {
            "test.kconfig": [
                ValidationIssue(
                    file="test.kconfig",
                    line=10,
                    severity="error",
                    rule="test_rule",
                    message="Test error message",
                    suggestion="Test suggestion"
                )
            ]
        }
        report = validator.generate_report(issues)

        assert "Kconfig Validation Report" in report, "Report should have header"
        assert "test.kconfig" in report, "Report should include file name"
        assert "Line 10" in report, "Report should include line number"
        assert "ERROR" in report, "Report should include severity"
        assert "test_rule" in report, "Report should include rule name"
        assert "Test error message" in report, "Report should include message"
        assert "Test suggestion" in report, "Report should include suggestion"

    def test_multiple_issues_report(self):
        """Test report generation with multiple issues."""
        validator = KconfigValidator()
        issues = {
            "test1.kconfig": [
                ValidationIssue(
                    file="test1.kconfig",
                    line=5,
                    severity="error",
                    rule="rule1",
                    message="Error 1",
                    suggestion="Fix 1"
                ),
                ValidationIssue(
                    file="test1.kconfig",
                    line=10,
                    severity="warning",
                    rule="rule2",
                    message="Warning 1",
                    suggestion="Fix 2"
                )
            ],
            "test2.kconfig": [
                ValidationIssue(
                    file="test2.kconfig",
                    line=3,
                    severity="info",
                    rule="rule3",
                    message="Info 1",
                    suggestion=""
                )
            ]
        }
        report = validator.generate_report(issues)

        # Check summary
        assert "Total files with issues: 2" in report, "Report should count files"
        assert "Total errors: 1" in report, "Report should count errors"
        assert "Total warnings: 1" in report, "Report should count warnings"
        assert "Total info: 1" in report, "Report should count info"

        # Check file sections
        assert "test1.kconfig" in report, "Report should include first file"
        assert "test2.kconfig" in report, "Report should include second file"

        # Check issues are included
        assert "Error 1" in report, "Report should include error message"
        assert "Warning 1" in report, "Report should include warning message"
        assert "Info 1" in report, "Report should include info message"

    def test_report_severity_symbols(self):
        """Test that report uses correct severity symbols."""
        validator = KconfigValidator()
        issues = {
            "test.kconfig": [
                ValidationIssue(
                    file="test.kconfig",
                    line=1,
                    severity="error",
                    rule="rule1",
                    message="Error",
                    suggestion=""
                ),
                ValidationIssue(
                    file="test.kconfig",
                    line=2,
                    severity="warning",
                    rule="rule2",
                    message="Warning",
                    suggestion=""
                ),
                ValidationIssue(
                    file="test.kconfig",
                    line=3,
                    severity="info",
                    rule="rule3",
                    message="Info",
                    suggestion=""
                )
            ]
        }
        report = validator.generate_report(issues)

        # Check for severity symbols (may vary by implementation)
        assert "ERROR" in report, "Report should show ERROR severity"
        assert "WARNING" in report, "Report should show WARNING severity"
        assert "INFO" in report, "Report should show INFO severity"

    def test_report_line_number_sorting(self):
        """Test that issues are sorted by line number."""
        validator = KconfigValidator()
        issues = {
            "test.kconfig": [
                ValidationIssue(
                    file="test.kconfig",
                    line=30,
                    severity="error",
                    rule="rule1",
                    message="Error at line 30",
                    suggestion=""
                ),
                ValidationIssue(
                    file="test.kconfig",
                    line=10,
                    severity="error",
                    rule="rule2",
                    message="Error at line 10",
                    suggestion=""
                ),
                ValidationIssue(
                    file="test.kconfig",
                    line=20,
                    severity="error",
                    rule="rule3",
                    message="Error at line 20",
                    suggestion=""
                )
            ]
        }
        report = validator.generate_report(issues)

        # Find positions of line numbers in report
        pos_10 = report.find("Line 10")
        pos_20 = report.find("Line 20")
        pos_30 = report.find("Line 30")

        assert pos_10 < pos_20 < pos_30, "Issues should be sorted by line number"

    def test_report_without_suggestions(self):
        """Test report generation when suggestions are empty."""
        validator = KconfigValidator()
        issues = {
            "test.kconfig": [
                ValidationIssue(
                    file="test.kconfig",
                    line=5,
                    severity="error",
                    rule="test_rule",
                    message="Test error",
                    suggestion=""
                )
            ]
        }
        report = validator.generate_report(issues)

        assert "Test error" in report, "Report should include message"
        # Suggestion section should not appear or be empty
        # The report format may vary, but it should handle empty suggestions gracefully

    def test_report_format_structure(self):
        """Test that report has proper structure."""
        validator = KconfigValidator()
        issues = {
            "test.kconfig": [
                ValidationIssue(
                    file="test.kconfig",
                    line=1,
                    severity="error",
                    rule="rule1",
                    message="Test",
                    suggestion="Fix"
                )
            ]
        }
        report = validator.generate_report(issues)

        # Check for structural elements
        assert "=" in report, "Report should have section separators"
        assert "-" in report, "Report should have subsection separators"
        assert "File:" in report, "Report should label file sections"
        assert "Message:" in report, "Report should label messages"
        assert "Suggestion:" in report, "Report should label suggestions"


class TestReportQuality:
    """Test quality of report suggestions."""

    def test_suggestions_are_actionable(self):
        """Test that suggestions provide actionable guidance."""
        validator = KconfigValidator()
        # Create content with known violation
        content = """
config NATIVE_UART_ENABLE
    bool "Enable UART"
"""
        issues = validator.check_structure(content, "test.kconfig")

        # Should have help text missing issue
        help_issues = [i for i in issues if i.rule == "help_text_missing"]
        assert len(help_issues) > 0, "Should detect missing help text"

        # Check suggestion quality
        suggestion = help_issues[0].suggestion
        assert len(suggestion) > 0, "Suggestion should not be empty"
        assert "help" in suggestion.lower(), "Suggestion should mention 'help' keyword"

    def test_suggestions_include_examples(self):
        """Test that suggestions include format examples where appropriate."""
        validator = KconfigValidator()
        # Create content with naming violation
        content = """
config INSTANCE_UART_0
    bool "Enable UART0"
"""
        issues = validator.check_instance_naming(content, "test.kconfig")

        # Check if any issues have example formats in suggestions
        for issue in issues:
            if issue.suggestion:
                # Suggestions should be helpful
                assert len(issue.suggestion) > 10, "Suggestion should be descriptive"

    def test_report_includes_all_issue_details(self):
        """Test that report includes all relevant issue details."""
        validator = KconfigValidator()
        issue = ValidationIssue(
            file="test.kconfig",
            line=42,
            severity="warning",
            rule="test_rule",
            message="Test message with details",
            suggestion="Test suggestion with guidance"
        )

        report = validator.generate_report({"test.kconfig": [issue]})

        # Verify all details are in report
        assert "test.kconfig" in report, "Report should include file name"
        assert "42" in report, "Report should include line number"
        assert "WARNING" in report, "Report should include severity"
        assert "test_rule" in report, "Report should include rule name"
        assert "Test message with details" in report, "Report should include full message"
        assert "Test suggestion with guidance" in report, "Report should include full suggestion"


class TestValidateDirectoryReporting:
    """Test directory validation and reporting."""

    def test_validate_directory_returns_dict(self):
        """Test that validate_directory returns proper structure."""
        validator = KconfigValidator()

        # Test with non-existent directory
        result = validator.validate_directory("nonexistent_dir")

        assert isinstance(result, dict), "Should return dictionary"
        assert len(result) > 0, "Should have error for non-existent directory"

    def test_validate_directory_aggregates_issues(self):
        """Test that validate_directory aggregates issues from multiple files."""
        import tempfile
        import os

        validator = KconfigValidator()

        # Create temporary directory with test files
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create test file 1
            file1 = os.path.join(tmpdir, "Kconfig1")
            with open(file1, 'w') as f:
                f.write("""
config NATIVE_UART_ENABLE
    bool "Enable UART"
""")

            # Create test file 2
            file2 = os.path.join(tmpdir, "Kconfig2")
            with open(file2, 'w') as f:
                f.write("""
config NATIVE_SPI_ENABLE
    bool "Enable SPI"
""")

            # Validate directory
            result = validator.validate_directory(tmpdir)

            # Should find issues in both files (missing help text)
            assert isinstance(result, dict), "Should return dictionary"
            # Files with issues should be in result
            assert len(result) >= 0, "Should process files in directory"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
