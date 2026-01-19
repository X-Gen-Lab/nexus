#!/usr/bin/env python3
"""
Property-based tests for OSAL backend selection configuration.

Feature: kconfig-architecture-enhancement, Property 13: OSAL backend configuration mutual exclusivity
Validates: Requirements 4.1

Property: For any configuration, when one OSAL backend is selected, all other
OSAL backend options should be unselected.
"""

import os
import tempfile
import pytest
from hypothesis import given, strategies as st, settings


# Import kconfiglib
try:
    import kconfiglib
except ImportError:
    pytest.skip("kconfiglib not installed", allow_module_level=True)


# Get the root Kconfig file path
KCONFIG_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', 'Kconfig')


# OSAL backend options
OSAL_BACKENDS = [
    'OSAL_BAREMETAL',
    'OSAL_FREERTOS',
    'OSAL_RTTHREAD',
    'OSAL_ZEPHYR',
    'OSAL_LINUX',
    'OSAL_NATIVE'
]


@st.composite
def osal_backend_selection(draw):
    """Generate an OSAL backend selection."""
    return draw(st.sampled_from(OSAL_BACKENDS))


@settings(max_examples=100)
@given(osal_backend_selection())
def test_osal_backend_mutual_exclusivity(selected_backend):
    """
    Property 13: OSAL backend configuration mutual exclusivity.

    For any OSAL backend selection, only one backend should be enabled
    and all others should be disabled.

    Feature: kconfig-architecture-enhancement, Property 13
    Validates: Requirements 4.1
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected backend
        backend_sym = kconf.syms.get(selected_backend)
        if backend_sym is None:
            pytest.skip(f"OSAL backend symbol {selected_backend} not found")

        # Set the backend value
        backend_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Verify mutual exclusivity
        for backend in OSAL_BACKENDS:
            if backend == selected_backend:
                # Selected backend should be enabled
                assert f'CONFIG_{backend}=y' in config_content, \
                    f"Selected OSAL backend {backend} should be enabled"
            else:
                # Other backends should be disabled or not set
                enabled_pattern = f'CONFIG_{backend}=y'
                assert enabled_pattern not in config_content, \
                    f"OSAL backend {backend} should not be enabled when {selected_backend} is selected"


@settings(max_examples=100)
@given(osal_backend_selection())
def test_osal_backend_name_consistency(selected_backend):
    """
    Property: OSAL backend name is consistent with selection.

    For any OSAL backend selection, the OSAL_BACKEND_NAME config should match
    the selected backend.

    Feature: kconfig-architecture-enhancement, Property 13
    Validates: Requirements 4.1, 4.2
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected backend
        backend_sym = kconf.syms.get(selected_backend)
        if backend_sym is None:
            pytest.skip(f"OSAL backend symbol {selected_backend} not found")

        backend_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Get OSAL_BACKEND_NAME value
        backend_name_sym = kconf.syms.get('OSAL_BACKEND_NAME')
        assert backend_name_sym is not None, "OSAL_BACKEND_NAME symbol not found"

        backend_name = backend_name_sym.str_value

        # Verify backend name matches selection
        expected_names = {
            'OSAL_BAREMETAL': 'baremetal',
            'OSAL_FREERTOS': 'freertos',
            'OSAL_RTTHREAD': 'rtthread',
            'OSAL_ZEPHYR': 'zephyr',
            'OSAL_LINUX': 'linux',
            'OSAL_NATIVE': 'native'
        }

        expected_name = expected_names.get(selected_backend)
        assert backend_name == expected_name, \
            f"OSAL_BACKEND_NAME should be '{expected_name}' for {selected_backend}, got '{backend_name}'"


@settings(max_examples=50)
@given(osal_backend_selection())
def test_only_one_osal_backend_in_config(selected_backend):
    """
    Property: Only one OSAL backend appears as enabled in config.

    For any OSAL backend selection, exactly one backend should appear
    as enabled (=y) in the generated .config file.

    Feature: kconfig-architecture-enhancement, Property 13
    Validates: Requirements 4.1
    """
    # Create a temporary .config file
    with tempfile.TemporaryDirectory() as tmpdir:
        config_path = os.path.join(tmpdir, '.config')

        # Load Kconfig
        kconf = kconfiglib.Kconfig(KCONFIG_ROOT)

        # Set the selected backend
        backend_sym = kconf.syms.get(selected_backend)
        if backend_sym is None:
            pytest.skip(f"OSAL backend symbol {selected_backend} not found")

        backend_sym.set_value('y')

        # Write configuration
        kconf.write_config(config_path)

        # Read back the configuration
        with open(config_path, 'r') as f:
            config_content = f.read()

        # Count how many backends are enabled
        enabled_count = 0
        for backend in OSAL_BACKENDS:
            if f'CONFIG_{backend}=y' in config_content:
                enabled_count += 1

        # Exactly one backend should be enabled
        assert enabled_count == 1, \
            f"Expected exactly 1 OSAL backend enabled, found {enabled_count}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
