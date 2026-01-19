#!/usr/bin/env python3
"""
Test script to verify Kconfig file structure and parsing.

This script tests that the root Kconfig file and all included files
can be parsed successfully by kconfiglib.
"""

import sys
import os


def test_kconfig_parsing():
    """Test that the root Kconfig file can be parsed."""
    try:
        import kconfiglib

        # Parse the root Kconfig file
        print("Parsing root Kconfig file...")
        kconf = kconfiglib.Kconfig('Kconfig')

        print("✓ Root Kconfig file parsed successfully")

        # List all configuration symbols
        print(f"\nFound {len(kconf.syms)} configuration symbols:")

        # Show some key symbols
        key_symbols = [
            'PLATFORM_NATIVE',
            'PLATFORM_STM32',
            'PLATFORM_NAME',
            'HAL_ENABLE',
            'OSAL_BAREMETAL',
            'OSAL_FREERTOS',
        ]

        for sym_name in key_symbols:
            sym = kconf.syms.get(sym_name)
            if sym:
                print(f"  {sym_name}: {sym.str_value} (type: {sym.type})")
            else:
                print(f"  {sym_name}: NOT FOUND")

        # Test generating a default .config
        print("\nGenerating default .config...")
        test_config_path = 'test_default.config'
        kconf.write_config(test_config_path)

        # Read back and verify
        with open(test_config_path, 'r', encoding='utf-8') as f:
            config_content = f.read()

        if 'CONFIG_PLATFORM_NATIVE=y' in config_content:
            print("✓ Default configuration generated successfully")
            print(f"  Default platform: NATIVE")
        else:
            print("✗ Default configuration incorrect")
            return False

        # Clean up
        os.remove(test_config_path)

        return True

    except Exception as e:
        print(f"✗ Failed to parse Kconfig: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """Run the test."""
    print("=" * 70)
    print("Testing Kconfig Structure")
    print("=" * 70)
    print()

    if test_kconfig_parsing():
        print("\n✓ Kconfig structure test passed!")
        return 0
    else:
        print("\n✗ Kconfig structure test failed!")
        return 1


if __name__ == '__main__':
    sys.exit(main())
