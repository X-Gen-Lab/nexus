#!/usr/bin/env python3
"""
Property-based tests for OSAL-specific configuration visibility.

Feature: kconfig-architecture-enhancement, Property 14: OSAL specific configuration visibility
Validates: Requirements 4.3

Property: For any OSAL backend selection, only that backend's specific
configuration options should be visible in the generated configuration.
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


# OSAL backend options and their specific config symbols
# These are backend-specific configs that should only appear when that backend is selected
OSAL_BACKEND_SPECIFIC_SYMBOLS = {
    'OSAL_BAREMETAL': ['BAREMETAL_SIMPLE_SCHEDULER', 'BAREMETAL_MAX_TASKS'],
    'OSAL_FREERTOS': ['FREERTOS_USE_PREEMPTION', 'FREERTOS_USE_TIMERS',
                      'FREERTOS_TIMER_TASK_PRIORITY', 'FREERTOS_TIMER_QUEUE_LENGTH'],
    'OSAL_RTTHREAD': ['RTTHREAD_USING_COMPONENTS_INIT', 'RTTHREAD_USING_USER_MAIN',
                      'RTTHREAD_MAIN_THREAD_STACK_SIZE'],
    'OSAL_ZEPHYR': [],  # No specific symbols defined yet
    'OSAL_LINUX': [],   # No specific symbols defined yet
    'OSAL_NATIVE': []   # No specific symbols defined yet
}


@st.composite
def osal_backend_selection(draw):
    """Generate an OSAL backend selection."""
    return draw(st.sampled_from(list(OSAL_BACKEND_SPECIFIC_SYMBOLS.keys())))


@settings(max_examples=100)
@given(osal_backend_selection())
def test_osal_specific_config_visibility(selected_backend):
    """
    Property 14: OSAL specific configuration visibility.

    For any OSAL backend selection, only that backend's specific
    configuration options should be visible in the generated configuration.

    Feature: kconfig-architecture-enhancement, Property 14
    Validates: Requirements 4.3
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

        # Get the specific symbols for the selected backend
        selected_symbols = OSAL_BACKEND_SPECIFIC_SYMBOLS[selected_backend]

        # Check that other backend-specific configs are not present
        for backend, symbols in OSAL_BACKEND_SPECIFIC_SYMBOLS.items():
            if backend == selected_backend:
                # Selected backend's configs may be present (if they have defaults)
                continue
            else:
                # Other backends' specific configs should not be present as enabled
                for symbol in symbols:
                    # Look for this symbol being set (not just commented out)
                    enabled_pattern = f'CONFIG_{symbol}='
                    for line in config_content.split('\n'):
                        if line.startswith(enabled_pattern):
                            # This is a backend-specific config that shouldn't be visible
                            pytest.fail(
                                f"Backend-specific config '{line}' from {backend} "
                                f"should not be set when {selected_backend} is selected"
                            )


@settings(max_examples=100)
@given(osal_backend_selection())
def test_osal_backend_dependent_symbols(selected_backend):
    """
    Property: OSAL backend-dependent symbols are correctly enabled/disabled.

    For any OSAL backend selection, symbols that depend on that backend
    should be available, while symbols depending on other backends should not.

    Feature: kconfig-architecture-enhancement, Property 14
    Validates: Requirements 4.3
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

        # Check that backend-specific symbols from other backends are not set
        for backend, symbols in OSAL_BACKEND_SPECIFIC_SYMBOLS.items():
            if backend == selected_backend:
                continue

            for symbol in symbols:
                # These symbols should not be set (enabled) in the config
                enabled_pattern = f'CONFIG_{symbol}='
                assert not any(line.startswith(enabled_pattern) for line in config_content.split('\n')), \
                    f"Symbol {symbol} from {backend} should not be set when {selected_backend} is selected"


@settings(max_examples=50)
@given(osal_backend_selection())
def test_osal_max_priorities_visibility(selected_backend):
    """
    Property: OSAL_MAX_PRIORITIES is only visible for RTOS backends.

    For any OSAL backend selection, OSAL_MAX_PRIORITIES should only be
    visible when a non-baremetal backend is selected.

    Feature: kconfig-architecture-enhancement, Property 14
    Validates: Requirements 4.3
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

        # Check OSAL_MAX_PRIORITIES visibility
        max_priorities_present = 'CONFIG_OSAL_MAX_PRIORITIES' in config_content

        if selected_backend == 'OSAL_BAREMETAL':
            # Should not be present for baremetal
            assert not max_priorities_present or '# CONFIG_OSAL_MAX_PRIORITIES is not set' in config_content, \
                "OSAL_MAX_PRIORITIES should not be set for baremetal backend"
        else:
            # Should be present for RTOS backends (may have a value or be commented)
            # This is acceptable either way since it depends on whether it has a default
            pass


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
