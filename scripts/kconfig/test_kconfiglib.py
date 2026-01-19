#!/usr/bin/env python3
"""
Test script to verify kconfiglib installation and basic functionality.

This script tests that kconfiglib is properly installed and can parse
basic Kconfig files.
"""

import sys
import os


def test_kconfiglib_import():
    """Test that kconfiglib can be imported."""
    try:
        import kconfiglib
        version = getattr(kconfiglib, '__version__', 'unknown')
        print(f"✓ kconfiglib imported successfully (version: {version})")
        return True
    except ImportError as e:
        print(f"✗ Failed to import kconfiglib: {e}")
        return False


def test_basic_kconfig_parsing():
    """Test basic Kconfig parsing functionality."""
    try:
        import kconfiglib

        # Create a simple test Kconfig file
        test_kconfig_content = """
mainmenu "Test Configuration"

config TEST_BOOL
    bool "Test boolean option"
    default y
    help
      This is a test boolean option.

config TEST_INT
    int "Test integer option"
    default 42
    range 0 100
    help
      This is a test integer option.

config TEST_STRING
    string "Test string option"
    default "hello"
    help
      This is a test string option.

config TEST_HEX
    hex "Test hex option"
    default 0x1000
    help
      This is a test hex option.
"""

        # Write test Kconfig file
        test_kconfig_path = 'test_Kconfig_temp'
        with open(test_kconfig_path, 'w', encoding='utf-8') as f:
            f.write(test_kconfig_content)

        try:
            # Parse the Kconfig file
            kconf = kconfiglib.Kconfig(test_kconfig_path)
            print("✓ Basic Kconfig file parsed successfully")

            # Test accessing configuration symbols
            test_bool = kconf.syms.get('TEST_BOOL')
            test_int = kconf.syms.get('TEST_INT')
            test_string = kconf.syms.get('TEST_STRING')
            test_hex = kconf.syms.get('TEST_HEX')

            if test_bool and test_int and test_string and test_hex:
                print("✓ Configuration symbols accessed successfully")
                print(f"  TEST_BOOL: {test_bool.str_value} (type: {test_bool.type})")
                print(f"  TEST_INT: {test_int.str_value} (type: {test_int.type})")
                print(f"  TEST_STRING: {test_string.str_value} (type: {test_string.type})")
                print(f"  TEST_HEX: {test_hex.str_value} (type: {test_hex.type})")
                return True
            else:
                print("✗ Failed to access configuration symbols")
                return False

        finally:
            # Clean up test file
            if os.path.exists(test_kconfig_path):
                os.remove(test_kconfig_path)

    except Exception as e:
        print(f"✗ Failed to parse Kconfig: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_config_file_generation():
    """Test .config file generation."""
    try:
        import kconfiglib

        # Create a simple test Kconfig file
        test_kconfig_content = """
config TEST_OPTION
    bool "Test option"
    default y
"""

        test_kconfig_path = 'test_Kconfig_temp'
        test_config_path = 'test_config_temp'

        with open(test_kconfig_path, 'w', encoding='utf-8') as f:
            f.write(test_kconfig_content)

        try:
            # Parse and write config
            kconf = kconfiglib.Kconfig(test_kconfig_path)
            kconf.write_config(test_config_path)

            # Read back the config file
            with open(test_config_path, 'r', encoding='utf-8') as f:
                config_content = f.read()

            if 'CONFIG_TEST_OPTION=y' in config_content:
                print("✓ .config file generation successful")
                return True
            else:
                print("✗ .config file content incorrect")
                return False

        finally:
            # Clean up test files
            for path in [test_kconfig_path, test_config_path]:
                if os.path.exists(path):
                    os.remove(path)

    except Exception as e:
        print(f"✗ Failed to generate .config file: {e}")
        return False


def main():
    """Run all tests."""
    print("=" * 70)
    print("Testing kconfiglib Installation and Functionality")
    print("=" * 70)
    print()

    tests = [
        ("Import Test", test_kconfiglib_import),
        ("Basic Parsing Test", test_basic_kconfig_parsing),
        ("Config Generation Test", test_config_file_generation),
    ]

    results = []
    for test_name, test_func in tests:
        print(f"\n{test_name}:")
        print("-" * 70)
        result = test_func()
        results.append(result)
        print()

    print("=" * 70)
    print("Test Summary:")
    print("=" * 70)
    passed = sum(results)
    total = len(results)
    print(f"Passed: {passed}/{total}")

    if passed == total:
        print("\n✓ All tests passed! kconfiglib is properly installed and functional.")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) failed.")
        return 1


if __name__ == '__main__':
    sys.exit(main())
