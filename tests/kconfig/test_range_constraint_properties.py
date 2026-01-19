"""
\file            test_range_constraint_properties.py
\brief           Property-based tests for range constraint validation
\author          Nexus Team

Feature: kconfig-architecture-enhancement
Property 8: Range constraint validation

Validates: Requirements 5.5, 7.2, 7.5
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
def int_config_with_range(draw):
    """Generate an integer config with range and default value"""
    sym = draw(symbol_name())
    min_val = draw(st.integers(min_value=0, max_value=1000))
    max_val = draw(st.integers(min_value=min_val + 1, max_value=min_val + 1000))
    default_val = draw(st.integers(min_value=min_val, max_value=max_val))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    range {min_val} {max_val}")
    kconfig_lines.append(f"    default {default_val}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, min_val, max_val, default_val


@settings(max_examples=100)
@given(config=int_config_with_range())
def test_property_8_valid_range_constraint(config):
    """
    Property 8: Range constraint validation

    For any integer configuration option with a range constraint,
    if the default value is within the range, validation should succeed.

    Validates: Requirements 5.5, 7.2, 7.5
    """
    content, sym, min_val, max_val, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify symbol was parsed with correct range
        assert sym in validator.symbols
        assert validator.symbols[sym]['range'] == (min_val, max_val)
        assert validator.symbols[sym]['default'] == str(default_val)


@st.composite
def int_config_with_invalid_default(draw):
    """Generate an integer config with default value outside range"""
    sym = draw(symbol_name())
    min_val = draw(st.integers(min_value=100, max_value=500))
    max_val = draw(st.integers(min_value=min_val + 100, max_value=min_val + 500))

    # Generate default value outside range
    if draw(st.booleans()):
        # Below minimum
        default_val = draw(st.integers(min_value=0, max_value=min_val - 1))
    else:
        # Above maximum
        default_val = draw(st.integers(min_value=max_val + 1, max_value=max_val + 1000))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    range {min_val} {max_val}")
    kconfig_lines.append(f"    default {default_val}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, min_val, max_val, default_val


@settings(max_examples=100)
@given(config=int_config_with_invalid_default())
def test_property_8_invalid_range_constraint(config):
    """
    Property 8: Range constraint validation

    For any integer configuration option with a range constraint,
    if the default value is outside the range, validation should fail.

    Validates: Requirements 5.5, 7.2, 7.5
    """
    content, sym, min_val, max_val, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should fail due to out-of-range default
        assert not success, "Expected validation to fail due to out-of-range default"

        # Should have error about out-of-range value
        assert any('out of range' in e.lower() for e in validator.errors), \
            f"Expected error about out-of-range value, got: {validator.errors}"

        # Error should mention the symbol
        assert any(sym in e for e in validator.errors), \
            f"Expected error to mention symbol {sym}"


@st.composite
def hex_config_with_range(draw):
    """Generate a hex config with range and default value"""
    sym = draw(symbol_name())
    min_val = draw(st.integers(min_value=0x1000, max_value=0x8000))
    max_val = draw(st.integers(min_value=min_val + 0x1000, max_value=min_val + 0x8000))
    default_val = draw(st.integers(min_value=min_val, max_value=max_val))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    hex \"Test hex config\"")
    kconfig_lines.append(f"    range 0x{min_val:x} 0x{max_val:x}")
    kconfig_lines.append(f"    default 0x{default_val:x}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, min_val, max_val, default_val


@settings(max_examples=100)
@given(config=hex_config_with_range())
def test_property_8_hex_range_constraint(config):
    """
    Property 8: Range constraint validation

    For any hex configuration option with a range constraint,
    if the default value is within the range, validation should succeed.

    Validates: Requirements 5.5, 7.2, 7.5
    """
    content, sym, min_val, max_val, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify symbol was parsed
        assert sym in validator.symbols
        assert validator.symbols[sym]['type'] == 'hex'


@st.composite
def int_config_without_range(draw):
    """Generate an integer config without range constraint"""
    sym = draw(symbol_name())
    default_val = draw(st.integers(min_value=-1000, max_value=10000))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    default {default_val}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, default_val


@settings(max_examples=100)
@given(config=int_config_without_range())
def test_property_8_no_range_constraint(config):
    """
    Property 8: Range constraint validation

    For any integer configuration option without a range constraint,
    any default value should be accepted.

    Validates: Requirements 5.5, 7.2, 7.5
    """
    content, sym, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify symbol was parsed
        assert sym in validator.symbols
        assert validator.symbols[sym]['range'] is None


@st.composite
def int_config_with_boundary_values(draw):
    """Generate an integer config with boundary values"""
    sym = draw(symbol_name())
    min_val = draw(st.integers(min_value=0, max_value=1000))
    max_val = draw(st.integers(min_value=min_val + 10, max_value=min_val + 1000))

    # Test boundary values: min, max, or middle
    boundary_choice = draw(st.sampled_from(['min', 'max', 'middle']))
    if boundary_choice == 'min':
        default_val = min_val
    elif boundary_choice == 'max':
        default_val = max_val
    else:
        default_val = (min_val + max_val) // 2

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    range {min_val} {max_val}")
    kconfig_lines.append(f"    default {default_val}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, min_val, max_val, default_val


@settings(max_examples=100)
@given(config=int_config_with_boundary_values())
def test_property_8_boundary_values(config):
    """
    Property 8: Range constraint validation

    For any integer configuration option with a range constraint,
    boundary values (min, max) should be accepted.

    Validates: Requirements 5.5, 7.2, 7.5
    """
    content, sym, min_val, max_val, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify the default value is within range
        assert min_val <= default_val <= max_val


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
