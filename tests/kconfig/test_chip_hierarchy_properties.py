#!/usr/bin/env python3
"""
Property-based tests for chip family configuration hierarchy.

Feature: kconfig-architecture-enhancement, Property 15: Chip family configuration hierarchy
Validates: Requirements 3.2, 3.3

Property: For any chip variant selection, its corresponding chip family
should also be selected. For example, if STM32F407 is selected, then
STM32F4 should also be selected.
"""

import os
import tempfile
import pytest
from hypothesis import given, strategies as st, settings


# Import kconfiglib
try:
    import kconfiglib
except ImportError:
    pytest.skip("kconfiglib not installed", allow_module_level=True)


# Get the root Kconfig file path
KCONFIG_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', 'Kconfig')


# Chip family to variant mapping
CHIP_HIERARCHY = {
    'STM32F4': ['STM32F407', 'STM32F429', 'STM32F446'],
    'STM32H7': ['STM32H743', 'STM32H750'],
    'STM32L4': ['STM32L476', 'STM32L432']
}


# Flatten all variants for selection
ALL_VARIANTS = []
for family, variants in CHIP_HIERARCHY.items():
    for variant in variants:
        ALL_VARIANTS.append((family, variant))


@st.composite
def chip_variant_selection(draw):
    """Generate a chip variant selection with its family."""
    return draw(st.sampled_from(ALL_VARIANTS))


@settings(max_examples=100)
@given(chip_variant_selection())
def test_chip_variant_implies_family(chip_selection):
    """
    Property 15: Chip family configuration hierarchy.

    For any chip variant selection, the corresponding chip family
    should also be selected.

    Feature: kconfig-architecture-enhancement, Property 15
    Validates: Requirements 3.2, 3.3
    """
    family, variant = chip_selection

    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # First enable STM32 platform
        platform_sym = kconf.syms.get('PLATFORM_STM32')
        if platform_sym is None:
            pytest.skip("PLATFORM_STM32 symbol not found")

        platform_sym.set_value('y')

        # Set the chip family
        family_sym = kconf.syms.get(family)
        if family_sym is None:
            pytest.skip(f"Chip family symbol {family} not found")

        family_sym.set_value('y')

        # Set the chip variant
        variant_sym = kconf.syms.get(variant)
        if variant_sym is None:
            pytest.skip(f"Chip variant symbol {variant} not found")

        variant_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Verify that both family and variant are enabled
        assert f'CONFIG_{family}=y' in config_content, \
            f"Chip family {family} should be enabled when variant {variant} is selected"

        assert f'CONFIG_{variant}=y' in config_content, \
            f"Chip variant {variant} should be enabled"


@settings(max_examples=100)
@given(chip_variant_selection())
def test_chip_name_matches_variant(chip_selection):
    """
    Property: Chip name configuration matches selected variant.

    For any chip variant selection, the STM32_CHIP_NAME should
    match the expected chip name for that variant.

    Feature: kconfig-architecture-enhancement, Property 15
    Validates: Requirements 3.2, 3.3
    """
    family, variant = chip_selection

    # Expected chip names
    CHIP_NAMES = {
        'STM32F407': 'STM32F407xx',
        'STM32F429': 'STM32F429xx',
        'STM32F446': 'STM32F446xx',
        'STM32H743': 'STM32H743xx',
        'STM32H750': 'STM32H750xx',
        'STM32L476': 'STM32L476xx',
        'STM32L432': 'STM32L432xx'
    }

    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Enable STM32 platform
        platform_sym = kconf.syms.get('PLATFORM_STM32')
        if platform_sym is None:
            pytest.skip("PLATFORM_STM32 symbol not found")

        platform_sym.set_value('y')

        # Set the chip family
        family_sym = kconf.syms.get(family)
        if family_sym is None:
            pytest.skip(f"Chip family symbol {family} not found")

        family_sym.set_value('y')

        # Set the chip variant
        variant_sym = kconf.syms.get(variant)
        if variant_sym is None:
            pytest.skip(f"Chip variant symbol {variant} not found")

        variant_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Get STM32_CHIP_NAME value
        chip_name_sym = kconf.syms.get('STM32_CHIP_NAME')
        assert chip_name_sym is not None, "STM32_CHIP_NAME symbol not found"

        chip_name = chip_name_sym.str_value

        # Verify chip name matches expected value
        expected_name = CHIP_NAMES.get(variant)
        assert chip_name == expected_name, \
            f"STM32_CHIP_NAME should be '{expected_name}' for {variant}, got '{chip_name}'"


@settings(max_examples=50)
@given(chip_variant_selection())
def test_only_one_variant_per_family(chip_selection):
    """
    Property: Only one chip variant can be selected per family.

    For any chip family, only one variant should be enabled at a time.

    Feature: kconfig-architecture-enhancement, Property 15
    Validates: Requirements 3.2, 3.3
    """
    family, variant = chip_selection

    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Enable STM32 platform
        platform_sym = kconf.syms.get('PLATFORM_STM32')
        if platform_sym is None:
            pytest.skip("PLATFORM_STM32 symbol not found")

        platform_sym.set_value('y')

        # Set the chip family
        family_sym = kconf.syms.get(family)
        if family_sym is None:
            pytest.skip(f"Chip family symbol {family} not found")

        family_sym.set_value('y')

        # Set the chip variant
        variant_sym = kconf.syms.get(variant)
        if variant_sym is None:
            pytest.skip(f"Chip variant symbol {variant} not found")

        variant_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Count how many variants of this family are enabled
        enabled_count = 0
        for v in CHIP_HIERARCHY[family]:
            if f'CONFIG_{v}=y' in config_content:
                enabled_count += 1

        # Exactly one variant should be enabled
        assert enabled_count == 1, \
            f"Expected exactly 1 variant of {family} enabled, found {enabled_count}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
