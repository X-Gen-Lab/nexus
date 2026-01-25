#!/usr/bin/env python3
r"""
Property-based tests for instance configuration generation.

Feature: kconfig-naming-standard
Property 2: Instance numbering continuity
Property 3: Choice completeness
Property 4: Dependency correctness
Validates: Requirements 3.5, 4.1, 4.3, 4.4, 4.5

Properties:
- Property 2: For any numeric peripheral instances, instance numbers should start
  from 0 and increment continuously without gaps.
- Property 3: For any choice configuration, each choice should contain: choice
  declaration, at least one option, default declaration, endchoice marker, and
  corresponding VALUE config.
- Property 4: For any instance configuration, all sub-configuration items' depends
  on should correctly reference the parent enable configuration symbol.
"""

import sys
import os
import re
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
    max_inst = draw(st.integers(min_value=1, max_value=8))
    inst_type = draw(st.sampled_from(["numeric", "alpha"]))

    num_params = draw(st.integers(min_value=0, max_value=3))
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
def test_property_2_instance_numbering_continuity(template):
    r"""
    \brief           Test Property 2: Instance numbering continuity

    \details         For any numeric peripheral instances, instance numbers
                     should start from 0 and increment continuously without gaps.

    Feature: kconfig-naming-standard, Property 2: Instance numbering continuity
    Validates: Requirements 3.5
    """
    # Only test numeric instances
    if template.instance_type != "numeric":
        return

    generator = KconfigGenerator(template)

    # Generate content for all instances
    content = ""
    for i in range(template.max_instances):
        content += generator.generate_instance_config(i)

    # Extract all instance numbers from INSTANCE_NX_<PERIPHERAL>_<N> patterns
    pattern = rf"INSTANCE_NX_{template.name.upper()}_(\d+)"
    matches = re.findall(pattern, content)

    if matches:
        instance_numbers = sorted([int(m) for m in matches])

        # Check that instances start from 0
        assert instance_numbers[0] == 0, f"Instances should start from 0, got {instance_numbers[0]}"

        # Check that instances are continuous
        for i, num in enumerate(instance_numbers):
            assert num == i, f"Instance numbers should be continuous, expected {i}, got {num}"


@settings(max_examples=100)
@given(template=peripheral_template())
def test_property_3_choice_completeness(template):
    r"""
    \brief           Test Property 3: Choice completeness

    \details         For any choice configuration, each choice should contain:
                     choice declaration, at least one option, default declaration,
                     endchoice marker, and corresponding VALUE config.

    Feature: kconfig-naming-standard, Property 3: Choice completeness
    Validates: Requirements 4.1, 4.3, 4.5
    """
    # Skip if no choices
    if not template.choices:
        return

    generator = KconfigGenerator(template)

    # Generate content for first instance
    if template.instance_type == "numeric":
        instance = 0
    else:
        instance = "A"

    content = generator.generate_instance_choices(instance)

    # For each choice in template
    for choice in template.choices:
        # Check for choice declaration
        assert "choice" in content, "Choice declaration missing"

        # Check for endchoice marker
        assert "endchoice" in content, "endchoice marker missing"

        # Check for default declaration
        assert "default" in content, "default declaration missing"

        # Check for at least one option
        assert any(f"config NX_" in content for opt in choice.options), \
            "At least one option should be present"

        # Check for VALUE config
        if template.instance_type == "numeric":
            value_pattern = rf"{template.name.upper()}{instance}_{choice.name.upper()}_VALUE"
        else:
            value_pattern = rf"{template.name.upper()}{instance.upper()}_{choice.name.upper()}_VALUE"

        assert value_pattern in content, f"VALUE config {value_pattern} missing"


@settings(max_examples=100)
@given(template=peripheral_template())
def test_property_4_dependency_correctness(template):
    r"""
    \brief           Test Property 4: Dependency correctness

    \details         For any instance configuration, all sub-configuration items'
                     depends on should correctly reference the parent enable
                     configuration symbol.

    Feature: kconfig-naming-standard, Property 4: Dependency correctness
    Validates: Requirements 4.4
    """
    generator = KconfigGenerator(template)

    # Generate content for first instance
    if template.instance_type == "numeric":
        instance = 0
        instance_enable = f"INSTANCE_NX_{template.name.upper()}_{instance}"
    else:
        instance = "A"
        instance_enable = f"INSTANCE_NX_{template.name.upper()}{instance}"

    content = generator.generate_instance_config(instance)

    # Find all "depends on" statements
    depends_pattern = r"depends on (\S+)"
    depends_matches = re.findall(depends_pattern, content)

    # All depends on statements should reference either:
    # 1. The instance enable symbol
    # 2. The peripheral enable symbol
    peripheral_enable = f"{template.platform.upper()}_{template.name.upper()}_ENABLE"

    for dep in depends_matches:
        assert dep == instance_enable or dep == peripheral_enable, \
            f"Dependency {dep} should reference {instance_enable} or {peripheral_enable}"


if __name__ == "__main__":
    # Run tests with pytest
    pytest.main([__file__, "-v"])
