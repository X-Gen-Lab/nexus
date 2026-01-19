#!/usr/bin/env python3
"""
Generate nexus_config.h from Kconfig .config file.

This script uses kconfiglib to parse Kconfig files and generate a C header
file with NX_CONFIG_ prefixed macros for the Nexus project.

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
    instance_symbols = {}  # Track INSTANCE_NX_* symbols for macro generation

    for key, value in config.items():
        # Skip if not set
        if value is None:
            continue

        # Convert CONFIG_xxx to NX_CONFIG_xxx
        if key.startswith('CONFIG_'):
            nx_key = key.replace('CONFIG_', 'NX_CONFIG_', 1)
        else:
            nx_key = f'NX_CONFIG_{key}'

        # Track instance symbols for macro generation
        if 'INSTANCE_NX_' in nx_key and value is True:
            # Extract peripheral type and instance number
            # e.g., NX_CONFIG_INSTANCE_NX_UART_0 -> UART, 0
            parts = nx_key.replace('NX_CONFIG_INSTANCE_NX_', '').split('_')
            if len(parts) >= 2:
                peripheral = '_'.join(parts[:-1])  # UART, SPI, GPIO, etc.
                instance_num = parts[-1]  # 0, 1, 2, etc.

                if peripheral not in instance_symbols:
                    instance_symbols[peripheral] = []
                instance_symbols[peripheral].append(instance_num)

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
    if instance_symbols:
        header.append('/*' + '-' * 75 + '*/')
        header.append(f'/* {"Peripheral Instance Traversal Macros":<73} */')
        header.append('/*' + '-' * 75 + '*/')
        header.append('')

        for peripheral in sorted(instance_symbols.keys()):
            instances = sorted(instance_symbols[peripheral], key=lambda x: int(x) if x.isdigit() else x)

            header.append('/**')
            header.append(f' * \\brief           {peripheral} instance traversal macro')
            header.append(' *')
            header.append(' * This macro expands to call the provided function for each enabled')
            header.append(f' * {peripheral} instance. Used by the device registration system.')
            header.append(' *')
            header.append(' * Example:')
            header.append(f' *   NX_DEFINE_INSTANCE_NX_{peripheral}(MY_REGISTER_FUNC)')
            header.append(' *   expands to:')
            instances_str = ' '.join([f'MY_REGISTER_FUNC({i})' for i in instances[:3]])
            if len(instances) > 3:
                instances_str += ' ...'
            header.append(f' *   {instances_str}')
            header.append(' */')

            # Generate the macro definition
            macro_lines = []
            for inst in instances:
                macro_lines.append(f'    _NX_{peripheral}_INSTANCE_{inst}(fn)')

            header.append(f'#define NX_DEFINE_INSTANCE_NX_{peripheral}(fn) \\')
            header.append(' \\\n'.join(macro_lines))
            header.append('')

            # Generate helper macros for each instance
            for inst in instances:
                header.append(f'#ifdef NX_CONFIG_INSTANCE_NX_{peripheral}_{inst}')
                header.append(f'#define _NX_{peripheral}_INSTANCE_{inst}(fn) fn({inst})')
                header.append(f'#define NX_CONFIG_{peripheral}{inst}_ENABLED 1')
                header.append('#else')
                header.append(f'#define _NX_{peripheral}_INSTANCE_{inst}(fn)')
                header.append(f'#define NX_CONFIG_{peripheral}{inst}_ENABLED 0')
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
    if instance_symbols:
        print(f'Generated instance macros for: {", ".join(sorted(instance_symbols.keys()))}')


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
