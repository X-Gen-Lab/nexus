#!/usr/bin/env python3
"""
Generate complete GPIO pin Kconfig configuration for STM32 series

This script generates detailed Kconfig configuration for each GPIO pin,
including mode, pull, speed, alternate function, and initial value settings.

Usage:
    python generate_kconfig_pins.py <series> <output_file>

Example:
    python generate_kconfig_pins.py f4 Kconfig_gpio_stm32f4
"""

import sys
from pathlib import Path


# GPIO port configurations for different STM32 series
STM32_SERIES_CONFIG = {
    'f1': {
        'ports': ['A', 'B', 'C', 'D', 'E', 'F', 'G'],
        'pins_per_port': 16,
        'has_af': True,
        'af_range': (0, 15),
    },
    'f4': {
        'ports': ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I'],
        'pins_per_port': 16,
        'has_af': True,
        'af_range': (0, 15),
    },
    'h7': {
        'ports': ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K'],
        'pins_per_port': 16,
        'has_af': True,
        'af_range': (0, 15),
    },
}


def generate_pin_config(port, pin, series='f4'):
    """Generate Kconfig configuration for a single GPIO pin."""

    config = f"""
menuconfig INSTANCE_STM32_GPIO{port}_PIN{pin}
    bool "Enable P{port}{pin}"
    default n
    help
      Enable GPIO pin P{port}{pin}

if INSTANCE_STM32_GPIO{port}_PIN{pin}

choice
    prompt "P{port}{pin} Read/Write Mode"
    default GPIO_{port}{pin}_RW_MODE_WRITE
    help
      Select read/write mode for P{port}{pin}

config GPIO_{port}{pin}_RW_MODE_READ
    bool "Read Only"
    help
      Configure P{port}{pin} as input only

config GPIO_{port}{pin}_RW_MODE_WRITE
    bool "Write Only"
    help
      Configure P{port}{pin} as output only

config GPIO_{port}{pin}_RW_MODE_READWRITE
    bool "Read-Write"
    help
      Configure P{port}{pin} as bidirectional (open-drain)

endchoice

config GPIO_{port}{pin}_RW_MODE
    int
    default 0 if GPIO_{port}{pin}_RW_MODE_READ
    default 1 if GPIO_{port}{pin}_RW_MODE_WRITE
    default 2 if GPIO_{port}{pin}_RW_MODE_READWRITE

choice
    prompt "P{port}{pin} GPIO Mode"
    default GPIO_{port}{pin}_MODE_OUTPUT_PP if GPIO_{port}{pin}_RW_MODE_WRITE
    default GPIO_{port}{pin}_MODE_INPUT if GPIO_{port}{pin}_RW_MODE_READ
    help
      Select GPIO mode for P{port}{pin}

config GPIO_{port}{pin}_MODE_INPUT
    bool "Input"
    help
      Input mode

config GPIO_{port}{pin}_MODE_OUTPUT_PP
    bool "Output Push-Pull"
    help
      Output push-pull mode

config GPIO_{port}{pin}_MODE_OUTPUT_OD
    bool "Output Open-Drain"
    help
      Output open-drain mode

config GPIO_{port}{pin}_MODE_AF_PP
    bool "Alternate Function Push-Pull"
    help
      Alternate function push-pull mode

config GPIO_{port}{pin}_MODE_AF_OD
    bool "Alternate Function Open-Drain"
    help
      Alternate function open-drain mode

config GPIO_{port}{pin}_MODE_ANALOG
    bool "Analog"
    help
      Analog mode

endchoice

config GPIO_{port}{pin}_MODE
    hex
    default 0x00000000 if GPIO_{port}{pin}_MODE_INPUT
    default 0x00000001 if GPIO_{port}{pin}_MODE_OUTPUT_PP
    default 0x00000011 if GPIO_{port}{pin}_MODE_OUTPUT_OD
    default 0x00000002 if GPIO_{port}{pin}_MODE_AF_PP
    default 0x00000012 if GPIO_{port}{pin}_MODE_AF_OD
    default 0x00000003 if GPIO_{port}{pin}_MODE_ANALOG

choice
    prompt "P{port}{pin} Pull-up/Pull-down"
    default GPIO_{port}{pin}_PULL_NONE
    help
      Select pull-up/pull-down configuration

config GPIO_{port}{pin}_PULL_NONE
    bool "No Pull"
    help
      No pull-up or pull-down

config GPIO_{port}{pin}_PULL_UP
    bool "Pull-up"
    help
      Enable pull-up resistor

config GPIO_{port}{pin}_PULL_DOWN
    bool "Pull-down"
    help
      Enable pull-down resistor

endchoice

config GPIO_{port}{pin}_PULL
    hex
    default 0x00000000 if GPIO_{port}{pin}_PULL_NONE
    default 0x00000001 if GPIO_{port}{pin}_PULL_UP
    default 0x00000002 if GPIO_{port}{pin}_PULL_DOWN

choice
    prompt "P{port}{pin} Speed"
    default GPIO_{port}{pin}_SPEED_MEDIUM
    help
      Select GPIO speed

config GPIO_{port}{pin}_SPEED_LOW
    bool "Low Speed"
    help
      Low speed (up to 2 MHz)

config GPIO_{port}{pin}_SPEED_MEDIUM
    bool "Medium Speed"
    help
      Medium speed (up to 25 MHz)

config GPIO_{port}{pin}_SPEED_HIGH
    bool "High Speed"
    help
      High speed (up to 50 MHz)

config GPIO_{port}{pin}_SPEED_VERY_HIGH
    bool "Very High Speed"
    help
      Very high speed (up to 100 MHz)

endchoice

config GPIO_{port}{pin}_SPEED
    hex
    default 0x00000000 if GPIO_{port}{pin}_SPEED_LOW
    default 0x00000001 if GPIO_{port}{pin}_SPEED_MEDIUM
    default 0x00000002 if GPIO_{port}{pin}_SPEED_HIGH
    default 0x00000003 if GPIO_{port}{pin}_SPEED_VERY_HIGH

config GPIO_{port}{pin}_ALTERNATE
    hex "P{port}{pin} Alternate Function"
    default 0x00
    range 0x00 0x0F
    depends on GPIO_{port}{pin}_MODE_AF_PP || GPIO_{port}{pin}_MODE_AF_OD
    help
      Alternate function number (0-15)

config GPIO_{port}{pin}_INIT_VALUE
    int "P{port}{pin} Initial Value"
    default 0
    range 0 1
    depends on GPIO_{port}{pin}_RW_MODE_WRITE || GPIO_{port}{pin}_RW_MODE_READWRITE
    help
      Initial output value (0=Low, 1=High)

endif # INSTANCE_STM32_GPIO{port}_PIN{pin}
"""
    return config


def generate_port_menu(port, series='f4'):
    """Generate menu for a GPIO port."""

    config = STM32_SERIES_CONFIG.get(series, STM32_SERIES_CONFIG['f4'])
    pins_per_port = config['pins_per_port']

    # Generate separator line: # + 76 dashes = 77 chars total
    separator = '#' + '-' * 76

    menu = f"""
{separator}
# Port {port} (GPIO{port}) - STM32{series.upper()} Series
{separator}

menu "Port {port} (GPIO{port})"
"""

    # Generate configuration for each pin
    for pin in range(pins_per_port):
        menu += generate_pin_config(port, pin, series)

    menu += f"\nendmenu  # Port {port}\n"

    return menu


def generate_kconfig_file(series, output_file):
    """Generate complete Kconfig file for a STM32 series."""

    if series not in STM32_SERIES_CONFIG:
        print(f"Error: Unsupported series '{series}'")
        print(f"Supported series: {', '.join(STM32_SERIES_CONFIG.keys())}")
        return False

    config = STM32_SERIES_CONFIG[series]
    ports = config['ports']

    # File header
    content = f"""# Kconfig_gpio_stm32{series}
# STM32{series.upper()} series GPIO pin configuration
# Author: Nexus Team
# Generated by generate_kconfig_pins.py
#
# STM32{series.upper()} series GPIO characteristics:
# - GPIO ports: {', '.join(ports)}
# - Pins per port: 0-{config['pins_per_port']-1}
# - GPIO modes: Input, Output PP/OD, AF PP/OD, Analog
# - Speed: Low, Medium, High, Very High
# - Pull: None, Pull-up, Pull-down
# - Alternate functions: AF0-AF15
"""

    # Generate configuration for each port
    for port in ports:
        content += generate_port_menu(port, series)

    # Write to file
    output_path = Path(output_file)
    output_path.write_text(content)

    print(f"Generated Kconfig file: {output_file}")
    print(f"Series: STM32{series.upper()}")
    print(f"Ports: {', '.join(ports)}")
    print(f"Total pins: {len(ports) * config['pins_per_port']}")

    return True


def main():
    """Main entry point."""
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <series> <output_file>")
        print(f"")
        print(f"Supported series: {', '.join(STM32_SERIES_CONFIG.keys())}")
        print(f"")
        print(f"Example:")
        print(f"  {sys.argv[0]} f4 Kconfig_gpio_stm32f4")
        sys.exit(1)

    series = sys.argv[1].lower()
    output_file = sys.argv[2]

    if not generate_kconfig_file(series, output_file):
        sys.exit(1)


if __name__ == "__main__":
    main()
