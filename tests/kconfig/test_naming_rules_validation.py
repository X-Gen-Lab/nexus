#!/usr/bin/env python3
r"""
Unit tests for naming rule validation functions.

Feature: kconfig-naming-standard
Validates: Requirements 1.5, 2.4

Tests edge cases and validation logic for naming rules.
"""

import sys
import os
import pytest

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.naming_rules import NamingRules


class TestNormalizeName:
    """Test cases for normalize_name function."""

    def test_normalize_empty_string(self):
        """Test normalization of empty string."""
        result = NamingRules.normalize_name('')
        assert result == 'CONFIG_', "Empty string should be normalized to 'CONFIG_'"

    def test_normalize_lowercase(self):
        """Test normalization converts to uppercase."""
        result = NamingRules.normalize_name('native')
        assert result == 'NATIVE', "Should convert to uppercase"

    def test_normalize_mixed_case(self):
        """Test normalization of mixed case."""
        result = NamingRules.normalize_name('NaTiVe')
        assert result == 'NATIVE', "Should convert to uppercase"

    def test_normalize_with_spaces(self):
        """Test normalization replaces spaces with underscores."""
        result = NamingRules.normalize_name('native platform')
        assert result == 'NATIVE_PLATFORM', "Should replace spaces with underscores"

    def test_normalize_with_hyphens(self):
        """Test normalization replaces hyphens with underscores."""
        result = NamingRules.normalize_name('native-platform')
        assert result == 'NATIVE_PLATFORM', "Should replace hyphens with underscores"

    def test_normalize_with_special_chars(self):
        """Test normalization removes special characters."""
        result = NamingRules.normalize_name('native@#$platform')
        assert result == 'NATIVEPLATFORM', "Should remove special characters"

    def test_normalize_consecutive_underscores(self):
        """Test normalization removes consecutive underscores."""
        result = NamingRules.normalize_name('native___platform')
        assert result == 'NATIVE_PLATFORM', "Should remove consecutive underscores"

    def test_normalize_leading_underscore(self):
        """Test normalization removes leading underscores."""
        result = NamingRules.normalize_name('_native')
        assert result == 'NATIVE', "Should remove leading underscores"

    def test_normalize_trailing_underscore(self):
        """Test normalization removes trailing underscores."""
        result = NamingRules.normalize_name('native_')
        assert result == 'NATIVE', "Should remove trailing underscores"

    def test_normalize_numbers_only(self):
        """Test normalization of numbers-only string."""
        result = NamingRules.normalize_name('123')
        assert result == 'CONFIG_123', "Numbers-only should be prefixed with CONFIG_"

    def test_normalize_starting_with_number(self):
        """Test normalization of string starting with number."""
        result = NamingRules.normalize_name('1native')
        assert result == 'CONFIG_1NATIVE', "String starting with number should be prefixed"

    def test_normalize_alphanumeric(self):
        """Test normalization of alphanumeric string."""
        result = NamingRules.normalize_name('native123')
        assert result == 'NATIVE123', "Alphanumeric should be preserved"

    def test_normalize_with_underscores(self):
        """Test normalization preserves underscores."""
        result = NamingRules.normalize_name('native_platform_123')
        assert result == 'NATIVE_PLATFORM_123', "Underscores should be preserved"


class TestValidatePattern:
    """Test cases for validate_pattern function."""

    def test_validate_platform_enable_valid(self):
        """Test validation of valid platform enable symbol."""
        assert NamingRules.validate_pattern('NATIVE_ENABLE', NamingRules.PLATFORM_ENABLE_PATTERN)
        assert NamingRules.validate_pattern('STM32_ENABLE', NamingRules.PLATFORM_ENABLE_PATTERN)
        assert NamingRules.validate_pattern('GD32_ENABLE', NamingRules.PLATFORM_ENABLE_PATTERN)

    def test_validate_platform_enable_invalid(self):
        """Test validation of invalid platform enable symbol."""
        assert not NamingRules.validate_pattern('native_enable', NamingRules.PLATFORM_ENABLE_PATTERN)
        assert not NamingRules.validate_pattern('NATIVE', NamingRules.PLATFORM_ENABLE_PATTERN)
        assert not NamingRules.validate_pattern('_NATIVE_ENABLE', NamingRules.PLATFORM_ENABLE_PATTERN)
        assert not NamingRules.validate_pattern('123_ENABLE', NamingRules.PLATFORM_ENABLE_PATTERN)

    def test_validate_platform_name_valid(self):
        """Test validation of valid platform name symbol."""
        assert NamingRules.validate_pattern('NATIVE_PLATFORM_NAME', NamingRules.PLATFORM_NAME_PATTERN)
        assert NamingRules.validate_pattern('STM32_PLATFORM_NAME', NamingRules.PLATFORM_NAME_PATTERN)

    def test_validate_platform_name_invalid(self):
        """Test validation of invalid platform name symbol."""
        assert not NamingRules.validate_pattern('NATIVE_NAME', NamingRules.PLATFORM_NAME_PATTERN)
        assert not NamingRules.validate_pattern('native_platform_name', NamingRules.PLATFORM_NAME_PATTERN)

    def test_validate_peripheral_enable_valid(self):
        """Test validation of valid peripheral enable symbol."""
        assert NamingRules.validate_pattern('NATIVE_UART_ENABLE', NamingRules.PERIPHERAL_ENABLE_PATTERN)
        assert NamingRules.validate_pattern('STM32_GPIO_ENABLE', NamingRules.PERIPHERAL_ENABLE_PATTERN)

    def test_validate_peripheral_enable_invalid(self):
        """Test validation of invalid peripheral enable symbol."""
        assert not NamingRules.validate_pattern('UART_ENABLE', NamingRules.PERIPHERAL_ENABLE_PATTERN)
        assert not NamingRules.validate_pattern('native_uart_enable', NamingRules.PERIPHERAL_ENABLE_PATTERN)

    def test_validate_instance_enable_valid(self):
        """Test validation of valid instance enable symbol."""
        assert NamingRules.validate_pattern('INSTANCE_NX_UART_0', NamingRules.INSTANCE_ENABLE_PATTERN)
        assert NamingRules.validate_pattern('INSTANCE_NX_GPIOA', NamingRules.INSTANCE_ENABLE_PATTERN)
        assert NamingRules.validate_pattern('INSTANCE_NX_SPI_1', NamingRules.INSTANCE_ENABLE_PATTERN)

    def test_validate_instance_enable_invalid(self):
        """Test validation of invalid instance enable symbol."""
        assert not NamingRules.validate_pattern('UART_0', NamingRules.INSTANCE_ENABLE_PATTERN)
        assert not NamingRules.validate_pattern('INSTANCE_UART_0', NamingRules.INSTANCE_ENABLE_PATTERN)
        assert not NamingRules.validate_pattern('instance_nx_uart_0', NamingRules.INSTANCE_ENABLE_PATTERN)

    def test_validate_instance_param_valid(self):
        """Test validation of valid instance parameter symbol."""
        assert NamingRules.validate_pattern('UART0_BAUDRATE', NamingRules.INSTANCE_PARAM_PATTERN)
        assert NamingRules.validate_pattern('GPIOA_PIN0_MODE', NamingRules.INSTANCE_PARAM_PATTERN)
        assert NamingRules.validate_pattern('SPI1_MAX_SPEED', NamingRules.INSTANCE_PARAM_PATTERN)

    def test_validate_instance_param_invalid(self):
        """Test validation of invalid instance parameter symbol."""
        # These should be invalid because they're lowercase or missing parts
        assert not NamingRules.validate_pattern('uart0_baudrate', NamingRules.INSTANCE_PARAM_PATTERN)
        assert not NamingRules.validate_pattern('0_BAUDRATE', NamingRules.INSTANCE_PARAM_PATTERN)

    def test_validate_choice_option_valid(self):
        """Test validation of valid choice option symbol."""
        assert NamingRules.validate_pattern('NX_UART0_PARITY_NONE', NamingRules.CHOICE_OPTION_PATTERN)
        assert NamingRules.validate_pattern('NX_GPIOA_PIN0_MODE_INPUT', NamingRules.CHOICE_OPTION_PATTERN)

    def test_validate_choice_option_invalid(self):
        """Test validation of invalid choice option symbol."""
        assert not NamingRules.validate_pattern('UART0_PARITY_NONE', NamingRules.CHOICE_OPTION_PATTERN)
        assert not NamingRules.validate_pattern('nx_uart0_parity_none', NamingRules.CHOICE_OPTION_PATTERN)

    def test_validate_choice_value_valid(self):
        """Test validation of valid choice value symbol."""
        assert NamingRules.validate_pattern('UART0_PARITY_VALUE', NamingRules.CHOICE_VALUE_PATTERN)
        assert NamingRules.validate_pattern('GPIOA_PIN0_MODE_VALUE', NamingRules.CHOICE_VALUE_PATTERN)

    def test_validate_choice_value_invalid(self):
        """Test validation of invalid choice value symbol."""
        assert not NamingRules.validate_pattern('UART0_PARITY', NamingRules.CHOICE_VALUE_PATTERN)
        assert not NamingRules.validate_pattern('uart0_parity_value', NamingRules.CHOICE_VALUE_PATTERN)


class TestEdgeCases:
    """Test edge cases for naming functions."""

    def test_platform_enable_with_numbers(self):
        """Test platform enable with numbers in name."""
        result = NamingRules.platform_enable('STM32F4')
        assert result == 'STM32F4_ENABLE'
        assert NamingRules.validate_pattern(result, NamingRules.PLATFORM_ENABLE_PATTERN)

    def test_peripheral_enable_with_numbers(self):
        """Test peripheral enable with numbers in name."""
        result = NamingRules.peripheral_enable('STM32', 'I2C')
        assert result == 'STM32_I2C_ENABLE'
        assert NamingRules.validate_pattern(result, NamingRules.PERIPHERAL_ENABLE_PATTERN)

    def test_instance_enable_with_large_number(self):
        """Test instance enable with large instance number."""
        result = NamingRules.instance_enable('UART', 99)
        assert result == 'INSTANCE_NX_UART_99'
        assert NamingRules.validate_pattern(result, NamingRules.INSTANCE_ENABLE_PATTERN)

    def test_instance_param_with_complex_name(self):
        """Test instance parameter with complex parameter name."""
        result = NamingRules.instance_param('UART', 0, 'TX_BUFFER_SIZE')
        assert result == 'UART0_TX_BUFFER_SIZE'
        assert NamingRules.validate_pattern(result, NamingRules.INSTANCE_PARAM_PATTERN)

    def test_choice_option_with_long_names(self):
        """Test choice option with long names."""
        result = NamingRules.choice_option('UART', 0, 'TRANSFER_MODE', 'DMA_CIRCULAR')
        assert result == 'NX_UART0_TRANSFER_MODE_DMA_CIRCULAR'
        assert NamingRules.validate_pattern(result, NamingRules.CHOICE_OPTION_PATTERN)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
