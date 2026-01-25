#!/usr/bin/env python3
"""
Generate nexus_config.h from Kconfig .config file.

This script uses kconfiglib to parse Kconfig files and generate a C header
file with NX_CONFIG_ prefixed macros for the Nexus project.

Features:
- Parses Kconfig configuration and generates C header macros
- Supports multiple platforms (NX/Native, STM32, GD32, ESP32, NRF52)
- Generates instance traversal macros for peripheral registration
- Handles composite peripheral names (INTERNAL_FLASH, OPTION_BYTES, ADC_BUFFER)
- Special handling for GPIO instances with port/pin format

Instance Naming Convention:
- Platform prefix identifies the target platform:
  * NX_     -> Native platform (PC simulation)
  * STM32_  -> STM32 platform
  * GD32_   -> GD32 platform
  * ESP32_  -> ESP32 platform
  * NRF52_  -> NRF52 platform

- Instance format examples:
  * INSTANCE_NX_UART_0           -> Platform: NX, Peripheral: UART, Instance: 0
  * INSTANCE_NX_INTERNAL_FLASH0  -> Platform: NX, Peripheral: INTERNAL_FLASH, Instance: 0
  * INSTANCE_NX_GPIOA_PIN0       -> Platform: NX, Peripheral: GPIO, Port: A, Pin: 0
  * INSTANCE_STM32_UART_0        -> Platform: STM32, Peripheral: UART, Instance: 0

Generated Macros:
- Configuration macros: NX_CONFIG_<SYMBOL>
- Instance traversal macros: NX_DEFINE_INSTANCE_<PLATFORM>_<PERIPHERAL>(fn)
- Instance enable flags: NX_CONFIG_<PERIPHERAL><INSTANCE>_ENABLED

Usage:
    python generate_config.py [--config .config] [--output nexus_config.h]
    python generate_config.py --default  # Generate default configuration
"""

import argparse
import os
import sys
from datetime import datetime

try:
    import kconfiglib
except ImportError:
    print("Error: kconfiglib is not installed")
    print("Please install it with: pip install kconfiglib>=14.1.0")
    sys.exit(1)


def parse_config_with_kconfig(kconfig_file='Kconfig', config_file='.config'):
    """
    Parse Kconfig and .config files using kconfiglib.

    Args:
        kconfig_file: Path to root Kconfig file
        config_file: Path to .config file (optional)

    Returns:
        Dictionary of config name -> value pairs
    """
    config = {}

    try:
        # Suppress kconfiglib warnings by redirecting stderr temporarily
        import io
        import contextlib

        # Create a custom stderr that filters out known harmless warnings
        class FilteredStderr:
            def __init__(self, original_stderr):
                self.original_stderr = original_stderr
                self.buffer = []

            def write(self, text):
                # Filter out known harmless warnings about choice symbols
                if 'choice symbol' in text and 'is defined with a prompt outside the choice' in text:
                    return
                if 'default selection' in text and 'is not contained in the choice' in text:
                    return
                # Pass through other messages
                self.original_stderr.write(text)

            def flush(self):
                self.original_stderr.flush()

        # Temporarily replace stderr
        original_stderr = sys.stderr
        sys.stderr = FilteredStderr(original_stderr)

        try:
            # Parse Kconfig file
            kconf = kconfiglib.Kconfig(kconfig_file)

            # Load .config if it exists
            if os.path.exists(config_file):
                kconf.load_config(config_file)
        finally:
            # Restore original stderr
            sys.stderr = original_stderr

        # Extract all configuration symbols
        for sym in kconf.unique_defined_syms:
            # Get symbol name with CONFIG_ prefix
            name = f'CONFIG_{sym.name}'

            # Get symbol value based on type
            if sym.type == kconfiglib.BOOL:
                if sym.tri_value == 2:  # y
                    config[name] = True
                elif sym.tri_value == 0:  # n
                    config[name] = False
                else:
                    config[name] = None
            elif sym.type == kconfiglib.TRISTATE:
                if sym.tri_value == 2:  # y
                    config[name] = True
                elif sym.tri_value == 1:  # m
                    config[name] = 'm'
                elif sym.tri_value == 0:  # n
                    config[name] = False
                else:
                    config[name] = None
            elif sym.type == kconfiglib.STRING:
                value = sym.str_value
                if value:
                    config[name] = value
                else:
                    config[name] = None
            elif sym.type == kconfiglib.INT:
                value = sym.str_value
                if value:
                    try:
                        config[name] = int(value)
                    except ValueError:
                        config[name] = value
                else:
                    config[name] = None
            elif sym.type == kconfiglib.HEX:
                value = sym.str_value
                if value:
                    # Keep hex format
                    if value.startswith('0x') or value.startswith('0X'):
                        config[name] = value
                    else:
                        config[name] = f'0x{value}'
                else:
                    config[name] = None
            else:
                # Unknown type, store as string
                config[name] = sym.str_value if sym.str_value else None

        return config

    except Exception as e:
        print(f"Error parsing Kconfig: {e}")
        import traceback
        traceback.print_exc()
        return {}


def categorize_config(key):
    """
    Determine the category for a configuration key.

    Args:
        key: Configuration key (e.g., 'NX_CONFIG_HAL_ENABLE')

    Returns:
        Category name or None
    """
    categories = {
        'PLATFORM': 'Platform Configuration',
        'HAL_ENABLE': 'HAL Enable',
        'HAL_DEBUG': 'Debug Configuration',
        'HAL_ASSERT': 'Assert Configuration',
        'HAL_STATISTICS': 'Statistics Configuration',
        'HAL_DEFAULT': 'Timeout Configuration',
        'HAL_INIT': 'Timeout Configuration',
        'HAL_MEM': 'Memory Configuration',
        'HAL_THREAD': 'Thread Safety',
        'HAL_MUTEX': 'Thread Safety',
        'UART': 'UART Configuration',
        'SPI': 'SPI Configuration',
        'I2C': 'I2C Configuration',
        'GPIO': 'GPIO Configuration',
        'ADC': 'ADC Configuration',
        'DAC': 'DAC Configuration',
        'TIMER': 'Timer Configuration',
        'CAN': 'CAN Configuration',
        'USB': 'USB Configuration',
        'FLASH': 'Flash Configuration',
        'RTC': 'RTC Configuration',
        'WATCHDOG': 'Watchdog Configuration',
        'SDIO': 'SDIO Configuration',
        'DMA': 'DMA Configuration',
        'CRC': 'CRC Configuration',
        'OSAL': 'OSAL Configuration',
        'LINKER': 'Linker Configuration',
        'STM32': 'STM32 Platform',
        'GD32': 'GD32 Platform',
        'ESP32': 'ESP32 Platform',
        'NRF52': 'NRF52 Platform',
        'NATIVE': 'Native Platform',
    }

    # Check each category prefix
    for prefix, category in categories.items():
        if prefix in key:
            return category

    return 'Other Configuration'


def parse_instance_symbol(nx_key, value):
    """
    Parse instance symbol and extract platform, peripheral, and instance information.

    Args:
        nx_key: Configuration key (e.g., 'NX_CONFIG_INSTANCE_NX_UART_0')
        value: Configuration value (must be True for instance symbols)

    Returns:
        Tuple of (platform, peripheral, instance_id, original_suffix, is_gpio, gpio_info)
        Returns (None, None, None, None, False, None) if not a valid instance symbol
    """
    if 'INSTANCE_' not in nx_key or value is not True:
        return None, None, None, None, False, None

    # Extract the part after INSTANCE_
    instance_part = nx_key.replace('NX_CONFIG_INSTANCE_', '')

    # Identify platform prefix (NX, STM32, GD32, ESP32, NRF52, etc.)
    platform = None
    peripheral_part = None

    # Check for known platform prefixes
    for platform_prefix in ['NX_', 'STM32_', 'GD32_', 'ESP32_', 'NRF52_']:
        if instance_part.startswith(platform_prefix):
            platform = platform_prefix.rstrip('_')
            peripheral_part = instance_part[len(platform_prefix):]
            break

    if not platform or not peripheral_part:
        return None, None, None, None, False, None

    # Special handling for GPIO instances
    # GPIO has two levels:
    # 1. Port instances: GPIOA, GPIOB (should be included in config, no traversal macro)
    # 2. Pin instances: GPIOA_PIN0, GPIOB_PIN1 (used for traversal macros)
    if peripheral_part.startswith('GPIO'):
        if '_PIN' in peripheral_part:
            # GPIO pin instance - extract port and pin for traversal macro
            gpio_match = peripheral_part.replace('GPIO', '')
            if '_PIN' in gpio_match:
                port = gpio_match.split('_PIN')[0]  # A, B, C, etc.
                pin = gpio_match.split('_PIN')[1]   # 0, 1, 2, etc.
                return platform, 'GPIO_PIN', None, None, True, (port, pin)
        else:
            # GPIO port instance (GPIOA, GPIOB, etc.)
            # These should be included in config but not generate traversal macros
            # Return None to skip traversal macro generation
            return None, None, None, None, False, None

    # Known composite peripheral names (must be checked before splitting)
    composite_peripherals = {
        'INTERNAL_FLASH': 'INTERNAL_FLASH',
        'OPTION_BYTES': 'OPTION_BYTES',
        'ADC_BUFFER': 'ADC_BUFFER',
    }

    # Check for composite peripheral names first
    peripheral = None
    instance_id = None
    original_suffix = None  # Track the original format (e.g., "FLASH0" for INTERNAL_FLASH0)

    for composite_name in composite_peripherals.keys():
        if peripheral_part.startswith(composite_name):
            peripheral = composite_name
            # Extract instance ID after composite name
            # e.g., INTERNAL_FLASH0 -> instance_id=0, original_suffix=FLASH0
            # e.g., OPTION_BYTES0 -> instance_id=0, original_suffix=BYTES0
            remainder = peripheral_part[len(composite_name):]
            original_suffix = remainder  # Keep original format for config symbol

            # Handle both FLASH0 and FLASH_0 formats for instance_id
            if remainder.startswith('_'):
                instance_id = remainder[1:]
            else:
                # Extract numeric suffix from remainder like "FLASH0" -> "0"
                import re
                match = re.search(r'\d+$', remainder)
                if match:
                    instance_id = match.group()
                else:
                    instance_id = remainder
            break

    # If not a composite peripheral, extract peripheral and instance normally
    if not peripheral:
        # Split by underscore and find the last numeric/alphanumeric part as instance
        parts = peripheral_part.split('_')
        if len(parts) >= 2:
            # Last part is instance ID
            instance_id = parts[-1]
            # Everything before is peripheral name
            peripheral = '_'.join(parts[:-1])
        elif len(parts) == 1:
            # Single part like "FLASH0" - extract number suffix
            import re
            match = re.match(r'([A-Z_]+?)(\d+)$', parts[0])
            if match:
                peripheral = match.group(1)
                instance_id = match.group(2)
            else:
                # No instance number
                return None, None, None, None, False, None
        else:
            return None, None, None, None, False, None

    return platform, peripheral, instance_id, original_suffix, False, None


def generate_header(config, output_path):
    """
    Generate the nexus_config.h header file.

    Args:
        config: Dictionary of configuration values
        output_path: Path to output header file
    """
    header = []

    # File header with Nexus comment style
    header.append('/**')
    header.append(' * \\file            nexus_config.h')
    header.append(' * \\brief           HAL configuration header (auto-generated)')
    header.append(' * \\author          Nexus Team')
    header.append(' *')
    header.append(' * This file is auto-generated from Kconfig. Do not edit manually.')
    header.append(f' * Generated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}')
    header.append(' */')
    header.append('')
    header.append('#ifndef NEXUS_CONFIG_H')
    header.append('#define NEXUS_CONFIG_H')
    header.append('')
    header.append('#ifdef __cplusplus')
    header.append('extern "C" {')
    header.append('#endif')
    header.append('')

    # Group configurations by category
    categorized = {}
    instance_symbols = {}  # Track INSTANCE_<PLATFORM>_* symbols for macro generation
    gpio_instances = []  # Special handling for GPIO instances

    # Known composite peripheral names (must be checked before splitting)
    composite_peripherals = {
        'INTERNAL_FLASH': 'INTERNAL_FLASH',
        'OPTION_BYTES': 'OPTION_BYTES',
        'ADC_BUFFER': 'ADC_BUFFER',
    }

    for key, value in config.items():
        # Skip if not set
        if value is None:
            continue

        # Convert CONFIG_xxx to NX_CONFIG_xxx
        if key.startswith('CONFIG_'):
            nx_key = key.replace('CONFIG_', 'NX_CONFIG_', 1)
        else:
            nx_key = f'NX_CONFIG_{key}'

        # Track instance symbols for macro generation using parse_instance_symbol function
        platform, peripheral, instance_id, original_suffix, is_gpio_pin, gpio_info = parse_instance_symbol(nx_key, value)

        if is_gpio_pin and gpio_info:
            # GPIO pin instance - add to gpio_instances for traversal macro
            gpio_instances.append(gpio_info)
            # Continue to add to categorized as well (don't skip)
        elif platform and peripheral and instance_id:
            # Regular peripheral instance
            platform_peripheral_key = f'{platform}_{peripheral}'

            if platform_peripheral_key not in instance_symbols:
                instance_symbols[platform_peripheral_key] = {
                    'platform': platform,
                    'peripheral': peripheral,
                    'instances': [],
                    'original_suffixes': {}
                }
            instance_symbols[platform_peripheral_key]['instances'].append(instance_id)

            # Store original suffix for composite peripherals
            if original_suffix:
                instance_symbols[platform_peripheral_key]['original_suffixes'][instance_id] = original_suffix
            # Continue to add to categorized as well (don't skip)

        # Determine category
        category = categorize_config(nx_key)

        if category not in categorized:
            categorized[category] = []

        categorized[category].append((nx_key, value))

    # Sort categories and write them
    for category in sorted(categorized.keys()):
        items = categorized[category]

        # Write category header
        header.append('/*' + '-' * 75 + '*/')
        header.append(f'/* {category:<73} */')
        header.append('/*' + '-' * 75 + '*/')
        header.append('')

        # Sort items within category
        for nx_key, value in sorted(items):
            # Generate macro based on value type
            if isinstance(value, bool):
                if value:
                    header.append(f'#define {nx_key} 1')
                else:
                    header.append(f'/* #undef {nx_key} */')
            elif isinstance(value, int):
                header.append(f'#define {nx_key} {value}')
            elif isinstance(value, str):
                # Check if it's a valid hex value (0x followed by hex digits)
                if (value.startswith('0x') or value.startswith('0X')) and len(value) > 2:
                    # Verify it's actually a valid hex number
                    try:
                        int(value, 16)
                        header.append(f'#define {nx_key} {value}')
                    except ValueError:
                        # Not a valid hex, treat as string
                        header.append(f'#define {nx_key} "{value}"')
                else:
                    # String value - add quotes
                    header.append(f'#define {nx_key} "{value}"')
            else:
                # Fallback for other types
                header.append(f'#define {nx_key} {value}')

        header.append('')

    # Generate instance traversal macros
    if instance_symbols or gpio_instances:
        header.append('/*' + '-' * 75 + '*/')
        header.append(f'/* {"Peripheral Instance Traversal Macros":<73} */')
        header.append('/*' + '-' * 75 + '*/')
        header.append('')

        # Generate GPIO instance macro first (special case)
        if gpio_instances:
            # Sort GPIO instances by port then pin
            gpio_instances_sorted = sorted(gpio_instances, key=lambda x: (x[0], int(x[1])))

            header.append('/**')
            header.append(' * \\brief           GPIO instance traversal macro')
            header.append(' *')
            header.append(' * This macro expands to call the provided function for each enabled')
            header.append(' * GPIO instance. Used by the device registration system.')
            header.append(' *')
            header.append(' * Example:')
            header.append(' *   NX_DEFINE_INSTANCE_NX_GPIO(MY_REGISTER_FUNC)')
            header.append(' *   expands to:')
            examples = [f'MY_REGISTER_FUNC({port}, {pin})' for port, pin in gpio_instances_sorted[:2]]
            if len(gpio_instances_sorted) > 2:
                examples.append('...')
            header.append(' *   ' + ' '.join(examples))
            header.append(' */')

            # Generate the macro definition
            macro_lines = []
            for port, pin in gpio_instances_sorted:
                macro_lines.append(f'    fn({port}, {pin})')

            header.append('#define NX_DEFINE_INSTANCE_NX_GPIO(fn) \\')
            header.append(' \\\n'.join(macro_lines))
            header.append('')

            print(f"Generated GPIO instance macro with {len(gpio_instances_sorted)} instances")

        # Generate other peripheral instance macros
        # Group by platform_peripheral key
        for platform_peripheral_key in sorted(instance_symbols.keys()):
            info = instance_symbols[platform_peripheral_key]
            platform = info['platform']
            peripheral = info['peripheral']
            instances = info['instances']

            # Sort instances (numeric if possible, otherwise alphabetic)
            instances_sorted = sorted(instances, key=lambda x: int(x) if x.isdigit() else x)

            header.append('/**')
            header.append(f' * \\brief           {peripheral} instance traversal macro')
            header.append(' *')
            header.append(' * This macro expands to call the provided function for each enabled')
            header.append(f' * {peripheral} instance. Used by the device registration system.')
            header.append(' *')
            header.append(' * Example:')
            header.append(f' *   NX_DEFINE_INSTANCE_{platform}_{peripheral}(MY_REGISTER_FUNC)')
            header.append(' *   expands to:')

            # Generate example based on instance format
            if instances_sorted and not instances_sorted[0].isdigit():
                # Non-numeric instances like FLASH0, BYTES0
                examples = [f'MY_REGISTER_FUNC({inst})' for inst in instances_sorted[:3]]
            else:
                # Numeric instances like 0, 1, 2
                examples = [f'MY_REGISTER_FUNC({inst})' for inst in instances_sorted[:3]]

            if len(instances_sorted) > 3:
                examples.append('...')
            header.append(' *   ' + ' '.join(examples))
            header.append(' */')

            # Generate the macro definition
            macro_lines = []
            for inst in instances_sorted:
                macro_lines.append(f'    _{platform}_{peripheral}_INSTANCE_{inst}(fn)')

            header.append(f'#define NX_DEFINE_INSTANCE_{platform}_{peripheral}(fn) \\')
            header.append(' \\\n'.join(macro_lines))
            header.append('')

            # Generate helper macros for each instance
            for inst in instances_sorted:
                # Get original suffix if available (for composite peripherals)
                original_suffixes = info.get('original_suffixes', {})
                original_suffix = original_suffixes.get(inst, None)

                # Determine the original config symbol format
                # For composite peripherals like INTERNAL_FLASH, the config is INSTANCE_NX_INTERNAL_FLASH0
                # For regular peripherals like UART, the config is INSTANCE_NX_UART_0
                if original_suffix:
                    # Use original suffix format (e.g., FLASH0, BYTES0)
                    config_symbol = f'NX_CONFIG_INSTANCE_{platform}_{peripheral}{original_suffix}'
                    enabled_symbol = f'NX_CONFIG_{peripheral}{original_suffix}_ENABLED'
                elif inst.isdigit():
                    # Numeric instance with underscore separator
                    config_symbol = f'NX_CONFIG_INSTANCE_{platform}_{peripheral}_{inst}'
                    enabled_symbol = f'NX_CONFIG_{peripheral}{inst}_ENABLED'
                else:
                    # Non-numeric instance like FLASH0, BYTES0 (fallback)
                    config_symbol = f'NX_CONFIG_INSTANCE_{platform}_{peripheral}{inst}'
                    enabled_symbol = f'NX_CONFIG_{peripheral}{inst}_ENABLED'

                header.append(f'#ifdef {config_symbol}')
                header.append(f'#define _{platform}_{peripheral}_INSTANCE_{inst}(fn) fn({inst})')
                header.append(f'#define {enabled_symbol} 1')
                header.append('#else')
                header.append(f'#define _{platform}_{peripheral}_INSTANCE_{inst}(fn)')
                header.append(f'#define {enabled_symbol} 0')
                header.append('#endif')
                header.append('')

    # File footer
    header.append('#ifdef __cplusplus')
    header.append('}')
    header.append('#endif')
    header.append('')
    header.append('#endif /* NEXUS_CONFIG_H */')
    header.append('')

    # Write output file
    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(header))

    print(f'Generated: {output_path}')

    # Build list of generated peripherals
    peripheral_list = []
    if gpio_instances:
        peripheral_list.append('GPIO')
    if instance_symbols:
        peripheral_list.extend(sorted([f'{info["peripheral"]}' for info in instance_symbols.values()]))

    if peripheral_list:
        print(f'Generated instance macros for: {", ".join(peripheral_list)}')


def generate_default_config(kconfig_file, output_path):
    """
    Generate a default configuration header using Kconfig defaults.

    Args:
        kconfig_file: Path to root Kconfig file
        output_path: Path to output header file
    """
    try:
        # Suppress kconfiglib warnings
        class FilteredStderr:
            def __init__(self, original_stderr):
                self.original_stderr = original_stderr

            def write(self, text):
                if 'choice symbol' in text and 'is defined with a prompt outside the choice' in text:
                    return
                if 'default selection' in text and 'is not contained in the choice' in text:
                    return
                self.original_stderr.write(text)

            def flush(self):
                self.original_stderr.flush()

        original_stderr = sys.stderr
        sys.stderr = FilteredStderr(original_stderr)

        try:
            # Parse Kconfig with defaults
            kconf = kconfiglib.Kconfig(kconfig_file)

            # Write default .config to temporary file
            temp_config = '.config_temp_default'
            kconf.write_config(temp_config)
        finally:
            sys.stderr = original_stderr

        # Parse the default config
        config = parse_config_with_kconfig(kconfig_file, temp_config)

        # Clean up temp file
        if os.path.exists(temp_config):
            os.remove(temp_config)

        # Generate header
        generate_header(config, output_path)

    except Exception as e:
        print(f"Error generating default configuration: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Generate nexus_config.h from Kconfig .config file'
    )
    parser.add_argument(
        '--kconfig', '-k',
        default='Kconfig',
        help='Path to root Kconfig file (default: Kconfig)'
    )
    parser.add_argument(
        '--config', '-c',
        default='.config',
        help='Path to .config file (default: .config)'
    )
    parser.add_argument(
        '--output', '-o',
        default='nexus_config.h',
        help='Output header file path (default: nexus_config.h)'
    )
    parser.add_argument(
        '--default', '-d',
        action='store_true',
        help='Generate default configuration (ignore .config)'
    )

    args = parser.parse_args()

    # Check if Kconfig file exists
    if not os.path.exists(args.kconfig):
        print(f'Error: Kconfig file not found: {args.kconfig}')
        return 1

    if args.default:
        print('Generating default configuration from Kconfig...')
        generate_default_config(args.kconfig, args.output)
    elif not os.path.exists(args.config):
        print(f'Warning: Config file not found: {args.config}')
        print('Generating default configuration from Kconfig...')
        generate_default_config(args.kconfig, args.output)
    else:
        print(f'Parsing configuration from {args.config}...')
        config = parse_config_with_kconfig(args.kconfig, args.config)
        if not config:
            print('Warning: No configuration found')
            print('Generating default configuration from Kconfig...')
            generate_default_config(args.kconfig, args.output)
        else:
            generate_header(config, args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())
