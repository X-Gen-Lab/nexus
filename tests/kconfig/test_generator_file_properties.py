#!/usr/bin/env python3
r"""
Property-based tests for file generation.

Feature: kconfig-naming-standard
Property 5: Help text existence
Property 6: File structure ordering
Property 7: Generation idempotency
Validates: Requirements 5.2, 5.4, 5.5, 6.1, 6.4, 7.2, 7.3

Properties:
- Property 5: For any configuration item (bool, int, hex, string types), it should
  contain the help keyword and non-empty help text.
- Property 6: For any peripheral Kconfig file, its content should be organized in
  the order: file header comment → peripheral enable → global config → instance 0
  → instance 1 → ...
- Property 7: For any peripheral template configuration, generating with the same
  parameters multiple times should produce exactly the same Kconfig file content.
"""

import sys
import os
import re
import tempfile
import pytest
from hypothesis import given, strategies as st, settings

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.generator import (
    KconfigGenerator,
    PeripheralTemplate,
    ParameterConfig,
    ChoiceConfig
)


# Strategy for generating valid peripheral names
@st.composite
def peripheral_name(draw):
    """Generate a valid peripheral name."""
    names = ["UART", "SPI", "I2C", "GPIO", "ADC", "DAC", "TIMER", "PWM", "CAN"]
    return draw(st.sampled_from(names))


# Strategy for generating valid platform names
@st.composite
def platform_name(draw):
    """Generate a valid platform name."""
    names = ["NATIVE", "STM32", "GD32", "NRF52", "ESP32"]
    return draw(st.sampled_from(names))


# Strategy for generating parameter configs
@st.composite
def parameter_config(draw):
    """Generate a valid parameter configuration."""
    param_types = ["int", "hex", "bool", "string"]
    param_type = draw(st.sampled_from(param_types))

    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), whitelist_characters='_'),
        min_size=3,
        max_size=15
    ))

    if param_type == "int":
        default = draw(st.integers(min_value=0, max_value=1000000))
        has_range = draw(st.booleans())
        if has_range:
            range_min = draw(st.integers(min_value=0, max_value=100))
            range_max = draw(st.integers(min_value=range_min + 1, max_value=range_min + 1000))
            return ParameterConfig(name, param_type, default, (range_min, range_max), "Test parameter")
        else:
            return ParameterConfig(name, param_type, default, None, "Test parameter")
    elif param_type == "hex":
        default = draw(st.integers(min_value=0, max_value=0xFFFF))
        return ParameterConfig(name, param_type, default, None, "Test parameter")
    elif param_type == "bool":
        default = draw(st.booleans())
        return ParameterConfig(name, param_type, default, None, "Test parameter")
    else:  # string
        default = draw(st.text(min_size=1, max_size=20))
        return ParameterConfig(name, param_type, default, None, "Test parameter")


# Strategy for generating choice configs
@st.composite
def choice_config(draw):
    """Generate a valid choice configuration."""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), whitelist_characters='_'),
        min_size=3,
        max_size=15
    ))

    num_options = draw(st.integers(min_value=2, max_value=5))
    options = [f"OPTION_{i}" for i in range(num_options)]
    default = options[0]

    values = {opt: i for i, opt in enumerate(options)}

    return ChoiceConfig(name, options, default, "Test choice", values)


# Strategy for generating peripheral templates
@st.composite
def peripheral_template(draw):
    """Generate a valid peripheral template."""
    periph = draw(peripheral_name())
    platform = draw(platform_name())
    max_inst = draw(st.integers(min_value=1, max_value=4))
    inst_type = draw(st.sampled_from(["numeric", "alpha"]))

    num_params = draw(st.integers(min_value=1, max_value=3))
    params = [draw(parameter_config()) for _ in range(num_params)]

    num_choices = draw(st.integers(min_value=0, max_value=2))
    choices = [draw(choice_config()) for _ in range(num_choices)]

    return PeripheralTemplate(
        name=periph,
        platform=platform,
        max_instances=max_inst,
        instance_type=inst_type,
        parameters=params,
        choices=choices,
        help_text="Test peripheral"
    )


@settings(max_examples=100)
@given(template=peripheral_template())
def test_property_5_help_text_existence(template):
    r"""
    \brief           Test Property 5: Help text existence

    \details         For any configuration item (bool, int, hex, string types),
                     it should contain the help keyword and non-empty help text.

    Feature: kconfig-naming-standard, Property 5: Help text existence
    Validates: Requirements 5.2, 5.4, 5.5
    """
    generator = KconfigGenerator(template)

    # Generate complete file content
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.kconfig') as f:
        temp_path = f.name

    try:
        generator.generate_file(temp_path)

        with open(temp_path, 'r') as f:
            content = f.read()

        # Find all config declarations (bool, int, hex, string)
        config_pattern = r'config\s+(\S+)\s+(bool|int|hex|string)'
        configs = re.findall(config_pattern, content)

        # For each config, check that it has help text
        for config_name, config_type in configs:
            # Find the config block
            config_block_pattern = rf'config\s+{re.escape(config_name)}\s+{config_type}.*?(?=\n(?:config|choice|endif|endchoice|$))'
            config_block_match = re.search(config_block_pattern, content, re.DOTALL)

            if config_block_match:
                config_block = config_block_match.group(0)

                # Check for help keyword
                assert 'help' in config_block, \
                    f"Config {config_name} missing help keyword"

                # Check for non-empty help text after help keyword
                help_pattern = r'help\s*\n\s+(.+)'
                help_match = re.search(help_pattern, config_block)
                assert help_match and help_match.group(1).strip(), \
                    f"Config {config_name} has empty help text"

    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


@settings(max_examples=100)
@given(template=peripheral_template())
def test_property_6_file_structure_ordering(template):
    r"""
    \brief           Test Property 6: File structure ordering

    \details         For any peripheral Kconfig file, its content should be
                     organized in the order: file header comment → peripheral
                     enable → global config → instance 0 → instance 1 → ...

    Feature: kconfig-naming-standard, Property 6: File structure ordering
    Validates: Requirements 6.1, 6.4
    """
    generator = KconfigGenerator(template)

    # Generate complete file content
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.kconfig') as f:
        temp_path = f.name

    try:
        generator.generate_file(temp_path)

        with open(temp_path, 'r') as f:
            content = f.read()

        # Find positions of key elements
        header_pos = content.find('#')
        peripheral_enable_pos = content.find(f"config {template.platform.upper()}_{template.name.upper()}_ENABLE")

        # Check header comes first
        assert header_pos >= 0, "File header comment missing"
        assert peripheral_enable_pos > header_pos, \
            "Peripheral enable should come after header"

        # Check for global config (if numeric instances)
        if template.instance_type == "numeric":
            max_instances_pos = content.find(f"config {template.platform.upper()}_{template.name.upper()}_MAX_INSTANCES")
            if max_instances_pos >= 0:
                assert max_instances_pos > peripheral_enable_pos, \
                    "Global config should come after peripheral enable"

        # Check instance ordering
        if template.instance_type == "numeric":
            # Check numeric instances are in order
            for i in range(min(template.max_instances, 3)):  # Check first 3 instances
                instance_pattern = rf"# {template.name.upper()}{i} Configuration"
                instance_pos = content.find(instance_pattern)

                if instance_pos >= 0:
                    # Check this instance comes after peripheral enable
                    assert instance_pos > peripheral_enable_pos, \
                        f"Instance {i} should come after peripheral enable"

                    # Check instances are in order
                    if i > 0:
                        prev_instance_pattern = rf"# {template.name.upper()}{i-1} Configuration"
                        prev_instance_pos = content.find(prev_instance_pattern)
                        if prev_instance_pos >= 0:
                            assert instance_pos > prev_instance_pos, \
                                f"Instance {i} should come after instance {i-1}"
        else:
            # Check alpha instances are in order
            for i in range(min(template.max_instances, 3)):
                instance_letter = chr(ord('A') + i)
                instance_pattern = rf"# {template.name.upper()}{instance_letter} Configuration"
                instance_pos = content.find(instance_pattern)

                if instance_pos >= 0:
                    assert instance_pos > peripheral_enable_pos, \
                        f"Instance {instance_letter} should come after peripheral enable"

    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


@settings(max_examples=100)
@given(template=peripheral_template())
def test_property_7_generation_idempotency(template):
    r"""
    \brief           Test Property 7: Generation idempotency

    \details         For any peripheral template configuration, generating with
                     the same parameters multiple times should produce exactly
                     the same Kconfig file content.

    Feature: kconfig-naming-standard, Property 7: Generation idempotency
    Validates: Requirements 7.2, 7.3
    """
    # Generate first time
    generator1 = KconfigGenerator(template)
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.kconfig') as f:
        temp_path1 = f.name

    # Generate second time
    generator2 = KconfigGenerator(template)
    with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.kconfig') as f:
        temp_path2 = f.name

    try:
        generator1.generate_file(temp_path1)
        generator2.generate_file(temp_path2)

        # Read both files
        with open(temp_path1, 'r') as f1:
            content1 = f1.read()

        with open(temp_path2, 'r') as f2:
            content2 = f2.read()

        # Compare content
        assert content1 == content2, \
            "Generated files should be identical when using same template"

    finally:
        if os.path.exists(temp_path1):
            os.remove(temp_path1)
        if os.path.exists(temp_path2):
            os.remove(temp_path2)


if __name__ == "__main__":
    # Run tests with pytest
    pytest.main([__file__, "-v"])
