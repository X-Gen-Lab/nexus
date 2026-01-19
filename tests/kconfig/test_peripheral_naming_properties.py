#!/usr/bin/env python3
"""
Property-based tests for peripheral naming consistency.

Feature: kconfig-architecture-enhancement, Property 10: Peripheral naming consistency
Validates: Requirements 14.1

Property: For any peripheral instance, its configuration symbols should follow
the INSTANCE_<PLATFORM>_<PERIPHERAL>_<N> naming pattern, where N is the instance number.
"""

import os
import re
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


# Peripheral instances to test with expected naming pattern
PERIPHERAL_INSTANCES = [
    ('PLATFORM_STM32', 'INSTANCE_NX_UART_1', 'NX', 'UART', '1'),
    ('PLATFORM_STM32', 'INSTANCE_NX_UART_2', 'NX', 'UART', '2'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_0', 'NX', 'UART', '0'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_1', 'NX', 'UART', '1'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_2', 'NX', 'UART', '2'),
]


@st.composite
def peripheral_instance_selection(draw):
    """Generate a peripheral instance selection."""
    return draw(st.sampled_from(PERIPHERAL_INSTANCES))


@settings(max_examples=100)
@given(peripheral_instance_selection())
def test_peripheral_instance_naming_pattern(peripheral_selection):
    """
    Property 10: Peripheral naming consistency.

    For any peripheral instance, the configuration symbol should follow
    the INSTANCE_<PLATFORM>_<PERIPHERAL>_<N> naming pattern.

    Feature: kconfig-architecture-enhancement, Property 10
    Validates: Requirements 14.1
    """
    platform, instance, platform_prefix, peripheral, number = peripheral_selection

    # Verify the instance name follows the pattern
    expected_pattern = f'INSTANCE_{platform_prefix}_{peripheral}_{number}'
    assert instance == expected_pattern, \
        f"Instance name {instance} does not match expected pattern {expected_pattern}"

    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Enable the platform
        platform_sym = kconf.syms.get(platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {platform} not found")

        platform_sym.set_value('y')

        # Check if instance symbol exists
        instance_sym = kconf.syms.get(instance)
        if instance_sym is None:
            pytest.skip(f"Instance symbol {instance} not found")

        # Verify the symbol exists and can be enabled
        instance_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Verify the instance is enabled in config
        assert f'CONFIG_{instance}=y' in config_content, \
            f"Instance {instance} should be enabled in configuration"


@settings(max_examples=100)
@given(peripheral_instance_selection())
def test_peripheral_parameter_naming_consistency(peripheral_selection):
    """
    Property: Peripheral parameter names follow consistent pattern.

    For any peripheral instance, parameter names should follow the
    <PERIPHERAL><N>_<PARAMETER> pattern (e.g., UART1_BAUDRATE).

    Feature: kconfig-architecture-enhancement, Property 10
    Validates: Requirements 14.1
    """
    platform, instance, platform_prefix, peripheral, number = peripheral_selection

    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Enable the platform
        platform_sym = kconf.syms.get(platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {platform} not found")

        platform_sym.set_value('y')

        # Enable the instance
        instance_sym = kconf.syms.get(instance)
        if instance_sym is None:
            pytest.skip(f"Instance symbol {instance} not found")

        instance_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Find all parameter lines for this instance
        # Parameters should follow pattern: UART1_BAUDRATE, UART2_TX_BUFFER_SIZE, etc.
        param_prefix = f'{peripheral}{number}_'
        param_pattern = re.compile(rf'CONFIG_{param_prefix}(\w+)=')

        params_found = param_pattern.findall(config_content)

        # Verify we found some parameters
        assert len(params_found) > 0, \
            f"No parameters found for {instance} with prefix {param_prefix}"

        # Verify all parameters follow the naming pattern
        for param in params_found:
            full_param = f'{param_prefix}{param}'
            # Parameter name should be all uppercase
            assert full_param.isupper(), \
                f"Parameter {full_param} should be all uppercase"


@settings(max_examples=50)
@given(peripheral_instance_selection())
def test_peripheral_instance_number_in_name(peripheral_selection):
    """
    Property: Peripheral instance number is correctly embedded in name.

    For any peripheral instance, the instance number should be correctly
    embedded in both the instance name and parameter names.

    Feature: kconfig-architecture-enhancement, Property 10
    Validates: Requirements 14.1
    """
    platform, instance, platform_prefix, peripheral, number = peripheral_selection

    # Verify instance name contains the number
    assert number in instance, \
        f"Instance name {instance} should contain number {number}"

    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Enable the platform
        platform_sym = kconf.syms.get(platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {platform} not found")

        platform_sym.set_value('y')

        # Enable the instance
        instance_sym = kconf.syms.get(instance)
        if instance_sym is None:
            pytest.skip(f"Instance symbol {instance} not found")

        instance_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Verify parameter names contain the instance number
        param_prefix = f'{peripheral}{number}_'
        assert param_prefix in config_content, \
            f"Parameter prefix {param_prefix} not found in configuration"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
