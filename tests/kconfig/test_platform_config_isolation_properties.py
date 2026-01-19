#!/usr/bin/env python3
"""
Property-based tests for platform-specific configuration isolation.

Feature: kconfig-architecture-enhancement, Property 2: Platform-specific configuration isolation
Validates: Requirements 2.2

Property: For any platform selection, the generated configuration should only
contain that platform's specific configuration symbols, not other platforms' symbols.
"""

import os
import tempfile
import re
import pytest
from hypothesis import given, strategies as st, settings


# Import kconfiglib
try:
    import kconfiglib
except ImportError:
    pytest.skip("kconfiglib not installed", allow_module_level=True)


# Get the root Kconfig file path
KCONFIG_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', 'Kconfig')


# Platform options and their prefixes
PLATFORM_CONFIGS = {
    'PLATFORM_NATIVE': 'NATIVE_',
    'PLATFORM_STM32': 'STM32_',
    'PLATFORM_GD32': 'GD32_',
    'PLATFORM_ESP32': 'ESP32_',
    'PLATFORM_NRF52': 'NRF52_'
}


@st.composite
def platform_selection(draw):
    """Generate a platform selection."""
    return draw(st.sampled_from(list(PLATFORM_CONFIGS.keys())))


@settings(max_examples=100)
@given(platform_selection())
def test_platform_config_isolation(selected_platform):
    """
    Property 2: Platform-specific configuration isolation.

    For any platform selection, the generated configuration should only
    contain configuration symbols specific to that platform, not symbols
    from other platforms.

    Feature: kconfig-architecture-enhancement, Property 2
    Validates: Requirements 2.2
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected platform
        platform_sym = kconf.syms.get(selected_platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {selected_platform} not found")

        platform_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Get the prefix for the selected platform
        selected_prefix = PLATFORM_CONFIGS[selected_platform]

        # Check that selected platform's configs are present
        # (if they have any default values)
        selected_pattern = rf'CONFIG_{re.escape(selected_prefix)}'
        has_selected_configs = re.search(selected_pattern, config_content)

        # Check that other platforms' configs are NOT present
        for platform, prefix in PLATFORM_CONFIGS.items():
            if platform != selected_platform:
                # Other platform-specific configs should not appear
                other_pattern = rf'CONFIG_{re.escape(prefix)}\w+=\w+'
                matches = re.findall(other_pattern, config_content)

                # Filter out false positives (e.g., if one prefix is substring of another)
                # Only fail if we find actual config assignments
                for match in matches:
                    # Make sure it's not a comment
                    line_start = config_content.rfind('\n', 0, config_content.find(match))
                    line = config_content[line_start:config_content.find('\n', line_start + 1)]
                    if not line.strip().startswith('#'):
                        pytest.fail(
                            f"Found {platform} config in {selected_platform} configuration: {match}"
                        )


@settings(max_examples=100)
@given(platform_selection())
def test_platform_namespace_consistency(selected_platform):
    """
    Property: Platform configuration symbols use consistent namespace.

    For any platform selection, all platform-specific configuration symbols
    should use the platform's prefix consistently.

    Feature: kconfig-architecture-enhancement, Property 2
    Validates: Requirements 2.2, 11.4
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected platform
        platform_sym = kconf.syms.get(selected_platform)
        if platform_sym is None:
            pytest.skip(f"Platform symbol {selected_platform} not found")

        platform_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Get the prefix for the selected platform
        selected_prefix = PLATFORM_CONFIGS[selected_platform]

        # Find all config symbols for the selected platform
        pattern = rf'CONFIG_{re.escape(selected_prefix)}(\w+)='
        matches = re.findall(pattern, config_content)

        # All platform-specific symbols should follow the naming pattern
        # <PLATFORM>_<COMPONENT>_<PARAMETER> or <PLATFORM>_<PARAMETER>
        for symbol_suffix in matches:
            full_symbol = f'{selected_prefix}{symbol_suffix}'

            # Symbol should not have mixed prefixes
            for other_platform, other_prefix in PLATFORM_CONFIGS.items():
                if other_platform != selected_platform:
                    assert not other_prefix in symbol_suffix, \
                        f"Platform symbol {full_symbol} contains other platform prefix {other_prefix}"


@settings(max_examples=50)
@given(platform_selection())
def test_no_cross_platform_dependencies(selected_platform):
    """
    Property: No cross-platform configuration dependencies.

    For any platform selection, platform-specific configurations should not
    depend on other platforms' configurations.

    Feature: kconfig-architecture-enhancement, Property 2
    Validates: Requirements 2.2
    """
    # Load Kconfig
    kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

    # Set the selected platform
    platform_sym = kconf.syms.get(selected_platform)
    if platform_sym is None:
        pytest.skip(f"Platform symbol {selected_platform} not found")

    platform_sym.set_value('y')

    # Get the prefix for the selected platform
    selected_prefix = PLATFORM_CONFIGS[selected_platform]

    # Check all symbols with the selected platform prefix
    for sym_name, sym in kconf.syms.items():
        if sym_name.startswith(selected_prefix):
            # Check dependencies
            if hasattr(sym, 'direct_dep') and sym.direct_dep is not None:
                # Get dependency expression as string
                dep_str = str(sym.direct_dep)

                # Check if it depends on other platforms
                for other_platform, other_prefix in PLATFORM_CONFIGS.items():
                    if other_platform != selected_platform:
                        # Should not depend on other platform symbols
                        assert other_prefix not in dep_str, \
                            f"Symbol {sym_name} depends on {other_platform} configuration"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
