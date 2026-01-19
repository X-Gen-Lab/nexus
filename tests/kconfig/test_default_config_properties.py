#!/usr/bin/env python3
"""
Property-based tests for default configuration generation.

Feature: kconfig-architecture-enhancement, Property 16: Default configuration generation idempotency
Validates: Requirements 6.7, 9.4

Property: For any platform, generating the default configuration multiple times
should produce identical results.
"""

import os
import tempfile
import pytest
from hypothesis import given, strategies as st, settings

# Import the generation functions
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts', 'kconfig'))
from generate_hal_config import generate_default_config


@settings(max_examples=10)  # Fewer examples since this is expensive
@given(st.integers(min_value=2, max_value=5))
def test_default_config_idempotency(num_generations):
    """
    Property 16: Default configuration generation idempotency.

    For any platform, generating the default configuration multiple times
    should produce identical results.

    Feature: kconfig-architecture-enhancement, Property 16
    Validates: Requirements 6.7, 9.4
    """
    kconfig_file = 'Kconfig'

    if not os.path.exists(kconfig_file):
        pytest.skip("Kconfig file not found")

    with tempfile.TemporaryDirectory() as tmpdir:
        # Generate default config multiple times
        outputs = []
        for i in range(num_generations):
            output_path = os.path.join(tmpdir, f'config_{i}.h')
            generate_default_config(kconfig_file, output_path)

            # Read the generated file
            with open(output_path, 'r', encoding='utf-8') as f:
                content = f.read()

            # Remove timestamp line for comparison (it will differ)
            lines = content.split('\n')
            filtered_lines = [
                line for line in lines
                if not line.strip().startswith('* Generated:')
            ]
            normalized_content = '\n'.join(filtered_lines)

            outputs.append(normalized_content)

        # Verify all outputs are identical (except timestamp)
        first_output = outputs[0]
        for i, output in enumerate(outputs[1:], 1):
            assert output == first_output, \
                f"Generation {i+1} differs from generation 1"


def test_default_config_deterministic():
    """
    Property: Default configuration is deterministic.

    Generating default configuration twice in succession should produce
    identical results (except for timestamp).

    Feature: kconfig-architecture-enhancement, Property 16
    Validates: Requirements 6.7, 9.4
    """
    kconfig_file = 'Kconfig'

    if not os.path.exists(kconfig_file):
        pytest.skip("Kconfig file not found")

    with tempfile.TemporaryDirectory() as tmpdir:
        # Generate first time
        output1_path = os.path.join(tmpdir, 'config1.h')
        generate_default_config(kconfig_file, output1_path)

        with open(output1_path, 'r', encoding='utf-8') as f:
            content1 = f.read()

        # Generate second time
        output2_path = os.path.join(tmpdir, 'config2.h')
        generate_default_config(kconfig_file, output2_path)

        with open(output2_path, 'r', encoding='utf-8') as f:
            content2 = f.read()

        # Remove timestamps for comparison
        def remove_timestamp(content):
            lines = content.split('\n')
            return '\n'.join([
                line for line in lines
                if not line.strip().startswith('* Generated:')
            ])

        normalized1 = remove_timestamp(content1)
        normalized2 = remove_timestamp(content2)

        assert normalized1 == normalized2, \
            "Default configuration generation is not deterministic"


def test_default_config_contains_platform():
    """
    Property: Default configuration includes platform selection.

    The default configuration should always include a platform selection
    (typically PLATFORM_NATIVE as the default).

    Feature: kconfig-architecture-enhancement, Property 16
    Validates: Requirements 9.1, 9.4
    """
    kconfig_file = 'Kconfig'

    if not os.path.exists(kconfig_file):
        pytest.skip("Kconfig file not found")

    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = os.path.join(tmpdir, 'config.h')
        generate_default_config(kconfig_file, output_path)

        with open(output_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Should have at least one platform defined
        platform_found = False
        platforms = [
            'NX_CONFIG_PLATFORM_NATIVE',
            'NX_CONFIG_PLATFORM_STM32',
            'NX_CONFIG_PLATFORM_GD32',
            'NX_CONFIG_PLATFORM_ESP32',
            'NX_CONFIG_PLATFORM_NRF52',
        ]

        for platform in platforms:
            if f'#define {platform} 1' in content:
                platform_found = True
                break

        assert platform_found, \
            "Default configuration should include a platform selection"


def test_default_config_has_valid_structure():
    """
    Property: Default configuration has valid C header structure.

    The generated default configuration should be a valid C header file
    with proper include guards and structure.

    Feature: kconfig-architecture-enhancement, Property 16
    Validates: Requirements 6.3, 6.4, 6.5
    """
    kconfig_file = 'Kconfig'

    if not os.path.exists(kconfig_file):
        pytest.skip("Kconfig file not found")

    with tempfile.TemporaryDirectory() as tmpdir:
        output_path = os.path.join(tmpdir, 'config.h')
        generate_default_config(kconfig_file, output_path)

        with open(output_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check required structure elements
        assert '#ifndef NEXUS_CONFIG_H' in content
        assert '#define NEXUS_CONFIG_H' in content
        assert '#endif /* NEXUS_CONFIG_H */' in content
        assert '#ifdef __cplusplus' in content
        assert 'extern "C" {' in content
        assert '\\file' in content
        assert '\\brief' in content
        assert 'auto-generated' in content.lower()


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
