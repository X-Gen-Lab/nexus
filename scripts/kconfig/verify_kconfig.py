#!/usr/bin/env python3
"""
Kconfig Verification Script

This script verifies the Kconfig configuration hierarchy for the Native platform.
It checks:
1. All peripheral Kconfig files are correctly imported
2. Dependencies are correctly set (depends on PLATFORM_NATIVE)
3. The hierarchy is complete
"""

import os
import sys
import re
from pathlib import Path

# Color codes for output
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
RESET = '\033[0m'

def print_success(msg):
    print(f"{GREEN}✓{RESET} {msg}")

def print_error(msg):
    print(f"{RED}✗{RESET} {msg}")

def print_warning(msg):
    print(f"{YELLOW}⚠{RESET} {msg}")

def check_file_exists(filepath):
    """Check if a file exists."""
    return os.path.isfile(filepath)

def read_file(filepath):
    """Read file content."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except Exception as e:
        print_error(f"Failed to read {filepath}: {e}")
        return None

def check_platform_kconfig():
    """Check platforms/Kconfig exists and has correct structure."""
    print("\n=== Checking Platform Top-Level Kconfig ===")

    filepath = "platforms/Kconfig"
    if not check_file_exists(filepath):
        print_error(f"{filepath} does not exist")
        return False

    print_success(f"{filepath} exists")

    content = read_file(filepath)
    if content is None:
        return False

    # Check for platform selection
    if 'choice' not in content or 'PLATFORM_NATIVE' not in content:
        print_error("Platform selection not found in platforms/Kconfig")
        return False

    print_success("Platform selection found")

    # Check for Native platform import (using rsource)
    if 'rsource "native/Kconfig"' not in content and 'source "platforms/native/Kconfig"' not in content:
        print_error("Native platform Kconfig not sourced")
        return False

    print_success("Native platform Kconfig sourced")

    return True

def check_native_kconfig():
    """Check platforms/native/Kconfig exists and has correct structure."""
    print("\n=== Checking Native Platform Kconfig ===")

    filepath = "platforms/native/Kconfig"
    if not check_file_exists(filepath):
        print_error(f"{filepath} does not exist")
        return False

    print_success(f"{filepath} exists")

    content = read_file(filepath)
    if content is None:
        return False

    # Check for PLATFORM_NATIVE dependency
    if 'if PLATFORM_NATIVE' not in content:
        print_error("PLATFORM_NATIVE guard not found")
        return False

    print_success("PLATFORM_NATIVE guard found")

    # Check for peripheral imports
    peripherals = ['uart', 'gpio', 'spi', 'i2c', 'timer', 'adc', 'dac']
    missing_peripherals = []

    for peripheral in peripherals:
        source_line = f'source "platforms/native/src/{peripheral}/Kconfig"'
        if source_line not in content:
            missing_peripherals.append(peripheral)

    if missing_peripherals:
        print_error(f"Missing peripheral imports: {', '.join(missing_peripherals)}")
        return False

    print_success(f"All {len(peripherals)} peripheral Kconfig files sourced")

    return True

def check_peripheral_kconfig(peripheral):
    """Check individual peripheral Kconfig file."""
    filepath = f"platforms/native/src/{peripheral}/Kconfig"

    if not check_file_exists(filepath):
        print_warning(f"{filepath} does not exist (may not be implemented yet)")
        return None

    content = read_file(filepath)
    if content is None:
        return False

    # Check for PLATFORM_NATIVE dependency
    if 'depends on PLATFORM_NATIVE' not in content:
        print_error(f"{peripheral}: Missing 'depends on PLATFORM_NATIVE'")
        return False

    # Check for peripheral enable config (flexible matching)
    peripheral_upper = peripheral.upper()
    has_enable_config = (
        f'config {peripheral_upper}_NATIVE' in content or
        f'config NATIVE_{peripheral_upper}' in content or
        f'config NATIVE_{peripheral_upper}_ENABLE' in content or
        f'menuconfig {peripheral_upper}_NATIVE' in content or
        f'menuconfig NATIVE_{peripheral_upper}' in content
    )

    if not has_enable_config:
        print_error(f"{peripheral}: Missing peripheral enable config")
        return False

    return True

def check_all_peripheral_kconfigs():
    """Check all peripheral Kconfig files."""
    print("\n=== Checking Peripheral Kconfig Files ===")

    peripherals = ['uart', 'gpio', 'spi', 'i2c', 'timer', 'adc', 'dac',
                   'flash', 'rtc', 'watchdog', 'crc', 'usb', 'sdio', 'option_bytes']

    success_count = 0
    warning_count = 0
    error_count = 0

    for peripheral in peripherals:
        result = check_peripheral_kconfig(peripheral)
        if result is True:
            print_success(f"{peripheral}: Kconfig valid")
            success_count += 1
        elif result is None:
            warning_count += 1
        else:
            error_count += 1

    print(f"\nPeripheral Kconfig Summary:")
    print(f"  Valid: {success_count}")
    print(f"  Not implemented: {warning_count}")
    print(f"  Errors: {error_count}")

    return error_count == 0

def check_kconfig_hierarchy():
    """Check the complete Kconfig hierarchy."""
    print("\n=== Checking Kconfig Hierarchy ===")

    # Check hierarchy: platforms/Kconfig -> platforms/native/Kconfig -> peripheral Kconfigs

    # Level 1: platforms/Kconfig
    if not check_platform_kconfig():
        return False

    # Level 2: platforms/native/Kconfig
    if not check_native_kconfig():
        return False

    # Level 3: Peripheral Kconfigs
    if not check_all_peripheral_kconfigs():
        return False

    print_success("\nKconfig hierarchy is complete and valid!")
    return True

def main():
    """Main verification function."""
    print("=" * 60)
    print("Kconfig Configuration Verification")
    print("=" * 60)

    # Change to repository root
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent
    os.chdir(repo_root)

    success = check_kconfig_hierarchy()

    print("\n" + "=" * 60)
    if success:
        print(f"{GREEN}All Kconfig checks passed!{RESET}")
        return 0
    else:
        print(f"{RED}Some Kconfig checks failed!{RESET}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
