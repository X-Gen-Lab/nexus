#!/usr/bin/env python3
"""
Property-based tests for platform-specific default values.

Feature: kconfig-architecture-enhancement, Property 23: Platform-specific default values correctness
Validates: Requirements 2.3, 9.2, 9.3

Property: For any configuration option, different platforms should have
correct platform-specific default values.
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


# Platform-specific expected defaults
PLATFORM_DEFAULTS = {
    'PLATFORM_NATIVE': {
        'PLATFORM_NAME': 'native',
        'NATIVE_ENABLE_LOGGING': 'y',
        'NATIVE_LOG_LEVEL': '3',
        'NATIVE_ENABLE_STATISTICS': 'y',
        'NATIVE_BUFFER_ALIGNMENT': '4',
        'NATIVE_DMA_CHANNELS': '8',
        'NATIVE_ISR_SLOTS': '64',
    },
    'PLATFORM_STM32': {
        'PLATFORM_NAME': 'stm32',
    },
    'PLATFORM_GD32': {
        'PLATFORM_NAME': 'gd32',
    },
    'PLATFORM_ESP32': {
        'PLATFORM_NAME': 'esp32',
    },
    'PLATFORM_NRF52': {
        'PLATFORM_NAME': 'nrf52',
    }
}


@st.composite
def platform_selection(draw):
    """Generate a platform selection."""
    return draw(st.sampled_from(PLATFORMS))


@settings(max_examples=100)
@given(platform_selection())
def test_platform_specific_defaults(selected_platform):
    """
    Property 23: Platform-specific default values correctness.

    For any platform selection, configuration options should have
    correct platform-specific default values.

    Feature: kconfig-architecture-enhancement, Property 23
    Validates: Requirements 2.3, 9.2, 9.3
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get expected defaults for this platform
    expected_defaults = PLATFORM_DEFAULTS.get(selected_platform, {})

    # Verify each expected default
    for config_name, expected_value in expected_defaults.items():
        sym = kconf.syms.get(config_name)
        if sym is None:
            # Symbol might not be defined yet for some platforms
            continue

        actual_value = sym.str_value

        # For boolean values, normalize
        if expected_value in ['y', 'n']:
            if expected_value == 'y':
                assert actual_value in ['y', '1', 'true'], \
                    f"{config_name} should default to {expected_value} for {selected_platform}, got {actual_value}"
            else:
                assert actual_value in ['n', '0', 'false', ''], \
                    f"{config_name} should default to {expected_value} for {selected_platform}, got {actual_value}"
        else:
            # For other values, check exact match
            assert actual_value == expected_value, \
                f"{config_name} should default to '{expected_value}' for {selected_platform}, got '{actual_value}'"


@settings(max_examples=100)
@given(platform_selection())
def test_platform_name_default(selected_platform):
    """
    Property: PLATFORM_NAME has correct default for each platform.

    For any platform selection, PLATFORM_NAME should default to the
    correct platform identifier string.

    Feature: kconfig-architecture-enhancement, Property 23
    Validates: Requirements 2.3
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get PLATFORM_NAME
    platform_name_sym = kconf.syms.get('PLATFORM_NAME')
    assert platform_name_sym is not None, "PLATFORM_NAME symbol not found"

    platform_name = platform_name_sym.str_value

    # Verify platform name matches expected default
    expected_names = {
        'PLATFORM_NATIVE': 'native',
        'PLATFORM_STM32': 'stm32',
        'PLATFORM_GD32': 'gd32',
        'PLATFORM_ESP32': 'esp32',
        'PLATFORM_NRF52': 'nrf52'
    }

    expected_name = expected_names[selected_platform]
    assert platform_name == expected_name, \
        f"PLATFORM_NAME should default to '{expected_name}' for {selected_platform}, got '{platform_name}'"


@settings(max_examples=50)
@given(platform_selection())
def test_defconfig_file_exists(selected_platform):
    """
    Property: Platform has defconfig file or uses defaults.

    For any platform, either a defconfig file should exist or the
    platform should use sensible defaults from Kconfig.

    Feature: kconfig-architecture-enhancement, Property 23
    Validates: Requirements 9.2, 9.5
    """
    # Map platform to defconfig path
    platform_paths = {
        'PLATFORM_NATIVE': 'platforms/native/defconfig',
        'PLATFORM_STM32': 'platforms/stm32/defconfig',
        'PLATFORM_GD32': 'platforms/gd32/defconfig',
        'PLATFORM_ESP32': 'platforms/esp32/defconfig',
        'PLATFORM_NRF52': 'platforms/nrf52/defconfig'
    }

    defconfig_path = platform_paths.get(selected_platform)
    if defconfig_path is None:
        pytest.skip(f"No defconfig path defined for {selected_platform}")

    # Check if defconfig exists or if platform has defaults in Kconfig
    defconfig_exists = os.path.exists(defconfig_path)

    # Load Kconfig to check if platform has defaults
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get platform-specific symbols
    platform_prefix = selected_platform.replace('PLATFORM_', '')
    has_platform_symbols = any(
        sym_name.startswith(platform_prefix + '_')
        for sym_name in kconf.syms.keys()
    )

    # Either defconfig exists OR platform has symbols with defaults
    assert defconfig_exists or has_platform_symbols, \
        f"Platform {selected_platform} should have either defconfig file or default values in Kconfig"


@settings(max_examples=50)
@given(platform_selection())
def test_platform_defaults_are_valid(selected_platform):
    """
    Property: Platform default values are valid.

    For any platform, all default values should be valid according to
    their type and range constraints.

    Feature: kconfig-architecture-enhancement, Property 23
    Validates: Requirements 9.3
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get platform prefix
    platform_prefix = selected_platform.replace('PLATFORM_', '')

    # Check all platform-specific symbols
    for sym_name, sym in kconf.syms.items():
        if sym_name.startswith(platform_prefix + '_'):
            # Get the default value
            default_value = sym.str_value

            # Check type validity
            if sym.type == kconfiglib.BOOL:
                assert default_value in ['y', 'n', ''], \
                    f"Boolean symbol {sym_name} has invalid default: {default_value}"

            elif sym.type == kconfiglib.INT:
                # Should be a valid integer
                try:
                    int_value = int(default_value) if default_value else 0
                except ValueError:
                    pytest.fail(f"Integer symbol {sym_name} has non-integer default: {default_value}")

                # Check range constraints
                for low, high, _ in sym.ranges:
                    low_val = int(low.str_value) if hasattr(low, 'str_value') else 0
                    high_val = int(high.str_value) if hasattr(high, 'str_value') else 0
                    if low_val <= int_value <= high_val:
                        break
                else:
                    # If there are ranges and value doesn't match any, that's an error
                    if sym.ranges and default_value:
                        pytest.fail(
                            f"Integer symbol {sym_name} default {int_value} "
                            f"doesn't match any range constraint"
                        )

            elif sym.type == kconfiglib.HEX:
                # Should be a valid hex value
                if default_value:
                    try:
                        int(default_value, 16)
                    except ValueError:
                        pytest.fail(f"Hex symbol {sym_name} has invalid hex default: {default_value}")

            elif sym.type == kconfiglib.STRING:
                # String values are always valid
                pass


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
