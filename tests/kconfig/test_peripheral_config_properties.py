#!/usr/bin/env python3
"""
Property-based tests for peripheral instance configuration completeness.

Feature: kconfig-architecture-enhancement, Property 9: Peripheral instance configuration completeness
Validates: Requirements 5.1, 5.2, 5.3

Property: For any enabled peripheral instance, all required parameters
for that peripheral type should be present in the configuration.
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


# Required parameters for UART instances
UART_REQUIRED_PARAMS = [
    'BAUDRATE',
    'DATA_BITS',
    'STOP_BITS',
    'PARITY_VALUE',
    'TX_BUFFER_SIZE',
    'RX_BUFFER_SIZE'
]


# UART instances to test
UART_INSTANCES = [
    ('PLATFORM_STM32', 'INSTANCE_STM32_UART_1', 'UART1'),
    ('PLATFORM_STM32', 'INSTANCE_STM32_UART_2', 'UART2'),
    ('PLATFORM_STM32', 'INSTANCE_STM32_UART_3', 'UART3'),
]


@st.composite
def uart_instance_selection(draw):
    """Generate a UART instance selection."""
    return draw(st.sampled_from(UART_INSTANCES))


@settings(max_examples=100)
@given(uart_instance_selection())
def test_uart_instance_has_required_parameters(uart_selection):
    """
    Property 9: Peripheral instance configuration completeness.

    For any enabled UART instance, all required parameters should be
    present in the configuration.

    Feature: kconfig-architecture-enhancement, Property 9
    Validates: Requirements 5.1, 5.2, 5.3
    """
    platform, instance, prefix = uart_selection

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

        # Enable the UART instance
        instance_sym = kconf.syms.get(instance)
        if instance_sym is None:
            pytest.skip(f"UART instance symbol {instance} not found")

        instance_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Verify all required parameters are present
        for param in UART_REQUIRED_PARAMS:
            param_name = f'{prefix}_{param}'
            # Check if the parameter exists in the config
            assert f'CONFIG_{param_name}=' in config_content, \
                f"Required parameter {param_name} missing for {instance}"


@settings(max_examples=100)
@given(uart_instance_selection())
def test_uart_instance_has_buffer_sizes(uart_selection):
    """
    Property: UART instances have both TX and RX buffer sizes.

    For any enabled UART instance, both TX_BUFFER_SIZE and RX_BUFFER_SIZE
    should be configured.

    Feature: kconfig-architecture-enhancement, Property 9
    Validates: Requirements 5.1, 5.2, 5.3
    """
    platform, instance, prefix = uart_selection

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

        # Enable the UART instance
        instance_sym = kconf.syms.get(instance)
        if instance_sym is None:
            pytest.skip(f"UART instance symbol {instance} not found")

        instance_sym.set_value('y')

        # Get buffer size symbols
        tx_buffer_sym = kconf.syms.get(f'{prefix}_TX_BUFFER_SIZE')
        rx_buffer_sym = kconf.syms.get(f'{prefix}_RX_BUFFER_SIZE')

        assert tx_buffer_sym is not None, f"TX_BUFFER_SIZE not found for {instance}"
        assert rx_buffer_sym is not None, f"RX_BUFFER_SIZE not found for {instance}"

        # Get buffer size values
        tx_buffer_size = tx_buffer_sym.str_value
        rx_buffer_size = rx_buffer_sym.str_value

        # Verify buffer sizes are configured
        assert tx_buffer_size != '', f"TX_BUFFER_SIZE not configured for {instance}"
        assert rx_buffer_size != '', f"RX_BUFFER_SIZE not configured for {instance}"

        # Verify buffer sizes are positive integers
        assert int(tx_buffer_size) > 0, f"TX_BUFFER_SIZE must be positive for {instance}"
        assert int(rx_buffer_size) > 0, f"RX_BUFFER_SIZE must be positive for {instance}"


@settings(max_examples=50)
@given(uart_instance_selection())
def test_uart_instance_has_transfer_mode(uart_selection):
    """
    Property: UART instances have transfer mode configuration.

    For any enabled UART instance, a transfer mode (polling, interrupt, or DMA)
    should be configured via MODE_VALUE.

    Feature: kconfig-architecture-enhancement, Property 9
    Validates: Requirements 5.1, 5.2, 5.3
    """
    platform, instance, prefix = uart_selection

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

        # Enable the UART instance
        instance_sym = kconf.syms.get(instance)
        if instance_sym is None:
            pytest.skip(f"UART instance symbol {instance} not found")

        instance_sym.set_value('y')

        # Get MODE_VALUE symbol
        mode_value_sym = kconf.syms.get(f'{prefix}_MODE_VALUE')

        assert mode_value_sym is not None, f"MODE_VALUE not found for {instance}"

        # Get mode value
        mode_value = mode_value_sym.str_value

        # Verify mode value is configured
        assert mode_value != '', f"MODE_VALUE not configured for {instance}"

        # Verify mode value is valid (0=polling, 1=interrupt, 2=DMA)
        mode_int = int(mode_value)
        assert 0 <= mode_int <= 2, f"MODE_VALUE must be 0, 1, or 2 for {instance}, got {mode_int}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
