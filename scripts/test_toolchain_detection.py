#!/usr/bin/env python3
"""
Test script for toolchain auto-detection.

This script tests the toolchain detection logic with various configurations.

Author: Nexus Team
"""

import os
import sys
import tempfile
from pathlib import Path

# Add scripts directory to path
sys.path.insert(0, os.path.dirname(__file__))

from generate_cmake_preset import (
    parse_kconfig,
    detect_platform,
    detect_toolchain,
    detect_build_type,
    generate_preset_name
)


def create_test_config(config_dict):
    """
    Create a temporary .config file for testing.

    Arguments:
        config_dict: Dictionary of configuration variables

    Returns:
        Path to temporary config file
    """
    fd, path = tempfile.mkstemp(suffix='.config', text=True)

    with os.fdopen(fd, 'w') as f:
        for key, value in config_dict.items():
            if value == 'y':
                f.write(f"{key}=y\n")
            elif value:
                f.write(f'{key}="{value}"\n')

    return path


def test_native_gcc():
    """Test native platform with GCC."""
    print("Test 1: Native platform with GCC")

    config = {
        'CONFIG_PLATFORM_NATIVE': 'y',
        'CONFIG_TOOLCHAIN_GCC': 'y',
        'CONFIG_BUILD_TYPE_DEBUG': 'y',
    }

    config_file = create_test_config(config)

    try:
        parsed = parse_kconfig(config_file)
        platform = detect_platform(parsed)
        toolchain, _ = detect_toolchain(parsed)
        build_type = detect_build_type(parsed)
        preset = generate_preset_name(platform, toolchain, build_type)

        assert platform == 'native', f"Expected 'native', got '{platform}'"
        assert toolchain == 'gcc', f"Expected 'gcc', got '{toolchain}'"
        assert build_type == 'Debug', f"Expected 'Debug', got '{build_type}'"

        print(f"  ✓ Platform: {platform}")
        print(f"  ✓ Toolchain: {toolchain}")
        print(f"  ✓ Build Type: {build_type}")
        print(f"  ✓ Preset: {preset}")
        print()

    finally:
        os.unlink(config_file)


def test_stm32_arm_gcc():
    """Test STM32 platform with ARM GCC."""
    print("Test 2: STM32 platform with ARM GCC")

    config = {
        'CONFIG_PLATFORM_STM32': 'y',
        'CONFIG_TOOLCHAIN_ARM_GCC': 'y',
        'CONFIG_TOOLCHAIN_FILE': 'cmake/toolchains/arm-none-eabi.cmake',
        'CONFIG_BUILD_TYPE_RELEASE': 'y',
    }

    config_file = create_test_config(config)

    try:
        parsed = parse_kconfig(config_file)
        platform = detect_platform(parsed)
        toolchain, toolchain_file = detect_toolchain(parsed)
        build_type = detect_build_type(parsed)
        preset = generate_preset_name(platform, toolchain, build_type)

        assert platform == 'stm32', f"Expected 'stm32', got '{platform}'"
        assert toolchain == 'arm-gcc', f"Expected 'arm-gcc', got '{toolchain}'"
        assert build_type == 'Release', f"Expected 'Release', got '{build_type}'"
        assert toolchain_file == 'cmake/toolchains/arm-none-eabi.cmake'

        print(f"  ✓ Platform: {platform}")
        print(f"  ✓ Toolchain: {toolchain}")
        print(f"  ✓ Build Type: {build_type}")
        print(f"  ✓ Preset: {preset}")
        print(f"  ✓ Toolchain File: {toolchain_file}")
        print()

    finally:
        os.unlink(config_file)


def test_stm32_iar():
    """Test STM32 platform with IAR."""
    print("Test 3: STM32 platform with IAR")

    config = {
        'CONFIG_PLATFORM_STM32': 'y',
        'CONFIG_TOOLCHAIN_IAR': 'y',
        'CONFIG_TOOLCHAIN_FILE': 'cmake/toolchains/iar-arm.cmake',
        'CONFIG_BUILD_TYPE_MINSIZEREL': 'y',
    }

    config_file = create_test_config(config)

    try:
        parsed = parse_kconfig(config_file)
        platform = detect_platform(parsed)
        toolchain, toolchain_file = detect_toolchain(parsed)
        build_type = detect_build_type(parsed)
        preset = generate_preset_name(platform, toolchain, build_type)

        assert platform == 'stm32', f"Expected 'stm32', got '{platform}'"
        assert toolchain == 'iar', f"Expected 'iar', got '{toolchain}'"
        assert build_type == 'MinSizeRel', f"Expected 'MinSizeRel', got '{build_type}'"
        assert toolchain_file == 'cmake/toolchains/iar-arm.cmake'

        print(f"  ✓ Platform: {platform}")
        print(f"  ✓ Toolchain: {toolchain}")
        print(f"  ✓ Build Type: {build_type}")
        print(f"  ✓ Preset: {preset}")
        print(f"  ✓ Toolchain File: {toolchain_file}")
        print()

    finally:
        os.unlink(config_file)


def test_native_msvc():
    """Test native platform with MSVC."""
    print("Test 4: Native platform with MSVC")

    config = {
        'CONFIG_PLATFORM_NATIVE': 'y',
        'CONFIG_TOOLCHAIN_MSVC': 'y',
        'CONFIG_BUILD_TYPE_RELWITHDEBINFO': 'y',
    }

    config_file = create_test_config(config)

    try:
        parsed = parse_kconfig(config_file)
        platform = detect_platform(parsed)
        toolchain, _ = detect_toolchain(parsed)
        build_type = detect_build_type(parsed)
        preset = generate_preset_name(platform, toolchain, build_type)

        assert platform == 'native', f"Expected 'native', got '{platform}'"
        assert toolchain == 'msvc', f"Expected 'msvc', got '{toolchain}'"
        assert build_type == 'RelWithDebInfo', f"Expected 'RelWithDebInfo', got '{build_type}'"

        print(f"  ✓ Platform: {platform}")
        print(f"  ✓ Toolchain: {toolchain}")
        print(f"  ✓ Build Type: {build_type}")
        print(f"  ✓ Preset: {preset}")
        print()

    finally:
        os.unlink(config_file)


def main():
    """Run all tests."""
    print("=" * 60)
    print("Toolchain Auto-Detection Test Suite")
    print("=" * 60)
    print()

    try:
        test_native_gcc()
        test_stm32_arm_gcc()
        test_stm32_iar()
        test_native_msvc()

        print("=" * 60)
        print("All tests passed! ✓")
        print("=" * 60)

        return 0

    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        return 1

    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
