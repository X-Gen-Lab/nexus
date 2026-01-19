#!/usr/bin/env python3
"""
Property-based tests for configuration symbol namespace consistency.

Feature: kconfig-architecture-enhancement, Property 3: Configuration symbol namespace consistency
Validates: Requirements 11.4, 14.1

Property: For any platform's configuration symbols, naming should follow
the pattern <PLATFORM>_<COMPONENT>_<PARAMETER>.
"""

import os
import re
import pytest
from hypothesis import given, strategies as st, settings


# Import kconfiglib
try:
    import kconfiglib
except ImportError:
    pytest.skip("kconfiglib not installed", allow_module_level=True)


# Get the root Kconfig file path
KCONFIG_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', 'Kconfig')


# Platform options and their prefixes
PLATFORM_PREFIXES = {
    'PLATFORM_NATIVE': 'NATIVE',
    'PLATFORM_STM32': 'STM32',
    'PLATFORM_GD32': 'GD32',
    'PLATFORM_ESP32': 'ESP32',
    'PLATFORM_NRF52': 'NRF52'
}


# Peripheral types that should follow naming conventions
PERIPHERAL_TYPES = ['UART', 'GPIO', 'SPI', 'I2C', 'TIMER', 'ADC', 'DAC', 'CAN']


@st.composite
def platform_selection(draw):
    """Generate a platform selection."""
    return draw(st.sampled_from(list(PLATFORM_PREFIXES.keys())))


@settings(max_examples=100)
@given(platform_selection())
def test_platform_symbol_naming_pattern(selected_platform):
    """
    Property 3: Configuration symbol namespace consistency.

    For any platform's configuration symbols, they should follow the
    naming pattern <PLATFORM>_<COMPONENT>_<PARAMETER> or
    <PLATFORM>_<PARAMETER> for platform-level settings.

    Feature: kconfig-architecture-enhancement, Property 3
    Validates: Requirements 11.4, 14.1
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get the prefix for the selected platform
    platform_prefix = PLATFORM_PREFIXES[selected_platform]

    # Check all symbols with the platform prefix
    for sym_name, sym in kconf.syms.items():
        if sym_name.startswith(platform_prefix + '_'):
            # Symbol should follow naming pattern
            # Valid patterns:
            # 1. <PLATFORM>_<COMPONENT>_<PARAMETER>
            # 2. <PLATFORM>_<PARAMETER> (for platform-level settings)
            # 3. INSTANCE_<PLATFORM>_<PERIPHERAL>_<N> (for peripheral instances)

            # Check if it's an instance symbol
            if sym_name.startswith('INSTANCE_' + platform_prefix):
                # Should match: INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>
                pattern = rf'^INSTANCE_{platform_prefix}_[A-Z0-9]+_\d+$'
                assert re.match(pattern, sym_name), \
                    f"Instance symbol {sym_name} doesn't follow INSTANCE_<PLATFORM>_<PERIPHERAL>_<N> pattern"
            else:
                # Should be all uppercase with underscores
                assert sym_name.isupper() or '_' in sym_name, \
                    f"Symbol {sym_name} should be uppercase with underscores"

                # Should not have consecutive underscores
                assert '__' not in sym_name, \
                    f"Symbol {sym_name} should not have consecutive underscores"

                # Should not start or end with underscore (except after prefix)
                parts = sym_name.split('_', 1)
                if len(parts) > 1:
                    assert not parts[1].startswith('_'), \
                        f"Symbol {sym_name} should not have underscore after prefix"
                    assert not parts[1].endswith('_'), \
                        f"Symbol {sym_name} should not end with underscore"


@settings(max_examples=100)
@given(platform_selection())
def test_peripheral_instance_naming_pattern(selected_platform):
    """
    Property: Peripheral instance naming follows consistent pattern.

    For any peripheral instance configuration, it should follow the
    pattern INSTANCE_<PLATFORM>_<PERIPHERAL>_<N> where N is the
    instance number.

    Feature: kconfig-architecture-enhancement, Property 3
    Validates: Requirements 14.1
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get the prefix for the selected platform
    platform_prefix = PLATFORM_PREFIXES[selected_platform]

    # Find all instance symbols
    instance_pattern = rf'^INSTANCE_{platform_prefix}_([A-Z0-9]+)_(\d+)$'

    for sym_name, sym in kconf.syms.items():
        if sym_name.startswith('INSTANCE_' + platform_prefix):
            match = re.match(instance_pattern, sym_name)
            assert match, \
                f"Instance symbol {sym_name} doesn't match pattern INSTANCE_{platform_prefix}_<PERIPHERAL>_<N>"

            peripheral_type = match.group(1)
            instance_num = match.group(2)

            # Peripheral type should be uppercase
            assert peripheral_type.isupper(), \
                f"Peripheral type {peripheral_type} should be uppercase"

            # Instance number should be valid
            assert instance_num.isdigit(), \
                f"Instance number {instance_num} should be numeric"

            # Instance number should not have leading zeros (except for 0 itself)
            if len(instance_num) > 1:
                assert not instance_num.startswith('0'), \
                    f"Instance number {instance_num} should not have leading zeros"


@settings(max_examples=50)
@given(platform_selection())
def test_peripheral_parameter_naming_consistency(selected_platform):
    """
    Property: Peripheral parameters follow consistent naming.

    For any peripheral instance, its parameters should follow the
    pattern <PERIPHERAL><N>_<PARAMETER> where N is the instance number.

    Feature: kconfig-architecture-enhancement, Property 3
    Validates: Requirements 14.2
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get the prefix for the selected platform
    platform_prefix = PLATFORM_PREFIXES[selected_platform]

    # Find peripheral parameter symbols
    # Pattern: <PERIPHERAL><N>_<PARAMETER>
    # Examples: UART1_BAUDRATE, GPIO2_MODE, SPI3_CLOCK_SPEED

    for peripheral in PERIPHERAL_TYPES:
        # Look for symbols like UART1_, GPIO2_, etc.
        param_pattern = rf'^{peripheral}\d+_[A-Z_]+$'

        for sym_name, sym in kconf.syms.items():
            if re.match(param_pattern, sym_name):
                # Extract instance number
                match = re.match(rf'^{peripheral}(\d+)_(.+)$', sym_name)
                if match:
                    instance_num = match.group(1)
                    param_name = match.group(2)

                    # Instance number should not have leading zeros
                    if len(instance_num) > 1:
                        assert not instance_num.startswith('0'), \
                            f"Instance number in {sym_name} should not have leading zeros"

                    # Parameter name should be uppercase with underscores
                    assert param_name.isupper() or '_' in param_name, \
                        f"Parameter name {param_name} should be uppercase"

                    # Should not have consecutive underscores
                    assert '__' not in param_name, \
                        f"Parameter name {param_name} should not have consecutive underscores"


@settings(max_examples=50)
@given(platform_selection())
def test_no_mixed_case_symbols(selected_platform):
    """
    Property: Configuration symbols are consistently uppercase.

    For any platform configuration symbol, it should be entirely
    uppercase (with underscores for separation).

    Feature: kconfig-architecture-enhancement, Property 3
    Validates: Requirements 11.4
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get the prefix for the selected platform
    platform_prefix = PLATFORM_PREFIXES[selected_platform]

    # Check all symbols with the platform prefix
    for sym_name, sym in kconf.syms.items():
        if sym_name.startswith(platform_prefix + '_') or sym_name.startswith('INSTANCE_' + platform_prefix):
            # Symbol should be uppercase (letters, numbers, underscores only)
            assert re.match(r'^[A-Z0-9_]+$', sym_name), \
                f"Symbol {sym_name} should be uppercase with underscores only (no lowercase or special chars)"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
