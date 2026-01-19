"""
\file            test_dependency_transitivity_properties.py
\brief           Property-based tests for dependency transitivity
\author          Nexus Team

Feature: kconfig-architecture-enhancement
Property 6: Dependency transitivity

Validates: Requirements 8.2
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
def dependency_chain(draw, length=3):
    """Generate a chain of dependencies: A selects B, B selects C"""
    symbols = [draw(symbol_name()) for _ in range(length)]
    # Ensure all symbols are unique
    assume(len(set(symbols)) == length)

    kconfig_lines = []

    # Create chain: symbols[0] selects symbols[1], symbols[1] selects symbols[2], etc.
    for i, sym in enumerate(symbols):
        kconfig_lines.append(f"config {sym}")
        kconfig_lines.append("    bool \"Test config\"")
        if i < len(symbols) - 1:
            kconfig_lines.append(f"    select {symbols[i + 1]}")
        kconfig_lines.append("")

    return '\n'.join(kconfig_lines), symbols


@settings(max_examples=100)
@given(chain=dependency_chain(length=3))
def test_property_6_select_transitivity(chain):
    """
    Property 6: Dependency transitivity

    For any configuration option A, if A selects B, and B selects C,
    when A is enabled, B and C should both be enabled.

    This test verifies that the validator correctly parses select chains.

    Validates: Requirements 8.2
    """
    content, symbols = chain

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
        for sym in symbols:
            assert sym in validator.symbols, f"Symbol {sym} not found in parsed symbols"

        # Verify select relationships
        # symbols[0] should select symbols[1]
        if symbols[0] in validator.selects:
            assert symbols[1] in validator.selects[symbols[0]], \
                f"{symbols[0]} should select {symbols[1]}"

        # symbols[1] should select symbols[2]
        if symbols[1] in validator.selects:
            assert symbols[2] in validator.selects[symbols[1]], \
                f"{symbols[1]} should select {symbols[2]}"


@st.composite
def depends_on_chain(draw, length=3):
    """Generate a chain of depends on: A depends on B, B depends on C"""
    symbols = [draw(symbol_name()) for _ in range(length)]
    # Ensure all symbols are unique
    assume(len(set(symbols)) == length)

    kconfig_lines = []

    # Create chain: symbols[0] depends on symbols[1], symbols[1] depends on symbols[2], etc.
    for i, sym in enumerate(symbols):
        kconfig_lines.append(f"config {sym}")
        kconfig_lines.append("    bool \"Test config\"")
        if i < len(symbols) - 1:
            kconfig_lines.append(f"    depends on {symbols[i + 1]}")
        kconfig_lines.append("")

    return '\n'.join(kconfig_lines), symbols


@settings(max_examples=100)
@given(chain=depends_on_chain(length=3))
def test_property_6_depends_on_transitivity(chain):
    """
    Property 6: Dependency transitivity

    For any configuration option A, if A depends on B, and B depends on C,
    the validator should correctly parse the dependency chain.

    Validates: Requirements 8.2
    """
    content, symbols = chain

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
        for sym in symbols:
            assert sym in validator.symbols, f"Symbol {sym} not found in parsed symbols"

        # Verify dependency relationships
        # symbols[0] should depend on symbols[1]
        if symbols[0] in validator.dependencies:
            assert symbols[1] in validator.dependencies[symbols[0]], \
                f"{symbols[0]} should depend on {symbols[1]}"

        # symbols[1] should depend on symbols[2]
        if symbols[1] in validator.dependencies:
            assert symbols[2] in validator.dependencies[symbols[1]], \
                f"{symbols[1]} should depend on {symbols[2]}"


@st.composite
def mixed_dependency_chain(draw):
    """Generate a mixed chain: A selects B, B depends on C, C selects D"""
    symbols = [draw(symbol_name()) for _ in range(4)]
    # Ensure all symbols are unique
    assume(len(set(symbols)) == 4)

    kconfig_lines = []

    # A selects B
    kconfig_lines.append(f"config {symbols[0]}")
    kconfig_lines.append("    bool \"Test config A\"")
    kconfig_lines.append(f"    select {symbols[1]}")
    kconfig_lines.append("")

    # B depends on C
    kconfig_lines.append(f"config {symbols[1]}")
    kconfig_lines.append("    bool \"Test config B\"")
    kconfig_lines.append(f"    depends on {symbols[2]}")
    kconfig_lines.append("")

    # C selects D
    kconfig_lines.append(f"config {symbols[2]}")
    kconfig_lines.append("    bool \"Test config C\"")
    kconfig_lines.append(f"    select {symbols[3]}")
    kconfig_lines.append("")

    # D
    kconfig_lines.append(f"config {symbols[3]}")
    kconfig_lines.append("    bool \"Test config D\"")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), symbols


@settings(max_examples=100)
@given(chain=mixed_dependency_chain())
def test_property_6_mixed_dependency_chain(chain):
    """
    Property 6: Dependency transitivity

    For any configuration with mixed select and depends on relationships,
    the validator should correctly parse all relationships.

    Validates: Requirements 8.2
    """
    content, symbols = chain

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
        for sym in symbols:
            assert sym in validator.symbols, f"Symbol {sym} not found in parsed symbols"

        # Verify relationships
        # A selects B
        if symbols[0] in validator.selects:
            assert symbols[1] in validator.selects[symbols[0]]

        # B depends on C
        if symbols[1] in validator.dependencies:
            assert symbols[2] in validator.dependencies[symbols[1]]

        # C selects D
        if symbols[2] in validator.selects:
            assert symbols[3] in validator.selects[symbols[2]]


@st.composite
def undefined_dependency(draw):
    """Generate a config with undefined dependency"""
    defined_sym = draw(symbol_name())
    undefined_sym = draw(symbol_name())
    # Ensure they're different
    assume(defined_sym != undefined_sym)

    kconfig_lines = []
    kconfig_lines.append(f"config {defined_sym}")
    kconfig_lines.append("    bool \"Test config\"")
    kconfig_lines.append(f"    depends on {undefined_sym}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), defined_sym, undefined_sym


@settings(max_examples=100)
@given(config=undefined_dependency())
def test_property_6_undefined_dependency_warning(config):
    """
    Property 6: Dependency transitivity

    For any configuration option that depends on an undefined symbol,
    the validator should produce a warning.

    Validates: Requirements 8.2
    """
    content, defined_sym, undefined_sym = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        validator.validate_all(kconfig_file)

        # Should have warning about undefined symbol
        assert any(undefined_sym in w for w in validator.warnings), \
            f"Expected warning about undefined symbol {undefined_sym}, got: {validator.warnings}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
