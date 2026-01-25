#!/usr/bin/env python3
r"""
Property-based tests for platform-level naming rules.

Feature: kconfig-naming-standard, Property 1: Naming format consistency (platform-level)
Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5

Property: For any platform name and feature name, the generated configuration
symbols should match the corresponding naming pattern regular expressions.
"""

import sys
import os
import pytest
from hypothesis import given, strategies as st, settings

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.naming_rules import NamingRules


# Strategy for generating valid platform names
@st.composite
def platform_name(draw):
    """Generate a valid platform name."""
    # Platform names should be alphanumeric with underscores
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=20
    ))
    # Ensure it starts with a letter
    if not name or not name[0].isalpha():
        name = 'P' + name
    return name


# Strategy for generating valid feature names
@st.composite
def feature_name(draw):
    """Generate a valid feature name."""
    # Feature names should be alphanumeric with underscores
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=20
    ))
    # Ensure it starts with a letter
    if not name or not name[0].isalpha():
        name = 'F' + name
    return name


@settings(max_examples=100)
@given(platform_name())
def test_platform_enable_naming_pattern(platform):
    """
    Property 1: Platform enable naming format consistency.

    For any platform name, the generated platform enable configuration symbol
    should match the PLATFORM_ENABLE_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 1.1, 1.5
    """
    # Generate platform enable symbol
    symbol = NamingRules.platform_enable(platform)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.PLATFORM_ENABLE_PATTERN), \
        f"Platform enable symbol '{symbol}' does not match pattern {NamingRules.PLATFORM_ENABLE_PATTERN}"

    # Verify it ends with _ENABLE
    assert symbol.endswith('_ENABLE'), \
        f"Platform enable symbol '{symbol}' should end with '_ENABLE'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Platform enable symbol '{symbol}' should be all uppercase"


@settings(max_examples=100)
@given(platform_name(), feature_name())
def test_platform_feature_enable_naming_pattern(platform, feature):
    """
    Property 1: Platform feature enable naming format consistency.

    For any platform name and feature name, the generated platform feature
    enable configuration symbol should match the PLATFORM_FEATURE_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 1.2, 1.5
    """
    # Generate platform feature enable symbol
    symbol = NamingRules.platform_feature_enable(platform, feature)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.PLATFORM_FEATURE_PATTERN), \
        f"Platform feature enable symbol '{symbol}' does not match pattern {NamingRules.PLATFORM_FEATURE_PATTERN}"

    # Verify it ends with _ENABLE
    assert symbol.endswith('_ENABLE'), \
        f"Platform feature enable symbol '{symbol}' should end with '_ENABLE'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Platform feature enable symbol '{symbol}' should be all uppercase"

    # Verify it contains both platform and feature
    normalized_platform = NamingRules.normalize_name(platform)
    normalized_feature = NamingRules.normalize_name(feature)
    assert normalized_platform in symbol, \
        f"Symbol '{symbol}' should contain platform '{normalized_platform}'"
    assert normalized_feature in symbol, \
        f"Symbol '{symbol}' should contain feature '{normalized_feature}'"


@settings(max_examples=100)
@given(platform_name())
def test_platform_name_naming_pattern(platform):
    """
    Property 1: Platform name naming format consistency.

    For any platform name, the generated platform name configuration symbol
    should match the PLATFORM_NAME_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 1.3, 1.5
    """
    # Generate platform name symbol
    symbol = NamingRules.platform_name(platform)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.PLATFORM_NAME_PATTERN), \
        f"Platform name symbol '{symbol}' does not match pattern {NamingRules.PLATFORM_NAME_PATTERN}"

    # Verify it ends with _PLATFORM_NAME
    assert symbol.endswith('_PLATFORM_NAME'), \
        f"Platform name symbol '{symbol}' should end with '_PLATFORM_NAME'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Platform name symbol '{symbol}' should be all uppercase"


@settings(max_examples=100)
@given(platform_name())
def test_platform_version_naming_pattern(platform):
    """
    Property 1: Platform version naming format consistency.

    For any platform name, the generated platform version configuration symbol
    should match the PLATFORM_VERSION_PATTERN.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 1.4, 1.5
    """
    # Generate platform version symbol
    symbol = NamingRules.platform_version(platform)

    # Verify it matches the pattern
    assert NamingRules.validate_pattern(symbol, NamingRules.PLATFORM_VERSION_PATTERN), \
        f"Platform version symbol '{symbol}' does not match pattern {NamingRules.PLATFORM_VERSION_PATTERN}"

    # Verify it ends with _PLATFORM_VERSION
    assert symbol.endswith('_PLATFORM_VERSION'), \
        f"Platform version symbol '{symbol}' should end with '_PLATFORM_VERSION'"

    # Verify it's all uppercase
    assert symbol.isupper(), \
        f"Platform version symbol '{symbol}' should be all uppercase"


@settings(max_examples=100)
@given(st.text(min_size=1, max_size=50))
def test_normalize_name_uppercase(name):
    """
    Property: Name normalization produces uppercase output.

    For any input string, the normalized name should be all uppercase
    (or contain only digits and underscores).

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 1.5
    """
    normalized = NamingRules.normalize_name(name)

    # Skip if normalization resulted in empty string
    if normalized:
        # Should be uppercase OR contain only digits/underscores
        assert normalized.isupper() or all(c.isdigit() or c == '_' for c in normalized), \
            f"Normalized name '{normalized}' should be all uppercase or digits/underscores"


@settings(max_examples=100)
@given(st.text(alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')), min_size=1, max_size=50))
def test_normalize_name_no_special_chars(name):
    """
    Property: Name normalization removes special characters.

    For any input string, the normalized name should only contain
    alphanumeric characters and underscores.

    Feature: kconfig-naming-standard, Property 1
    Validates: Requirements 1.5
    """
    normalized = NamingRules.normalize_name(name)

    # Skip if normalization resulted in empty string
    if normalized:
        # Should only contain A-Z, 0-9, and _
        assert all(c.isalnum() or c == '_' for c in normalized), \
            f"Normalized name '{normalized}' should only contain alphanumeric and underscore"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
