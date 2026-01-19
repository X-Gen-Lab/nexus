"""
\file            test_default_range_consistency_properties.py
\brief           Property-based tests for default value range consistency
\author          Nexus Team

Feature: kconfig-architecture-enhancement
Property 21: Default value range consistency

Validates: Requirements 10.5
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
def consistent_int_config(draw):
    """Generate an integer config with consistent default and range"""
    sym = draw(symbol_name())
    min_val = draw(st.integers(min_value=0, max_value=1000))
    max_val = draw(st.integers(min_value=min_val + 1, max_value=min_val + 1000))
    # Default is always within range
    default_val = draw(st.integers(min_value=min_val, max_value=max_val))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    range {min_val} {max_val}")
    kconfig_lines.append(f"    default {default_val}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, min_val, max_val, default_val


@settings(max_examples=100)
@given(config=consistent_int_config())
def test_property_21_consistent_default_range(config):
    """
    Property 21: Default value range consistency

    For any configuration option with a range constraint,
    the default value should be within the defined range.

    Validates: Requirements 10.5
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

        # Verify the parsed values are consistent
        assert sym in validator.symbols
        parsed_range = validator.symbols[sym]['range']
        parsed_default = validator.symbols[sym]['default']

        # Verify range was parsed correctly
        assert parsed_range == (min_val, max_val)

        # Verify default is within range
        assert min_val <= default_val <= max_val


@st.composite
def inconsistent_int_config(draw):
    """Generate an integer config with inconsistent default and range"""
    sym = draw(symbol_name())
    min_val = draw(st.integers(min_value=100, max_value=500))
    max_val = draw(st.integers(min_value=min_val + 100, max_value=min_val + 500))

    # Default is always outside range
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
@given(config=inconsistent_int_config())
def test_property_21_inconsistent_default_range_error(config):
    """
    Property 21: Default value range consistency

    For any configuration option with a range constraint,
    if the default value is outside the range, validation should fail.

    Validates: Requirements 10.5
    """
    content, sym, min_val, max_val, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should fail due to inconsistent default
        assert not success, "Expected validation to fail due to inconsistent default"

        # Should have error about out-of-range default
        assert any('out of range' in e.lower() for e in validator.errors), \
            f"Expected error about out-of-range default, got: {validator.errors}"

        # Error should mention the symbol and the range
        range_errors = [e for e in validator.errors if 'out of range' in e.lower()]
        assert any(sym in e for e in range_errors), \
            f"Expected error to mention symbol {sym}"
        assert any(str(min_val) in e and str(max_val) in e for e in range_errors), \
            f"Expected error to mention range [{min_val}, {max_val}]"


@st.composite
def multiple_configs_with_ranges(draw):
    """Generate multiple configs with various range consistencies"""
    configs = []

    # Generate 2-5 configs
    num_configs = draw(st.integers(min_value=2, max_value=5))

    for i in range(num_configs):
        sym = draw(symbol_name())
        min_val = draw(st.integers(min_value=10, max_value=1000))  # Start from 10 to allow below-range values
        max_val = draw(st.integers(min_value=min_val + 10, max_value=min_val + 1000))

        # Some configs have consistent defaults, some don't
        if draw(st.booleans()):
            # Consistent
            default_val = draw(st.integers(min_value=min_val, max_value=max_val))
            is_consistent = True
        else:
            # Inconsistent
            if draw(st.booleans()):
                default_val = draw(st.integers(min_value=0, max_value=min_val - 1))
            else:
                default_val = draw(st.integers(min_value=max_val + 1, max_value=max_val + 1000))
            is_consistent = False

        configs.append((sym, min_val, max_val, default_val, is_consistent))

    # Ensure all symbols are unique
    symbols = [c[0] for c in configs]
    assume(len(set(symbols)) == len(symbols))

    # Generate Kconfig content
    kconfig_lines = []
    for sym, min_val, max_val, default_val, _ in configs:
        kconfig_lines.append(f"config {sym}")
        kconfig_lines.append("    int \"Test integer config\"")
        kconfig_lines.append(f"    range {min_val} {max_val}")
        kconfig_lines.append(f"    default {default_val}")
        kconfig_lines.append("")

    return '\n'.join(kconfig_lines), configs


@settings(max_examples=100)
@given(data=multiple_configs_with_ranges())
def test_property_21_multiple_configs_consistency(data):
    """
    Property 21: Default value range consistency

    For any set of configuration options with range constraints,
    validation should detect all inconsistent defaults.

    Validates: Requirements 10.5
    """
    content, configs = data

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Count inconsistent configs
        inconsistent_count = sum(1 for _, _, _, _, is_consistent in configs if not is_consistent)

        if inconsistent_count > 0:
            # Should fail if any config is inconsistent
            assert not success, "Expected validation to fail due to inconsistent defaults"

            # Should have errors for inconsistent configs
            assert len(validator.errors) >= inconsistent_count, \
                f"Expected at least {inconsistent_count} errors, got {len(validator.errors)}"
        else:
            # Should succeed if all configs are consistent
            assert success, f"Expected validation to succeed, got errors: {validator.errors}"


@st.composite
def config_with_zero_range(draw):
    """Generate a config with zero-width range (min == max)"""
    sym = draw(symbol_name())
    value = draw(st.integers(min_value=0, max_value=1000))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    range {value} {value}")
    kconfig_lines.append(f"    default {value}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, value


@settings(max_examples=100)
@given(config=config_with_zero_range())
def test_property_21_zero_width_range(config):
    """
    Property 21: Default value range consistency

    For any configuration option with a zero-width range (min == max),
    the default value must equal that value.

    Validates: Requirements 10.5
    """
    content, sym, value = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify the parsed values
        assert sym in validator.symbols
        assert validator.symbols[sym]['range'] == (value, value)
        assert validator.symbols[sym]['default'] == str(value)


@st.composite
def config_with_large_range(draw):
    """Generate a config with a large range"""
    sym = draw(symbol_name())
    min_val = 0
    max_val = draw(st.integers(min_value=10000, max_value=1000000))
    default_val = draw(st.integers(min_value=min_val, max_value=max_val))

    kconfig_lines = []
    kconfig_lines.append(f"config {sym}")
    kconfig_lines.append("    int \"Test integer config\"")
    kconfig_lines.append(f"    range {min_val} {max_val}")
    kconfig_lines.append(f"    default {default_val}")
    kconfig_lines.append("")

    return '\n'.join(kconfig_lines), sym, min_val, max_val, default_val


@settings(max_examples=100, deadline=None)
@given(config=config_with_large_range())
def test_property_21_large_range_consistency(config):
    """
    Property 21: Default value range consistency

    For any configuration option with a large range,
    the default value should still be validated correctly.

    Validates: Requirements 10.5
    """
    content, sym, min_val, max_val, default_val = config

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed
        assert success, f"Validation failed with errors: {validator.errors}"

        # Verify the default is within range
        assert min_val <= default_val <= max_val


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
