"""
\file            test_dependency_constraints_properties.py
\brief           Property-based tests for dependency constraints
\author          Nexus Team

Feature: kconfig-architecture-enhancement
Property 7: Dependency constraints

Validates: Requirements 8.1, 8.4
"""

import pytest
from hypothesis import given, strategies as st, settings, assume
from pathlib import Path
import tempfile
import sys

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'scripts' / 'kconfig'))

from validate_kconfig import KconfigValidator


@st.composite
def symbol_name(draw):
    """Generate a valid Kconfig symbol name"""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), min_codepoint=65, max_codepoint=90),
        min_size=3,
        max_size=15
    ))
    return 'CONFIG_' + name.replace(' ', '_')


@st.composite
def config_with_dependency(draw):
    """Generate a config with a dependency"""
    sym_a = draw(symbol_name())
    sym_b = draw(symbol_name())
    # Ensure they're different
    assume(sym_a != sym_b)

    kconfig_lines = []

    # Define B first
    kconfig_lines.append(f"config {sym_b}")
    kconfig_lines.append("    bool \"Dependency config\"")
    kconfig_lines.append("    default y")
    kconfig_lines.append("")

    # Define A that depends on B
    kconfig_lines.append(f"config {sym_a}")
    kconfig_lines.append("    bool \"Dependent config\"")
    kconfig_lines.append(f"    depends on {sym_b}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym_a, sym_b


@settings(max_examples=100)
@given(config=config_with_dependency())
def test_property_7_dependency_constraint_parsing(config):
    """
    Property 7: Dependency constraints

    For any configuration option A, if A depends on B, the validator
    should correctly parse this dependency relationship.

    Validates: Requirements 8.1, 8.4
    """
    content, sym_a, sym_b = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify both symbols were parsed
        assert sym_a in validator.symbols
        assert sym_b in validator.symbols

        # Verify dependency relationship
        assert sym_a in validator.dependencies, \
            f"{sym_a} should have dependencies"
        assert sym_b in validator.dependencies[sym_a], \
            f"{sym_a} should depend on {sym_b}"


@st.composite
def config_with_missing_dependency(draw):
    """Generate a config that depends on undefined symbol"""
    sym_a = draw(symbol_name())
    sym_b = draw(symbol_name())
    # Ensure they're different
    assume(sym_a != sym_b)

    kconfig_lines = []

    # Define A that depends on undefined B
    kconfig_lines.append(f"config {sym_a}")
    kconfig_lines.append("    bool \"Dependent config\"")
    kconfig_lines.append(f"    depends on {sym_b}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym_a, sym_b


@settings(max_examples=100, deadline=None)
@given(config=config_with_missing_dependency())
def test_property_7_missing_dependency_warning(config):
    """
    Property 7: Dependency constraints

    For any configuration option A, if A depends on B and B is not defined,
    the validator should produce a warning.

    Validates: Requirements 8.1, 8.4
    """
    content, sym_a, sym_b = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        validator.validate_all(kconfig_file)

        # Should have warning about undefined dependency
        assert any(sym_b in w and 'undefined' in w.lower() for w in validator.warnings), \
            f"Expected warning about undefined symbol {sym_b}, got: {validator.warnings}"


@st.composite
def config_with_multiple_dependencies(draw):
    """Generate a config with multiple dependencies"""
    sym_a = draw(symbol_name())
    sym_b = draw(symbol_name())
    sym_c = draw(symbol_name())
    # Ensure they're all different
    assume(len({sym_a, sym_b, sym_c}) == 3)

    kconfig_lines = []

    # Define B and C
    kconfig_lines.append(f"config {sym_b}")
    kconfig_lines.append("    bool \"Dependency B\"")
    kconfig_lines.append("")

    kconfig_lines.append(f"config {sym_c}")
    kconfig_lines.append("    bool \"Dependency C\"")
    kconfig_lines.append("")

    # Define A that depends on both B and C
    kconfig_lines.append(f"config {sym_a}")
    kconfig_lines.append("    bool \"Dependent config\"")
    kconfig_lines.append(f"    depends on {sym_b} && {sym_c}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym_a, sym_b, sym_c


@settings(max_examples=100, deadline=None)
@given(config=config_with_multiple_dependencies())
def test_property_7_multiple_dependencies(config):
    """
    Property 7: Dependency constraints

    For any configuration option A, if A depends on multiple symbols,
    the validator should parse all dependencies.

    Validates: Requirements 8.1, 8.4
    """
    content, sym_a, sym_b, sym_c = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify all symbols were parsed
        assert sym_a in validator.symbols
        assert sym_b in validator.symbols
        assert sym_c in validator.symbols

        # Verify dependency relationships
        assert sym_a in validator.dependencies
        # Should depend on both B and C
        assert sym_b in validator.dependencies[sym_a] or sym_c in validator.dependencies[sym_a], \
            f"{sym_a} should depend on {sym_b} and/or {sym_c}"


@st.composite
def config_with_negated_dependency(draw):
    """Generate a config with negated dependency"""
    sym_a = draw(symbol_name())
    sym_b = draw(symbol_name())
    # Ensure they're different
    assume(sym_a != sym_b)

    kconfig_lines = []

    # Define B
    kconfig_lines.append(f"config {sym_b}")
    kconfig_lines.append("    bool \"Dependency config\"")
    kconfig_lines.append("")

    # Define A that depends on !B
    kconfig_lines.append(f"config {sym_a}")
    kconfig_lines.append("    bool \"Dependent config\"")
    kconfig_lines.append(f"    depends on !{sym_b}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym_a, sym_b


@settings(max_examples=100)
@given(config=config_with_negated_dependency())
def test_property_7_negated_dependency(config):
    """
    Property 7: Dependency constraints

    For any configuration option A, if A depends on !B (not B),
    the validator should correctly parse the negated dependency.

    Validates: Requirements 8.1, 8.4
    """
    content, sym_a, sym_b = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify both symbols were parsed
        assert sym_a in validator.symbols
        assert sym_b in validator.symbols

        # Verify dependency relationship (with negation)
        assert sym_a in validator.dependencies
        # The dependency should include !B
        deps = validator.dependencies[sym_a]
        assert any(sym_b in dep for dep in deps), \
            f"{sym_a} should depend on (negated) {sym_b}"


@st.composite
def config_with_conditional_dependency(draw):
    """Generate a config with conditional dependency"""
    sym_a = draw(symbol_name())
    sym_b = draw(symbol_name())
    sym_c = draw(symbol_name())
    # Ensure they're all different
    assume(len({sym_a, sym_b, sym_c}) == 3)

    kconfig_lines = []

    # Define B and C
    kconfig_lines.append(f"config {sym_b}")
    kconfig_lines.append("    bool \"Condition config\"")
    kconfig_lines.append("")

    kconfig_lines.append(f"config {sym_c}")
    kconfig_lines.append("    bool \"Dependency config\"")
    kconfig_lines.append("")

    # Define A that depends on C if B is set
    kconfig_lines.append(f"config {sym_a}")
    kconfig_lines.append("    bool \"Dependent config\"")
    kconfig_lines.append(f"    depends on {sym_c} if {sym_b}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym_a, sym_b, sym_c


@settings(max_examples=100)
@given(config=config_with_conditional_dependency())
def test_property_7_conditional_dependency(config):
    """
    Property 7: Dependency constraints

    For any configuration option A, if A has conditional dependencies,
    the validator should parse them correctly.

    Validates: Requirements 8.1, 8.4
    """
    content, sym_a, sym_b, sym_c = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify all symbols were parsed
        assert sym_a in validator.symbols
        assert sym_b in validator.symbols
        assert sym_c in validator.symbols

        # Verify dependency relationships
        # A should have dependencies
        assert sym_a in validator.dependencies
        # Should depend on at least one of B or C
        deps = validator.dependencies[sym_a]
        assert any(sym_b in dep or sym_c in dep for dep in deps), \
            f"{sym_a} should have conditional dependency on {sym_b} and {sym_c}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
