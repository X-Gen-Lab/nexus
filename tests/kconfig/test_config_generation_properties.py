#!/usr/bin/env python3
"""
Property-based tests for configuration generation script.

Feature: kconfig-architecture-enhancement, Property 4: Configuration value type conversion correctness
Validates: Requirements 6.2, 6.6, 7.1, 7.2, 7.3, 7.4

Property: For any .config file, the generated C header should contain correct
type conversions:
- CONFIG_XXX=y → #define NX_CONFIG_XXX 1
- CONFIG_XXX=n → /* #undef NX_CONFIG_XXX */
- CONFIG_XXX=123 → #define NX_CONFIG_XXX 123
- CONFIG_XXX="str" → #define NX_CONFIG_XXX "str"
- CONFIG_XXX=0x1000 → #define NX_CONFIG_XXX 0x1000
"""

import os
import tempfile
import re
import pytest
from hypothesis import given, strategies as st, settings


# Import the generation functions
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'kconfig'))
from generate_hal_config import generate_header


@st.composite
def config_entry(draw):
    """Generate a valid configuration entry with name and value."""
    # Generate config name
    name_part = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), min_codepoint=65, max_codepoint=90) | st.sampled_from('_0123456789'),
        min_size=1,
        max_size=20
    ))
    if name_part[0].isdigit() or name_part[0] == '_':
        name_part = 'A' + name_part

    name = f'CONFIG_{name_part}'

    # Generate value of different types
    value_type = draw(st.sampled_from(['bool_true', 'bool_false', 'int', 'string', 'hex']))

    if value_type == 'bool_true':
        value = True
    elif value_type == 'bool_false':
        value = False
    elif value_type == 'int':
        value = draw(st.integers(min_value=0, max_value=1000000))
    elif value_type == 'string':
        # Generate valid strings (no quotes, newlines, control characters, or surrogates)
        # Surrogates (U+D800 to U+DFFF) are not valid in UTF-8
        text = draw(st.text(
            alphabet=st.characters(
                blacklist_characters='"\n\r',
                blacklist_categories=('Cc', 'Cs')  # Exclude control chars and surrogates
            ),
            min_size=0,
            max_size=30
        ))
        value = text
    else:  # hex
        value = draw(st.integers(min_value=0, max_value=0xFFFFFFFF))
        value = f'0x{value:x}'

    return (name, value, value_type)


@settings(max_examples=100)
@given(st.lists(config_entry(), min_size=1, max_size=20, unique_by=lambda x: x[0]))
def test_config_type_conversion_correctness(config_list):
    """
    Property 4: Configuration value type conversion correctness.

    For any configuration entries, the generated header should contain
    correct type conversions according to the specification.

    Feature: kconfig-architecture-enhancement, Property 4
    Validates: Requirements 6.2, 6.6, 7.1, 7.2, 7.3, 7.4
    """
    # Create config dictionary
    config = {}
    for name, value, value_type in config_list:
        config[name] = value

    # Generate header to temporary file
    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = os.path.join(tmpdir, 'test_config.h')
        generate_header(config, output_path)

        # Read generated header
        with open(output_path, 'r', encoding='utf-8') as f:
            header_content = f.read()

        # Verify each configuration entry
        for name, value, value_type in config_list:
            # Convert CONFIG_ to NX_CONFIG_
            nx_name = name.replace('CONFIG_', 'NX_CONFIG_', 1)

            if value_type == 'bool_true':
                # Should have: #define NX_CONFIG_XXX 1
                pattern = rf'#define\s+{re.escape(nx_name)}\s+1'
                assert re.search(pattern, header_content), \
                    f"Expected '#define {nx_name} 1' for bool true value"

                # Should NOT have undef
                undef_pattern = rf'/\*\s*#undef\s+{re.escape(nx_name)}\s*\*/'
                assert not re.search(undef_pattern, header_content), \
                    f"Should not have undef for bool true value"

            elif value_type == 'bool_false':
                # Should have: /* #undef NX_CONFIG_XXX */
                pattern = rf'/\*\s*#undef\s+{re.escape(nx_name)}\s*\*/'
                assert re.search(pattern, header_content), \
                    f"Expected '/* #undef {nx_name} */' for bool false value"

                # Should NOT have define
                define_pattern = rf'#define\s+{re.escape(nx_name)}\s+'
                assert not re.search(define_pattern, header_content), \
                    f"Should not have define for bool false value"

            elif value_type == 'int':
                # Should have: #define NX_CONFIG_XXX <number>
                pattern = rf'#define\s+{re.escape(nx_name)}\s+{value}\b'
                assert re.search(pattern, header_content), \
                    f"Expected '#define {nx_name} {value}' for int value"

            elif value_type == 'string':
                # Should have: #define NX_CONFIG_XXX "string"
                escaped_value = re.escape(value)
                pattern = rf'#define\s+{re.escape(nx_name)}\s+"' + escaped_value + r'"'
                assert re.search(pattern, header_content), \
                    f"Expected '#define {nx_name} \"{value}\"' for string value"

            elif value_type == 'hex':
                # Should have: #define NX_CONFIG_XXX 0x<hex>
                pattern = rf'#define\s+{re.escape(nx_name)}\s+{re.escape(value)}\b'
                assert re.search(pattern, header_content), \
                    f"Expected '#define {nx_name} {value}' for hex value"


@settings(max_examples=100)
@given(st.lists(config_entry(), min_size=1, max_size=10, unique_by=lambda x: x[0]))
def test_config_prefix_conversion(config_list):
    """
    Property: CONFIG_ prefix is correctly converted to NX_CONFIG_.

    For any configuration with CONFIG_ prefix, it should be converted
    to NX_CONFIG_ in the generated header.

    Feature: kconfig-architecture-enhancement, Property 4
    Validates: Requirements 6.2
    """
    # Create config dictionary
    config = {}
    for name, value, value_type in config_list:
        config[name] = value

    # Generate header to temporary file
    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = os.path.join(tmpdir, 'test_config.h')
        generate_header(config, output_path)

        # Read generated header
        with open(output_path, 'r', encoding='utf-8') as f:
            header_content = f.read()

        # Verify prefix conversion
        for name, value, value_type in config_list:
            # Original CONFIG_ should not appear in defines
            if value_type != 'bool_false':  # False values are commented out
                pattern = rf'#define\s+{re.escape(name)}\s+'
                assert not re.search(pattern, header_content), \
                    f"Original CONFIG_ prefix should not appear: {name}"

            # NX_CONFIG_ should appear
            nx_name = name.replace('CONFIG_', 'NX_CONFIG_', 1)
            if value_type == 'bool_false':
                pattern = rf'#undef\s+{re.escape(nx_name)}'
            else:
                pattern = rf'#define\s+{re.escape(nx_name)}\s+'

            assert re.search(pattern, header_content), \
                f"Expected NX_CONFIG_ prefix: {nx_name}"


@settings(max_examples=50)
@given(st.lists(config_entry(), min_size=5, max_size=15, unique_by=lambda x: x[0]))
def test_header_structure_completeness(config_list):
    """
    Property: Generated header has complete structure.

    For any configuration, the generated header should have:
    - Include guards
    - File header comment
    - C++ extern "C" wrapper
    - Proper formatting

    Feature: kconfig-architecture-enhancement, Property 4
    Validates: Requirements 6.3, 6.4, 6.5
    """
    # Create config dictionary
    config = {}
    for name, value, value_type in config_list:
        config[name] = value

    # Generate header to temporary file
    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = os.path.join(tmpdir, 'test_config.h')
        generate_header(config, output_path)

        # Read generated header
        with open(output_path, 'r', encoding='utf-8') as f:
            header_content = f.read()

        # Check include guards
        assert '#ifndef NEXUS_CONFIG_H' in header_content, \
            "Missing include guard #ifndef"
        assert '#define NEXUS_CONFIG_H' in header_content, \
            "Missing include guard #define"
        assert '#endif /* NEXUS_CONFIG_H */' in header_content, \
            "Missing include guard #endif"

        # Check file header comment
        assert '\\file' in header_content, "Missing \\file in header comment"
        assert '\\brief' in header_content, "Missing \\brief in header comment"
        assert '\\author' in header_content, "Missing \\author in header comment"
        assert 'auto-generated' in header_content.lower(), \
            "Missing auto-generated warning"
        assert 'Generated:' in header_content, "Missing generation timestamp"

        # Check C++ wrapper
        assert '#ifdef __cplusplus' in header_content, \
            "Missing C++ wrapper #ifdef"
        assert 'extern "C" {' in header_content, \
            "Missing extern C opening"
        assert '#endif' in header_content, "Missing C++ wrapper #endif"

        # Check that opening and closing braces match
        opening_count = header_content.count('extern "C" {')
        closing_count = header_content.count('}')
        # Should have at least one closing brace for extern "C"
        assert closing_count >= opening_count, \
            "Mismatched extern C braces"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
