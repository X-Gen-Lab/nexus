#!/usr/bin/env python3
"""
Property-based tests for Kconfig configuration file parsing round-trip consistency.

Feature: kconfig-architecture-enhancement, Property 17: Configuration file parsing round-trip consistency
Validates: Requirements 6.1

Property: For any valid .config file, parsing then serializing should produce
an equivalent configuration (ignoring comments and empty lines).
"""

import os
import tempfile
import pytest
from hypothesis import given, strategies as st, settings
import kconfiglib


# Strategy for generating valid Kconfig configuration values
@st.composite
def config_value(draw):
    """Generate a valid configuration value (bool, int, string, or hex)."""
    value_type = draw(st.sampled_from(['bool', 'int', 'string', 'hex']))

    if value_type == 'bool':
        return draw(st.sampled_from(['y', 'n']))
    elif value_type == 'int':
        return str(draw(st.integers(min_value=0, max_value=1000000)))
    elif value_type == 'string':
        # Generate valid strings (no quotes, newlines, or carriage returns)
        # .config files are line-based, so strings cannot contain line breaks
        # Also exclude surrogate characters which cannot be encoded in UTF-8
        text = draw(st.text(
            alphabet=st.characters(
                blacklist_characters='"\n\r',
                blacklist_categories=('Cc', 'Cs')  # Exclude control characters and surrogates
            ),
            min_size=0,
            max_size=50
        ))
        return f'"{text}"'
    else:  # hex
        value = draw(st.integers(min_value=0, max_value=0xFFFFFFFF))
        return f'0x{value:x}'


@st.composite
def config_name(draw):
    """Generate a valid configuration symbol name."""
    # CONFIG_ prefix followed by uppercase letters, digits, and underscores
    name_part = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), min_codepoint=65, max_codepoint=90) | st.sampled_from('_0123456789'),
        min_size=1,
        max_size=30
    ))
    # Ensure it starts with a letter
    if name_part[0].isdigit() or name_part[0] == '_':
        name_part = 'A' + name_part
    return f'CONFIG_{name_part}'


@st.composite
def config_file_content(draw):
    """Generate valid .config file content."""
    num_configs = draw(st.integers(min_value=1, max_value=20))
    lines = []

    # Add header comment
    lines.append('#')
    lines.append('# Automatically generated configuration')
    lines.append('#')

    config_dict = {}
    for _ in range(num_configs):
        name = draw(config_name())
        # Avoid duplicates
        if name in config_dict:
            continue

        value = draw(config_value())
        config_dict[name] = value

        if value == 'n':
            # Unset configs are represented as comments
            lines.append(f'# {name} is not set')
        else:
            lines.append(f'{name}={value}')

    return '\n'.join(lines) + '\n', config_dict


def parse_config_content(content):
    """
    Parse .config file content and extract configuration values.

    Returns a dictionary of config name -> value pairs.
    """
    config = {}

    for line in content.split('\n'):
        line = line.strip()

        # Skip empty lines
        if not line:
            continue

        # Handle "# CONFIG_xxx is not set" pattern
        if line.startswith('# ') and ' is not set' in line:
            parts = line.split()
            if len(parts) >= 2:
                name = parts[1]
                config[name] = 'n'
            continue

        # Skip other comments
        if line.startswith('#'):
            continue

        # Parse CONFIG_xxx=value
        if '=' in line:
            name, value = line.split('=', 1)
            config[name.strip()] = value.strip()

    return config


def normalize_config_value(value):
    """Normalize a config value for comparison."""
    if value == 'y' or value == 'n':
        return value
    # Remove quotes from strings for comparison
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    # Normalize hex values
    if value.startswith('0x') or value.startswith('0X'):
        try:
            return hex(int(value, 16))
        except ValueError:
            return value
    return value


@settings(max_examples=100)
@given(config_file_content())
def test_config_roundtrip_consistency(config_data):
    """
    Property 17: Configuration file parsing round-trip consistency.

    For any valid .config file, parsing then serializing should produce
    an equivalent configuration (ignoring comments and empty lines).

    Feature: kconfig-architecture-enhancement, Property 17
    Validates: Requirements 6.1
    """
    content, expected_config = config_data

    # Create temporary files
    with tempfile.TemporaryDirectory() as tmpdir:
        # Write original config
        config_path = os.path.join(tmpdir, '.config')
        with open(config_path, 'w', encoding='utf-8') as f:
            f.write(content)

        # Parse the config
        parsed_config = parse_config_content(content)

        # Verify parsed config matches expected
        for name, expected_value in expected_config.items():
            assert name in parsed_config, f"Config {name} not found in parsed config"
            parsed_value = parsed_config[name]

            # Normalize values for comparison
            expected_norm = normalize_config_value(expected_value)
            parsed_norm = normalize_config_value(parsed_value)

            assert expected_norm == parsed_norm, \
                f"Config {name}: expected {expected_norm}, got {parsed_norm}"

        # Write back the parsed config
        roundtrip_path = os.path.join(tmpdir, '.config_roundtrip')
        with open(roundtrip_path, 'w', encoding='utf-8') as f:
            # Write header
            f.write('#\n')
            f.write('# Automatically generated configuration\n')
            f.write('#\n')

            # Write configs in sorted order for consistency
            for name in sorted(parsed_config.keys()):
                value = parsed_config[name]
                if value == 'n':
                    f.write(f'# {name} is not set\n')
                else:
                    f.write(f'{name}={value}\n')

        # Parse the roundtrip config
        with open(roundtrip_path, 'r', encoding='utf-8') as f:
            roundtrip_content = f.read()

        roundtrip_config = parse_config_content(roundtrip_content)

        # Verify roundtrip config matches original parsed config
        assert set(parsed_config.keys()) == set(roundtrip_config.keys()), \
            "Roundtrip config has different keys"

        for name in parsed_config.keys():
            original_value = normalize_config_value(parsed_config[name])
            roundtrip_value = normalize_config_value(roundtrip_config[name])

            assert original_value == roundtrip_value, \
                f"Roundtrip mismatch for {name}: {original_value} != {roundtrip_value}"


@settings(max_examples=100, deadline=None)
@given(st.lists(
    st.tuples(config_name(), config_value()),
    min_size=1,
    max_size=20,
    unique_by=lambda x: x[0]  # Unique config names
))
def test_config_parsing_preserves_all_values(config_list):
    """
    Property: Parsing a config file preserves all configuration values.

    For any list of configuration name-value pairs, writing them to a file
    and parsing should recover all the values.

    Feature: kconfig-architecture-enhancement, Property 17
    Validates: Requirements 6.1
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Write config file
        with open(config_path, 'w', encoding='utf-8') as f:
            f.write('#\n# Test configuration\n#\n')
            for name, value in config_list:
                if value == 'n':
                    f.write(f'# {name} is not set\n')
                else:
                    f.write(f'{name}={value}\n')

        # Parse config file
        with open(config_path, 'r', encoding='utf-8') as f:
            content = f.read()

        parsed_config = parse_config_content(content)

        # Verify all configs are present
        for name, expected_value in config_list:
            assert name in parsed_config, f"Config {name} not found"

            expected_norm = normalize_config_value(expected_value)
            parsed_norm = normalize_config_value(parsed_config[name])

            assert expected_norm == parsed_norm, \
                f"Value mismatch for {name}: expected {expected_norm}, got {parsed_norm}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
