"""
\file            test_templates.py
\brief           Unit tests for peripheral template library
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Tests for peripheral template completeness and validity.
"""

import pytest
import sys
import os

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.templates import (
    UART_TEMPLATE,
    GPIO_TEMPLATE,
    SPI_TEMPLATE,
    I2C_TEMPLATE,
    ADC_TEMPLATE,
    DAC_TEMPLATE,
    CRC_TEMPLATE,
    WATCHDOG_TEMPLATE,
    PERIPHERAL_TEMPLATES,
    get_template,
    list_templates
)
from kconfig_tools.naming_rules import ParameterConfig, ChoiceConfig


class TestUARTTemplate:
    """Test UART peripheral template"""

    def test_uart_template_exists(self):
        """Test UART template is defined"""
        assert UART_TEMPLATE is not None
        assert UART_TEMPLATE.name == "UART"

    def test_uart_template_platform(self):
        """Test UART template platform"""
        assert UART_TEMPLATE.platform == "NATIVE"

    def test_uart_template_instance_type(self):
        """Test UART template uses numeric instances"""
        assert UART_TEMPLATE.instance_type == "numeric"
        assert UART_TEMPLATE.max_instances == 4

    def test_uart_template_parameters(self):
        """Test UART template has required parameters"""
        param_names = [p.name for p in UART_TEMPLATE.parameters]
        assert "BAUDRATE" in param_names
        assert "DATA_BITS" in param_names
        assert "STOP_BITS" in param_names
        assert "TX_BUFFER_SIZE" in param_names
        assert "RX_BUFFER_SIZE" in param_names

    def test_uart_template_choices(self):
        """Test UART template has required choices"""
        choice_names = [c.name for c in UART_TEMPLATE.choices]
        assert "PARITY" in choice_names
        assert "MODE" in choice_names

    def test_uart_parity_choice(self):
        """Test UART parity choice configuration"""
        parity = next(c for c in UART_TEMPLATE.choices if c.name == "PARITY")
        assert "NONE" in parity.options
        assert "ODD" in parity.options
        assert "EVEN" in parity.options
        assert parity.default == "NONE"
        assert parity.values == {"NONE": 0, "ODD": 1, "EVEN": 2}

    def test_uart_mode_choice(self):
        """Test UART mode choice configuration"""
        mode = next(c for c in UART_TEMPLATE.choices if c.name == "MODE")
        assert "POLLING" in mode.options
        assert "INTERRUPT" in mode.options
        assert "DMA" in mode.options
        assert mode.default == "INTERRUPT"


class TestGPIOTemplate:
    """Test GPIO peripheral template"""

    def test_gpio_template_exists(self):
        """Test GPIO template is defined"""
        assert GPIO_TEMPLATE is not None
        assert GPIO_TEMPLATE.name == "GPIO"

    def test_gpio_template_instance_type(self):
        """Test GPIO template uses alpha instances"""
        assert GPIO_TEMPLATE.instance_type == "alpha"
        assert GPIO_TEMPLATE.max_instances == 3

    def test_gpio_template_parameters(self):
        """Test GPIO template has required parameters"""
        param_names = [p.name for p in GPIO_TEMPLATE.parameters]
        assert "OUTPUT_VALUE" in param_names

    def test_gpio_template_choices(self):
        """Test GPIO template has required choices"""
        choice_names = [c.name for c in GPIO_TEMPLATE.choices]
        assert "MODE" in choice_names
        assert "PULL" in choice_names
        assert "SPEED" in choice_names

    def test_gpio_mode_choice(self):
        """Test GPIO mode choice configuration"""
        mode = next(c for c in GPIO_TEMPLATE.choices if c.name == "MODE")
        assert "INPUT" in mode.options
        assert "OUTPUT_PP" in mode.options
        assert "OUTPUT_OD" in mode.options
        assert "AF_PP" in mode.options
        assert "AF_OD" in mode.options
        assert "ANALOG" in mode.options
        assert mode.default == "OUTPUT_PP"


class TestSPITemplate:
    """Test SPI peripheral template"""

    def test_spi_template_exists(self):
        """Test SPI template is defined"""
        assert SPI_TEMPLATE is not None
        assert SPI_TEMPLATE.name == "SPI"

    def test_spi_template_parameters(self):
        """Test SPI template has required parameters"""
        param_names = [p.name for p in SPI_TEMPLATE.parameters]
        assert "MAX_SPEED" in param_names
        assert "TX_BUFFER_SIZE" in param_names
        assert "RX_BUFFER_SIZE" in param_names

    def test_spi_template_choices(self):
        """Test SPI template has required choices"""
        choice_names = [c.name for c in SPI_TEMPLATE.choices]
        assert "CPOL" in choice_names
        assert "CPHA" in choice_names
        assert "BIT_ORDER" in choice_names

    def test_spi_cpol_choice(self):
        """Test SPI clock polarity choice"""
        cpol = next(c for c in SPI_TEMPLATE.choices if c.name == "CPOL")
        assert "LOW" in cpol.options
        assert "HIGH" in cpol.options
        assert cpol.default == "LOW"

    def test_spi_cpha_choice(self):
        """Test SPI clock phase choice"""
        cpha = next(c for c in SPI_TEMPLATE.choices if c.name == "CPHA")
        assert "1EDGE" in cpha.options
        assert "2EDGE" in cpha.options
        assert cpha.default == "1EDGE"


class TestI2CTemplate:
    """Test I2C peripheral template"""

    def test_i2c_template_exists(self):
        """Test I2C template is defined"""
        assert I2C_TEMPLATE is not None
        assert I2C_TEMPLATE.name == "I2C"

    def test_i2c_template_parameters(self):
        """Test I2C template has required parameters"""
        param_names = [p.name for p in I2C_TEMPLATE.parameters]
        assert "TX_BUFFER_SIZE" in param_names
        assert "RX_BUFFER_SIZE" in param_names

    def test_i2c_template_choices(self):
        """Test I2C template has required choices"""
        choice_names = [c.name for c in I2C_TEMPLATE.choices]
        assert "SPEED" in choice_names
        assert "ADDR_MODE" in choice_names

    def test_i2c_speed_choice(self):
        """Test I2C speed mode choice"""
        speed = next(c for c in I2C_TEMPLATE.choices if c.name == "SPEED")
        assert "STANDARD" in speed.options
        assert "FAST" in speed.options
        assert "FAST_PLUS" in speed.options
        assert speed.default == "STANDARD"
        assert speed.values["STANDARD"] == 100000
        assert speed.values["FAST"] == 400000

    def test_i2c_addr_mode_choice(self):
        """Test I2C addressing mode choice"""
        addr_mode = next(c for c in I2C_TEMPLATE.choices if c.name == "ADDR_MODE")
        assert "7BIT" in addr_mode.options
        assert "10BIT" in addr_mode.options
        assert addr_mode.default == "7BIT"


class TestADCTemplate:
    """Test ADC peripheral template"""

    def test_adc_template_exists(self):
        """Test ADC template is defined"""
        assert ADC_TEMPLATE is not None
        assert ADC_TEMPLATE.name == "ADC"

    def test_adc_template_parameters(self):
        """Test ADC template has required parameters"""
        param_names = [p.name for p in ADC_TEMPLATE.parameters]
        assert "CHANNEL_COUNT" in param_names

    def test_adc_template_choices(self):
        """Test ADC template has required choices"""
        choice_names = [c.name for c in ADC_TEMPLATE.choices]
        assert "RESOLUTION" in choice_names
        assert "SAMPLE_TIME" in choice_names

    def test_adc_resolution_choice(self):
        """Test ADC resolution choice"""
        resolution = next(c for c in ADC_TEMPLATE.choices if c.name == "RESOLUTION")
        assert "8BIT" in resolution.options
        assert "10BIT" in resolution.options
        assert "12BIT" in resolution.options
        assert "16BIT" in resolution.options
        assert resolution.default == "12BIT"
        assert resolution.values["12BIT"] == 12


class TestDACTemplate:
    """Test DAC peripheral template"""

    def test_dac_template_exists(self):
        """Test DAC template is defined"""
        assert DAC_TEMPLATE is not None
        assert DAC_TEMPLATE.name == "DAC"

    def test_dac_template_parameters(self):
        """Test DAC template has required parameters"""
        param_names = [p.name for p in DAC_TEMPLATE.parameters]
        assert "CHANNEL_COUNT" in param_names
        assert "VREF_MV" in param_names

    def test_dac_template_choices(self):
        """Test DAC template has required choices"""
        choice_names = [c.name for c in DAC_TEMPLATE.choices]
        assert "RESOLUTION" in choice_names
        assert "TRIGGER" in choice_names

    def test_dac_trigger_choice(self):
        """Test DAC trigger mode choice"""
        trigger = next(c for c in DAC_TEMPLATE.choices if c.name == "TRIGGER")
        assert "SOFTWARE" in trigger.options
        assert "TIMER" in trigger.options
        assert "EXTERNAL" in trigger.options
        assert trigger.default == "SOFTWARE"


class TestCRCTemplate:
    """Test CRC peripheral template"""

    def test_crc_template_exists(self):
        """Test CRC template is defined"""
        assert CRC_TEMPLATE is not None
        assert CRC_TEMPLATE.name == "CRC"

    def test_crc_template_parameters(self):
        """Test CRC template has required parameters"""
        param_names = [p.name for p in CRC_TEMPLATE.parameters]
        assert "POLYNOMIAL" in param_names
        assert "INIT_VALUE" in param_names
        assert "FINAL_XOR" in param_names

    def test_crc_template_choices(self):
        """Test CRC template has required choices"""
        choice_names = [c.name for c in CRC_TEMPLATE.choices]
        assert "ALGORITHM" in choice_names
        assert "INPUT_FORMAT" in choice_names

    def test_crc_algorithm_choice(self):
        """Test CRC algorithm choice"""
        algorithm = next(c for c in CRC_TEMPLATE.choices if c.name == "ALGORITHM")
        assert "CRC32" in algorithm.options
        assert "CRC16" in algorithm.options
        assert "CRC8" in algorithm.options
        assert algorithm.default == "CRC32"

    def test_crc_polynomial_parameter(self):
        """Test CRC polynomial parameter is hex type"""
        polynomial = next(p for p in CRC_TEMPLATE.parameters if p.name == "POLYNOMIAL")
        assert polynomial.type == "hex"
        assert polynomial.default == 0x04C11DB7


class TestWatchdogTemplate:
    """Test Watchdog peripheral template"""

    def test_watchdog_template_exists(self):
        """Test Watchdog template is defined"""
        assert WATCHDOG_TEMPLATE is not None
        assert WATCHDOG_TEMPLATE.name == "WATCHDOG"

    def test_watchdog_template_parameters(self):
        """Test Watchdog template has required parameters"""
        param_names = [p.name for p in WATCHDOG_TEMPLATE.parameters]
        assert "DEFAULT_TIMEOUT_MS" in param_names
        assert "WINDOW_MIN_MS" in param_names

    def test_watchdog_template_choices(self):
        """Test Watchdog template has required choices"""
        choice_names = [c.name for c in WATCHDOG_TEMPLATE.choices]
        assert "WINDOW" in choice_names

    def test_watchdog_window_choice(self):
        """Test Watchdog window mode choice"""
        window = next(c for c in WATCHDOG_TEMPLATE.choices if c.name == "WINDOW")
        assert "DISABLED" in window.options
        assert "ENABLED" in window.options
        assert window.default == "DISABLED"

    def test_watchdog_timeout_parameter(self):
        """Test Watchdog timeout parameter has valid range"""
        timeout = next(p for p in WATCHDOG_TEMPLATE.parameters if p.name == "DEFAULT_TIMEOUT_MS")
        assert timeout.range is not None
        assert timeout.range[0] == 100
        assert timeout.range[1] == 60000


class TestTemplateRegistry:
    """Test template registry functions"""

    def test_peripheral_templates_dict(self):
        """Test PERIPHERAL_TEMPLATES dictionary contains all templates"""
        assert "UART" in PERIPHERAL_TEMPLATES
        assert "GPIO" in PERIPHERAL_TEMPLATES
        assert "SPI" in PERIPHERAL_TEMPLATES
        assert "I2C" in PERIPHERAL_TEMPLATES
        assert "ADC" in PERIPHERAL_TEMPLATES
        assert "DAC" in PERIPHERAL_TEMPLATES
        assert "CRC" in PERIPHERAL_TEMPLATES
        assert "WATCHDOG" in PERIPHERAL_TEMPLATES

    def test_get_template_valid(self):
        """Test get_template with valid peripheral name"""
        uart = get_template("UART")
        assert uart.name == "UART"

        gpio = get_template("gpio")  # Test case insensitive
        assert gpio.name == "GPIO"

    def test_get_template_invalid(self):
        """Test get_template with invalid peripheral name"""
        with pytest.raises(ValueError) as exc_info:
            get_template("INVALID")
        assert "Unknown peripheral" in str(exc_info.value)

    def test_list_templates(self):
        """Test list_templates returns all template names"""
        templates = list_templates()
        assert len(templates) == 8
        assert "UART" in templates
        assert "GPIO" in templates
        assert "SPI" in templates
        assert "I2C" in templates
        assert "ADC" in templates
        assert "DAC" in templates
        assert "CRC" in templates
        assert "WATCHDOG" in templates


class TestTemplateParameterValidity:
    """Test template parameter validity (Property 9)"""

    def test_all_parameters_have_non_empty_names(self):
        """Test all parameters have non-empty names"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for param in template.parameters:
                assert param.name, f"{template_name} has parameter with empty name"
                assert param.name.strip(), f"{template_name} has parameter with whitespace-only name"

    def test_all_parameters_have_valid_types(self):
        """Test all parameters have valid types"""
        valid_types = ["int", "hex", "bool", "string"]
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for param in template.parameters:
                assert param.type in valid_types, \
                    f"{template_name}.{param.name} has invalid type '{param.type}'"

    def test_all_parameters_have_matching_default_values(self):
        """Test all parameters have default values matching their type"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for param in template.parameters:
                if param.type == "int":
                    assert isinstance(param.default, int), \
                        f"{template_name}.{param.name} int type has non-int default"
                elif param.type == "hex":
                    assert isinstance(param.default, (int, str)), \
                        f"{template_name}.{param.name} hex type has invalid default"
                elif param.type == "bool":
                    assert isinstance(param.default, bool), \
                        f"{template_name}.{param.name} bool type has non-bool default"
                elif param.type == "string":
                    assert isinstance(param.default, str), \
                        f"{template_name}.{param.name} string type has non-string default"

    def test_range_only_for_int_hex_types(self):
        """Test range is only specified for int/hex types"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for param in template.parameters:
                if param.range is not None:
                    assert param.type in ["int", "hex"], \
                        f"{template_name}.{param.name} has range but type is '{param.type}'"

    def test_all_choices_have_non_empty_names(self):
        """Test all choices have non-empty names"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for choice in template.choices:
                assert choice.name, f"{template_name} has choice with empty name"
                assert choice.name.strip(), f"{template_name} has choice with whitespace-only name"

    def test_all_choices_have_options(self):
        """Test all choices have at least one option"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for choice in template.choices:
                assert len(choice.options) > 0, \
                    f"{template_name}.{choice.name} has no options"

    def test_all_choices_have_valid_defaults(self):
        """Test all choices have default in options list"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for choice in template.choices:
                assert choice.default in choice.options, \
                    f"{template_name}.{choice.name} default '{choice.default}' not in options"

    def test_all_choice_values_are_integers(self):
        """Test all choice value mappings use integers"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            for choice in template.choices:
                if choice.values is not None:
                    for option, value in choice.values.items():
                        assert isinstance(value, int), \
                            f"{template_name}.{choice.name}.{option} value is not an integer"

    def test_no_duplicate_parameter_names(self):
        """Test no duplicate parameter names in templates"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            param_names = [p.name for p in template.parameters]
            assert len(param_names) == len(set(param_names)), \
                f"{template_name} has duplicate parameter names"

    def test_no_duplicate_choice_names(self):
        """Test no duplicate choice names in templates"""
        for template_name, template in PERIPHERAL_TEMPLATES.items():
            choice_names = [c.name for c in template.choices]
            assert len(choice_names) == len(set(choice_names)), \
                f"{template_name} has duplicate choice names"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
