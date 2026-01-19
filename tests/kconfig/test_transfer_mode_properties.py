#!/usr/bin/env python3
"""
Property-based tests for transfer mode configuration pattern consistency.

Feature: kconfig-architecture-enhancement, Property 12: Transfer mode configuration pattern consistency
Validates: Requirements 5.4, 14.4

Property: For any peripheral instance that supports multiple transfer modes,
a choice menu and corresponding MODE_VALUE integer configuration should be provided.
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


# Peripheral instances that support transfer modes (platform, instance, prefix)
TRANSFER_MODE_PERIPHERALS = [
    # STM32 UART instances - (platform, instance, choice_prefix, value_prefix)
    ('PLATFORM_STM32', 'INSTANCE_NX_UART_1', 'NX_UART1', 'UART1'),
    ('PLATFORM_STM32', 'INSTANCE_NX_UART_2', 'NX_UART2', 'UART2'),
    ('PLATFORM_STM32', 'INSTANCE_NX_UART_3', 'NX_UART3', 'UART3'),
    # Native UART instances
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_0', 'NX_UART0', 'UART0'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_1', 'NX_UART1', 'UART1'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_2', 'NX_UART2', 'UART2'),
    ('PLATFORM_NATIVE', 'INSTANCE_NX_UART_3', 'NX_UART3', 'UART3'),
]


# Expected transfer modes
TRANSFER_MODES = ['POLLING', 'INTERRUPT', 'DMA']


@st.composite
def transfer_mode_peripheral_selection(draw):
    """Generate a transfer mode peripheral instance selection."""
    return draw(st.sampled_from(TRANSFER_MODE_PERIPHERALS))


@settings(max_examples=100)
@given(transfer_mode_peripheral_selection())
def test_peripheral_has_mode_value_config(peripheral_selection):
    """
    Property 12: Transfer mode configuration pattern consistency.

    For any peripheral instance that supports multiple transfer modes,
    a MODE_VALUE integer configuration should be provided.

    Feature: kconfig-architecture-enhancement, Property 12
    Validates: Requirements 5.4, 14.4
    """
    platform, instance, choice_prefix, value_prefix = peripheral_selection

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

    # Check for MODE_VALUE symbol (uses value_prefix, not choice_prefix)
    mode_value_sym = kconf.syms.get(f'{value_prefix}_MODE_VALUE')

    assert mode_value_sym is not None, \
        f"MODE_VALUE missing for {instance} (expected {value_prefix}_MODE_VALUE)"

    # Verify MODE_VALUE is an integer type
    assert mode_value_sym.type == kconfiglib.INT, \
        f"MODE_VALUE should be INT type for {instance}, got {mode_value_sym.type}"


@settings(max_examples=100)
@given(transfer_mode_peripheral_selection())
def test_peripheral_has_mode_choice_options(peripheral_selection):
    """
    Property: Transfer mode peripherals have choice options.

    For any peripheral instance that supports multiple transfer modes,
    choice options for each mode (POLLING, INTERRUPT, DMA) should exist.

    Feature: kconfig-architecture-enhancement, Property 12
    Validates: Requirements 5.4, 14.4
    """
    platform, instance, choice_prefix, value_prefix = peripheral_selection

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

    # Check for mode choice options (uses choice_prefix)
    mode_options_found = []
    for mode in TRANSFER_MODES:
        mode_sym = kconf.syms.get(f'{choice_prefix}_MODE_{mode}')
        if mode_sym is not None:
            mode_options_found.append(mode)

    # At least one mode option should exist
    assert len(mode_options_found) > 0, \
        f"No transfer mode options found for {instance} (expected {choice_prefix}_MODE_POLLING, etc.)"

    # Typically should have at least 2 modes
    assert len(mode_options_found) >= 2, \
        f"Expected at least 2 transfer modes for {instance}, found {len(mode_options_found)}: {mode_options_found}"


@settings(max_examples=100)
@given(transfer_mode_peripheral_selection())
def test_mode_value_has_valid_default(peripheral_selection):
    """
    Property: MODE_VALUE has a valid default value.

    For any enabled peripheral instance with transfer mode configuration,
    MODE_VALUE should have a valid default value (0, 1, or 2).

    Feature: kconfig-architecture-enhancement, Property 12
    Validates: Requirements 5.4, 14.4
    """
    platform, instance, choice_prefix, value_prefix = peripheral_selection

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

    # Get MODE_VALUE symbol (uses value_prefix)
    mode_value_sym = kconf.syms.get(f'{value_prefix}_MODE_VALUE')

    if mode_value_sym is None:
        pytest.skip(f"MODE_VALUE symbol not found for {instance}")

    # Get default value
    mode_value = mode_value_sym.str_value

    # Verify default is set (only when instance is enabled)
    if mode_value == '':
        pytest.skip(f"MODE_VALUE not visible for {instance} (instance may not be fully enabled)")

    # Verify default is valid (0=polling, 1=interrupt, 2=DMA)
    mode_int = int(mode_value)
    assert 0 <= mode_int <= 2, \
        f"MODE_VALUE default must be 0, 1, or 2 for {instance}, got {mode_int}"


@settings(max_examples=100)
@given(transfer_mode_peripheral_selection(), st.sampled_from([0, 1, 2]))
def test_mode_value_reflects_choice_selection(peripheral_selection, mode_index):
    """
    Property: MODE_VALUE reflects the selected transfer mode choice.

    For any peripheral instance, when a transfer mode is selected,
    MODE_VALUE should reflect the correct integer value.

    Feature: kconfig-architecture-enhancement, Property 12
    Validates: Requirements 5.4, 14.4
    """
    platform, instance, choice_prefix, value_prefix = peripheral_selection

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

    # Try to set the mode (uses choice_prefix)
    mode_name = TRANSFER_MODES[mode_index]
    mode_sym = kconf.syms.get(f'{choice_prefix}_MODE_{mode_name}')

    if mode_sym is None:
        pytest.skip(f"Mode {mode_name} not available for {instance}")

    # Set the mode
    mode_sym.set_value('y')

    # Get MODE_VALUE (uses value_prefix)
    mode_value_sym = kconf.syms.get(f'{value_prefix}_MODE_VALUE')

    if mode_value_sym is None:
        pytest.skip(f"MODE_VALUE symbol not found for {instance}")

    # Get the value
    mode_value_str = mode_value_sym.str_value

    # Skip if value is not set (instance may not be fully enabled)
    if mode_value_str == '':
        pytest.skip(f"MODE_VALUE not visible for {instance}")

    # Verify MODE_VALUE matches the selected mode
    mode_value = int(mode_value_str)
    assert mode_value == mode_index, \
        f"MODE_VALUE should be {mode_index} when {mode_name} is selected for {instance}, got {mode_value}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
