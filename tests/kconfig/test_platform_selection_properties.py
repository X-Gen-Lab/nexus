#!/usr/bin/env python3
"""
Property-based tests for platform selection configuration.

Feature: kconfig-architecture-enhancement, Property 1: Platform selection mutual exclusivity
Validates: Requirements 2.4

Property: For any configuration, when one platform is selected, all other
platform options should be unselected.
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


# Platform options
PLATFORMS = [
    'PLATFORM_NATIVE',
    'PLATFORM_STM32',
    'PLATFORM_GD32',
    'PLATFORM_ESP32',
    'PLATFORM_NRF52'
]


@st.composite
def platform_selection(draw):
    """Generate a platform selection."""
    return draw(st.sampled_from(PLATFORMS))


@settings(max_examples=100)
@given(platform_selection())
def test_platform_mutual_exclusivity(selected_platform):
    """
    Property 1: Platform selection mutual exclusivity.

    For any platform selection, only one platform should be enabled
    and all others should be disabled.

    Feature: kconfig-architecture-enhancement, Property 1
    Validates: Requirements 2.4
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected platform
        platform_sym = kconf.syms.get(selected_platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {selected_platform} not found")

        # Set the platform value
        platform_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Verify mutual exclusivity
        for platform in PLATFORMS:
            if platform == selected_platform:
                # Selected platform should be enabled
                assert f'CONFIG_{platform}=y' in config_content, \
                    f"Selected platform {platform} should be enabled"
            else:
                # Other platforms should be disabled or not set
                # They should either have "# CONFIG_<platform> is not set" or not appear
                enabled_pattern = f'CONFIG_{platform}=y'
                assert enabled_pattern not in config_content, \
                    f"Platform {platform} should not be enabled when {selected_platform} is selected"


@settings(max_examples=100)
@given(platform_selection())
def test_platform_name_consistency(selected_platform):
    """
    Property: Platform name is consistent with selection.

    For any platform selection, the PLATFORM_NAME config should match
    the selected platform.

    Feature: kconfig-architecture-enhancement, Property 1
    Validates: Requirements 2.4
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected platform
        platform_sym = kconf.syms.get(selected_platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {selected_platform} not found")

        platform_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Get PLATFORM_NAME value
        platform_name_sym = kconf.syms.get('PLATFORM_NAME')
        assert platform_name_sym is not None, "PLATFORM_NAME symbol not found"

        platform_name = platform_name_sym.str_value

        # Verify platform name matches selection
        expected_names = {
            'PLATFORM_NATIVE': 'native',
            'PLATFORM_STM32': 'stm32',
            'PLATFORM_GD32': 'gd32',
            'PLATFORM_ESP32': 'esp32',
            'PLATFORM_NRF52': 'nrf52'
        }

        expected_name = expected_names.get(selected_platform)
        assert platform_name == expected_name, \
            f"PLATFORM_NAME should be '{expected_name}' for {selected_platform}, got '{platform_name}'"


@settings(max_examples=50)
@given(platform_selection())
def test_only_one_platform_in_config(selected_platform):
    """
    Property: Only one platform appears as enabled in config.

    For any platform selection, exactly one platform should appear
    as enabled (=y) in the generated .config file.

    Feature: kconfig-architecture-enhancement, Property 1
    Validates: Requirements 2.4
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected platform
        platform_sym = kconf.syms.get(selected_platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {selected_platform} not found")

        platform_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Count how many platforms are enabled
        enabled_count = 0
        for platform in PLATFORMS:
            if f'CONFIG_{platform}=y' in config_content:
                enabled_count += 1

        # Exactly one platform should be enabled
        assert enabled_count == 1, \
            f"Expected exactly 1 platform enabled, found {enabled_count}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
