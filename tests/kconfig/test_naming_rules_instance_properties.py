#!/usr/bin/env python3
r"""
Property-based tests for instance-level naming rules.

Feature: kconfig-naming-standard, Property 1: Naming format consistency (instance-level)
Feature: kconfig-naming-standard, Property 10: GPIO special naming
Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.6, 3.7

Property: For any peripheral name and instance identifier, the generated
configuration symbols should match the corresponding naming pattern regular expressions.
"""

import sys
import os
import pytest
from hypothesis import given, strategies as st, settings, assume

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.naming_rules import NamingRules


# Strategy for generating valid peripheral names
@st.composite
def peripheral_name(draw):
    """Generate a valid peripheral name."""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=20
    ))
    if not name or not name[0].isalpha():
        name = 'P' + name
    return name


# Strategy for generating numeric instance identifiers
@st.composite
def numeric_instance(draw):
    """Generate a numeric instance identifier."""
    return draw(st.integers(min_value=0, max_value=99))


# Strategy for generating alpha instance identifiers (for GPIO)
@st.composite
def alpha_instance(draw):
    """Generate an alpha instance identifier (A-Z)."""
    return draw(st.sampled_from(['A', 'B', 'C', 'D', 'E', 'F']))


# Strategy for generating parameter names
@st.composite
def param_name(draw):
    """Generate a valid parameter name."""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=20
    ))
    if not name or not name[0].isalpha():
        name = 'PARAM' + name
    return name


# Strategy for generating category names
@st.composite
def category_name(draw):
    """Generate a valid category name."""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=20
    ))
    if not name or not name[0].isalpha():
        name = 'CAT' + name
    return name


# Strategy for generating option names
@st.composite
def option_name(draw):
    """Generate a valid option name."""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=20
    ))
    if not name or not name[0].isalpha():
        name = 'OPT' + name
    return name


@settings(max_examples=100)
@given(peripheral_name(), numeric_instance())
def test_instance_enable_naming_pattern_numeric(peripheral, instance):
    """
    Property 1: Instance enable naming format consistency (numeric).

    For any peripheral name and numeric instance, the generated instance
    enable configuration symbol should match the INSTANCE_ENABLE_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 3.1
    """
    # Generate instance enable symbol
    symbol = NamingRules.instance_enable(peripheral, instance)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.INSTANCE_ENABLE_PATTERN), \
        f"Instance enable symbol '{symbol}' does not match pattern {NamingRules.INSTANCE_ENABLE_PATTERN}"

    # Verify it starts with INSTANCE_NX_
    assert symbol.startswith('INSTANCE_NX_'), \
        f"Instance enable symbol '{symbol}' should start with 'INSTANCE_NX_'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Instance enable symbol '{symbol}' should be all uppercase"


@settings(max_examples=100)
@given(alpha_instance())
def test_instance_enable_naming_pattern_gpio(instance):
    """
    Property 10: GPIO special naming for instance enable.

    For any GPIO port letter, the generated instance enable configuration
    symbol should follow the INSTANCE_NX_GPIO{LETTER} pattern.

    Feature: kconfig-naming-standard, Property 10
    Validates: Requirements 3.6
    """
    # Generate GPIO instance enable symbol
    symbol = NamingRules.instance_enable('GPIO', instance)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.INSTANCE_ENABLE_PATTERN), \
        f"GPIO instance enable symbol '{symbol}' does not match pattern {NamingRules.INSTANCE_ENABLE_PATTERN}"

    # Verify it follows GPIO{LETTER} pattern
    expected = f"INSTANCE_NX_GPIO{instance.upper()}"
    assert symbol == expected, \
        f"GPIO instance enable symbol '{symbol}' should be '{expected}'"


@settings(max_examples=100)
@given(peripheral_name(), numeric_instance(), param_name())
def test_instance_param_naming_pattern_numeric(peripheral, instance, param):
    """
    Property 1: Instance parameter naming format consistency (numeric).

    For any peripheral name, numeric instance, and parameter name, the
    generated instance parameter configuration symbol should match the
    INSTANCE_PARAM_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 3.2
    """
    # Generate instance parameter symbol
    symbol = NamingRules.instance_param(peripheral, instance, param)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.INSTANCE_PARAM_PATTERN), \
        f"Instance param symbol '{symbol}' does not match pattern {NamingRules.INSTANCE_PARAM_PATTERN}"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Instance param symbol '{symbol}' should be all uppercase"


@settings(max_examples=100)
@given(alpha_instance(), param_name())
def test_instance_param_naming_pattern_gpio(instance, param):
    """
    Property 10: GPIO special naming for instance parameters.

    For any GPIO port letter and parameter name, the generated instance
    parameter configuration symbol should follow the GPIO{LETTER}_{PARAM} pattern.

    Feature: kconfig-naming-standard, Property 10
    Validates: Requirements 3.7
    """
    # Generate GPIO instance parameter symbol
    symbol = NamingRules.instance_param('GPIO', instance, param)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.INSTANCE_PARAM_PATTERN), \
        f"GPIO instance param symbol '{symbol}' does not match pattern {NamingRules.INSTANCE_PARAM_PATTERN}"

    # Verify it starts with GPIO{LETTER}_
    expected_prefix = f"GPIO{instance.upper()}_"
    assert symbol.startswith(expected_prefix), \
        f"GPIO instance param symbol '{symbol}' should start with '{expected_prefix}'"


@settings(max_examples=100)
@given(peripheral_name(), numeric_instance(), category_name(), option_name())
def test_choice_option_naming_pattern(peripheral, instance, category, option):
    """
    Property 1: Choice option naming format consistency.

    For any peripheral name, instance, category, and option, the generated
    choice option configuration symbol should match the CHOICE_OPTION_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 3.3
    """
    # Generate choice option symbol
    symbol = NamingRules.choice_option(peripheral, instance, category, option)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.CHOICE_OPTION_PATTERN), \
        f"Choice option symbol '{symbol}' does not match pattern {NamingRules.CHOICE_OPTION_PATTERN}"

    # Verify it starts with NX_
    assert symbol.startswith('NX_'), \
        f"Choice option symbol '{symbol}' should start with 'NX_'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Choice option symbol '{symbol}' should be all uppercase"


@settings(max_examples=100)
@given(peripheral_name(), numeric_instance(), category_name())
def test_choice_value_naming_pattern(peripheral, instance, category):
    """
    Property 1: Choice value naming format consistency.

    For any peripheral name, instance, and category, the generated choice
    value configuration symbol should match the CHOICE_VALUE_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 3.4
    """
    # Generate choice value symbol
    symbol = NamingRules.choice_value(peripheral, instance, category)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.CHOICE_VALUE_PATTERN), \
        f"Choice value symbol '{symbol}' does not match pattern {NamingRules.CHOICE_VALUE_PATTERN}"

    # Verify it ends with _VALUE
    assert symbol.endswith('_VALUE'), \
        f"Choice value symbol '{symbol}' should end with '_VALUE'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Choice value symbol '{symbol}' should be all uppercase"


@settings(max_examples=50)
@given(peripheral_name(), numeric_instance())
def test_instance_symbols_contain_instance_number(peripheral, instance):
    """
    Property: Instance symbols contain the instance number.

    For any peripheral and numeric instance, all generated symbols should
    contain the instance number.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 3.1, 3.2, 3.3, 3.4
    """
    instance_str = str(instance)

    # Test instance enable
    enable_symbol = NamingRules.instance_enable(peripheral, instance)
    assert instance_str in enable_symbol, \
        f"Instance enable symbol '{enable_symbol}' should contain instance number '{instance_str}'"

    # Test instance param
    param_symbol = NamingRules.instance_param(peripheral, instance, 'TEST_PARAM')
    assert instance_str in param_symbol, \
        f"Instance param symbol '{param_symbol}' should contain instance number '{instance_str}'"

    # Test choice option
    choice_symbol = NamingRules.choice_option(peripheral, instance, 'MODE', 'OPTION')
    assert instance_str in choice_symbol, \
        f"Choice option symbol '{choice_symbol}' should contain instance number '{instance_str}'"

    # Test choice value
    value_symbol = NamingRules.choice_value(peripheral, instance, 'MODE')
    assert instance_str in value_symbol, \
        f"Choice value symbol '{value_symbol}' should contain instance number '{instance_str}'"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
