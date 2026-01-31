#!/usr/bin/env python3
"""
Generate CMake preset based on Kconfig configuration.

This script reads the .config file and generates an appropriate CMake preset
that matches the configured platform and toolchain.

Author: Nexus Team
"""

import os
import sys
import json
import argparse
from pathlib import Path


def parse_kconfig(config_file):
    """
    Parse Kconfig .config file and extract configuration.

    Arguments:
        config_file: Path to .config file

    Returns:
        Dictionary of configuration variables
    """
    config = {}

    if not os.path.exists(config_file):
        print(f"Error: Config file not found: {config_file}", file=sys.stderr)
        return config

    with open(config_file, 'r') as f:
        for line in f:
            line = line.strip()

            # Skip comments and empty lines
            if not line or line.startswith('#'):
                continue

            # Parse CONFIG_XXX=value
            if '=' in line:
                key, value = line.split('=', 1)
                # Remove quotes from string values
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                config[key] = value

    return config


def detect_platform(config):
    """
    Detect platform from Kconfig.

    Arguments:
        config: Configuration dictionary

    Returns:
        Platform name (e.g., 'native', 'stm32')
    """
    if config.get('CONFIG_PLATFORM_NATIVE') == 'y':
        return 'native'
    elif config.get('CONFIG_PLATFORM_STM32') == 'y':
        return 'stm32'
    elif config.get('CONFIG_PLATFORM_GD32') == 'y':
        return 'gd32'
    elif config.get('CONFIG_PLATFORM_ESP32') == 'y':
        return 'esp32'
    elif config.get('CONFIG_PLATFORM_NRF52') == 'y':
        return 'nrf52'
    else:
        return 'unknown'


def detect_toolchain(config):
    """
    Detect toolchain from Kconfig.

    Arguments:
        config: Configuration dictionary

    Returns:
        Tuple of (toolchain_name, toolchain_file)
    """
    if config.get('CONFIG_TOOLCHAIN_GCC') == 'y':
        return ('gcc', None)
    elif config.get('CONFIG_TOOLCHAIN_CLANG') == 'y':
        return ('clang', None)
    elif config.get('CONFIG_TOOLCHAIN_MSVC') == 'y':
        return ('msvc', None)
    elif config.get('CONFIG_TOOLCHAIN_ARM_GCC') == 'y':
        return ('arm-gcc', 'cmake/toolchains/arm-none-eabi.cmake')
    elif config.get('CONFIG_TOOLCHAIN_ARM_CLANG') == 'y':
        return ('arm-clang', 'cmake/toolchains/armclang.cmake')
    elif config.get('CONFIG_TOOLCHAIN_IAR') == 'y':
        return ('iar', 'cmake/toolchains/iar-arm.cmake')
    else:
        return ('unknown', None)


def detect_build_type(config):
    """
    Detect build type from Kconfig.

    Arguments:
        config: Configuration dictionary

    Returns:
        Build type (e.g., 'Debug', 'Release')
    """
    if config.get('CONFIG_BUILD_TYPE_DEBUG') == 'y':
        return 'Debug'
    elif config.get('CONFIG_BUILD_TYPE_RELEASE') == 'y':
        return 'Release'
    elif config.get('CONFIG_BUILD_TYPE_MINSIZEREL') == 'y':
        return 'MinSizeRel'
    elif config.get('CONFIG_BUILD_TYPE_RELWITHDEBINFO') == 'y':
        return 'RelWithDebInfo'
    else:
        return 'Debug'


def generate_preset_name(platform, toolchain, build_type):
    """
    Generate preset name.

    Arguments:
        platform: Platform name
        toolchain: Toolchain name
        build_type: Build type

    Returns:
        Preset name
    """
    # Simplify toolchain name
    toolchain_short = {
        'gcc': 'gcc',
        'clang': 'clang',
        'msvc': 'msvc',
        'arm-gcc': 'arm',
        'arm-clang': 'armclang',
        'iar': 'iar'
    }.get(toolchain, toolchain)

    # Simplify build type
    build_short = build_type.lower()

    if platform == 'native':
        # For native, include host OS
        import platform as plat
        host_os = plat.system().lower()
        return f"{host_os}-{toolchain_short}-{build_short}"
    else:
        # For embedded, use platform name
        return f"{platform}-{toolchain_short}-{build_short}"


def generate_cmake_preset(config_file, output_file=None):
    """
    Generate CMake preset from Kconfig.

    Arguments:
        config_file: Path to .config file
        output_file: Optional output file for preset

    Returns:
        Generated preset name or None on error
    """
    # Parse Kconfig
    config = parse_kconfig(config_file)
    if not config:
        print("Error: Failed to parse Kconfig", file=sys.stderr)
        return None

    # Detect configuration
    platform = detect_platform(config)
    toolchain, toolchain_file = detect_toolchain(config)
    build_type = detect_build_type(config)

    if platform == 'unknown':
        print("Error: No platform selected in Kconfig", file=sys.stderr)
        return None

    if toolchain == 'unknown':
        print("Error: No toolchain selected in Kconfig", file=sys.stderr)
        return None

    # Generate preset name
    preset_name = generate_preset_name(platform, toolchain, build_type)

    # Print information
    print(f"Detected configuration:")
    print(f"  Platform:   {platform}")
    print(f"  Toolchain:  {toolchain}")
    print(f"  Build Type: {build_type}")
    print(f"  Preset:     {preset_name}")

    if toolchain_file:
        print(f"  Toolchain File: {toolchain_file}")

    # Generate command
    cmd_parts = ['cmake', '--preset', preset_name]

    print(f"\nRecommended command:")
    print(f"  {' '.join(cmd_parts)}")

    # Optionally write to file
    if output_file:
        with open(output_file, 'w') as f:
            f.write(preset_name + '\n')
        print(f"\nPreset name written to: {output_file}")

    return preset_name


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description='Generate CMake preset from Kconfig configuration'
    )
    parser.add_argument(
        '--config',
        default='.config',
        help='Path to Kconfig .config file (default: .config)'
    )
    parser.add_argument(
        '--output',
        help='Output file for preset name'
    )

    args = parser.parse_args()

    # Generate preset
    preset_name = generate_cmake_preset(args.config, args.output)

    if preset_name:
        return 0
    else:
        return 1


if __name__ == '__main__':
    sys.exit(main())
