#!/usr/bin/env python3
"""
Property-based tests for buffer configuration pattern consistency.

Feature: kconfig-architecture-enhancement, Property 11: Buffer configuration pattern consistency
Validates: Requirements 14.3

Property: For any peripheral instance that supports buffers, both TX_BUFFER_SIZE
and RX_BUFFER_SIZE configuration options should be present.
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


# Peripheral instances that support buffers (platform, instance, prefix)
BUFFERED_PERIPHERALS = [
    # STM32 UART instances
    ('PLATFORM_STM32', 'INSTANCE_STM32_UART_1', 'UART1'),
    ('PLATFORM_STM32', 'INSTANCE_STM32_UART_2', 'UART2'),
    ('PLATFORM_STM32', 'INSTANCE_STM32_UART_3', 'UART3'),
    # Native UART instances
    ('PLATFORM_NATIVE', 'INSTANCE_NATIVE_UART_0', 'UART0'),
    ('PLATFORM_NATIVE', 'INSTANCE_NATIVE_UART_1', 'UART1'),
    ('PLATFORM_NATIVE', 'INSTANCE_NATIVE_UART_2', 'UART2'),
    ('PLATFORM_NATIVE', 'INSTANCE_NATIVE_UART_3', 'UART3'),
    # Native SPI instances
    ('PLATFORM_NATIVE', 'INSTANCE_NX_SPI_0', 'SPI0'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_SPI_1', 'SPI1'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_SPI_2', 'SPI2'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_SPI_3', 'SPI3'),
    # Native I2C instances
    ('PLATFORM_NATIVE', 'INSTANCE_NX_I2C_0', 'I2C0'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_I2C_1', 'I2C1'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_I2C_2', 'I2C2'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_I2C_3', 'I2C3'),
]


@st.composite
def buffered_peripheral_selection(draw):
    """Generate a buffered peripheral instance selection."""
    return draw(st.sampled_from(BUFFERED_PERIPHERALS))


@settings(max_examples=100)
@given(buffered_peripheral_selection())
def test_buffered_peripheral_has_both_buffer_sizes(peripheral_selection):
    """
    Property 11: Buffer configuration pattern consistency.

    For any peripheral instance that supports buffers, both TX_BUFFER_SIZE
    and RX_BUFFER_SIZE configuration options should be present.

    Feature: kconfig-architecture-enhancement, Property 11
    Validates: Requirements 14.3
    """
    platform, instance, prefix = peripheral_selection

    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Enable the platform
    platform_sym = kconf.syms.get(platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {platform} not found")

    platform_sym.set_value('y')

    # Check if instance exists
    instance_sym = kconf.syms.get(instance)
    if instance_sym is None:
        pytest.skip(f"Instance symbol {instance} not found")

    # Enable the instance
    instance_sym.set_value('y')

    # Check for TX_BUFFER_SIZE
    tx_buffer_sym = kconf.syms.get(f'{prefix}_TX_BUFFER_SIZE')
    rx_buffer_sym = kconf.syms.get(f'{prefix}_RX_BUFFER_SIZE')

    # Both buffer size symbols should exist
    assert tx_buffer_sym is not None, \
        f"TX_BUFFER_SIZE missing for {instance} (expected {prefix}_TX_BUFFER_SIZE)"
    assert rx_buffer_sym is not None, \
        f"RX_BUFFER_SIZE missing for {instance} (expected {prefix}_RX_BUFFER_SIZE)"


@settings(max_examples=100)
@given(buffered_peripheral_selection())
def test_buffer_sizes_have_valid_ranges(peripheral_selection):
    """
    Property: Buffer sizes have valid range constraints.

    For any peripheral instance with buffer configuration, the buffer sizes
    should have valid range constraints (min >= 16, max <= 4096).

    Feature: kconfig-architecture-enhancement, Property 11
    Validates: Requirements 14.3
    """
    platform, instance, prefix = peripheral_selection

    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Enable the platform
    platform_sym = kconf.syms.get(platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {platform} not found")

    platform_sym.set_value('y')

    # Check if instance exists
    instance_sym = kconf.syms.get(instance)
    if instance_sym is None:
        pytest.skip(f"Instance symbol {instance} not found")

    # Enable the instance
    instance_sym.set_value('y')

    # Get buffer size symbols
    tx_buffer_sym = kconf.syms.get(f'{prefix}_TX_BUFFER_SIZE')
    rx_buffer_sym = kconf.syms.get(f'{prefix}_RX_BUFFER_SIZE')

    if tx_buffer_sym is None or rx_buffer_sym is None:
        pytest.skip(f"Buffer size symbols not found for {instance}")

    # Check TX buffer range
    if hasattr(tx_buffer_sym, 'ranges') and tx_buffer_sym.ranges:
        for range_tuple in tx_buffer_sym.ranges:
            min_val, max_val, _ = range_tuple
            min_int = int(min_val.str_value) if hasattr(min_val, 'str_value') else 0
            max_int = int(max_val.str_value) if hasattr(max_val, 'str_value') else 0
            assert min_int >= 16, f"TX_BUFFER_SIZE min range too small for {instance}: {min_int}"
            assert max_int <= 4096, f"TX_BUFFER_SIZE max range too large for {instance}: {max_int}"

    # Check RX buffer range
    if hasattr(rx_buffer_sym, 'ranges') and rx_buffer_sym.ranges:
        for range_tuple in rx_buffer_sym.ranges:
            min_val, max_val, _ = range_tuple
            min_int = int(min_val.str_value) if hasattr(min_val, 'str_value') else 0
            max_int = int(max_val.str_value) if hasattr(max_val, 'str_value') else 0
            assert min_int >= 16, f"RX_BUFFER_SIZE min range too small for {instance}: {min_int}"
            assert max_int <= 4096, f"RX_BUFFER_SIZE max range too large for {instance}: {max_int}"


@settings(max_examples=100)
@given(buffered_peripheral_selection())
def test_buffer_sizes_have_default_values(peripheral_selection):
    """
    Property: Buffer sizes have default values.

    For any peripheral instance with buffer configuration, both TX and RX
    buffer sizes should have default values configured.

    Feature: kconfig-architecture-enhancement, Property 11
    Validates: Requirements 14.3
    """
    platform, instance, prefix = peripheral_selection

    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Enable the platform
    platform_sym = kconf.syms.get(platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {platform} not found")

    platform_sym.set_value('y')

    # Check if instance exists
    instance_sym = kconf.syms.get(instance)
    if instance_sym is None:
        pytest.skip(f"Instance symbol {instance} not found")

    # Enable the instance
    instance_sym.set_value('y')

    # Get buffer size symbols
    tx_buffer_sym = kconf.syms.get(f'{prefix}_TX_BUFFER_SIZE')
    rx_buffer_sym = kconf.syms.get(f'{prefix}_RX_BUFFER_SIZE')

    if tx_buffer_sym is None or rx_buffer_sym is None:
        pytest.skip(f"Buffer size symbols not found for {instance}")

    # Get default values
    tx_default = tx_buffer_sym.str_value
    rx_default = rx_buffer_sym.str_value

    # Verify defaults are set
    assert tx_default != '', f"TX_BUFFER_SIZE has no default value for {instance}"
    assert rx_default != '', f"RX_BUFFER_SIZE has no default value for {instance}"

    # Verify defaults are positive integers
    assert int(tx_default) > 0, f"TX_BUFFER_SIZE default must be positive for {instance}"
    assert int(rx_default) > 0, f"RX_BUFFER_SIZE default must be positive for {instance}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
