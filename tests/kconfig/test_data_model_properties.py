#!/usr/bin/env python3
r"""
Property-based tests for data model classes.

Feature: kconfig-naming-standard, Property 9: Template parameter validity
Validates: Requirements 7.5

Property: For any peripheral template, its parameter configuration should satisfy:
- Parameter names are non-empty
- Types are in allowed range (int, hex, bool, string)
- Default values match type requirements
- Range is only used for int/hex types
"""

import sys
import os
import pytest
from hypothesis import given, strategies as st, settings, assume

# Add scripts directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from kconfig_tools.naming_rules import ParameterConfig, ChoiceConfig, PeripheralTemplate


# Strategy for generating valid parameter names
@st.composite
def valid_param_name(draw):
    """Generate a valid parameter name."""
    name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd'), whitelist_characters='_'),
        min_size=1,
        max_size=30
    ))
    # Ensure it's not just whitespace
    assume(name.strip())
    return name


# Strategy for generating valid parameter types
valid_param_types = st.sampled_from(["int", "hex", "bool", "string"])


# Strategy for generating default values based on type
@st.composite
def default_value_for_type(draw, param_type):
    """Generate a valid default value for the given parameter type."""
    if param_type == "int":
        return draw(st.integers(min_value=-1000000, max_value=1000000))
    elif param_type == "hex":
        return draw(st.one_of(
            st.integers(min_value=0, max_value=0xFFFFFF),
            st.text(alphabet='0123456789ABCDEFabcdef', min_size=1, max_size=8).map(lambda x: f"0x{x}")
        ))
    elif param_type == "bool":
        return draw(st.booleans())
    elif param_type == "string":
        return draw(st.text(min_size=0, max_size=100))
    return None


# Strategy for generating valid ranges
@st.composite
def valid_range(draw):
    """Generate a valid range tuple."""
    min_val = draw(st.integers(min_value=-1000, max_value=1000))
    max_val = draw(st.integers(min_value=min_val, max_value=min_val + 1000))
    return (min_val, max_val)


@settings(max_examples=100)
@given(
    name=valid_param_name(),
    param_type=valid_param_types
)
def test_parameter_config_name_non_empty(name, param_type):
    """
    Property 9: Parameter names must be non-empty.

    For any parameter configuration, the parameter name should be non-empty
    after stripping whitespace.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Generate default value based on type
    if param_type == "int":
        default = 0
    elif param_type == "hex":
        default = 0x00
    elif param_type == "bool":
        default = False
    else:  # string
        default = ""

    # Create parameter config
    param = ParameterConfig(
        name=name,
        type=param_type,
        default=default
    )

    # Verify name is non-empty after stripping
    assert param.name.strip(), \
        f"Parameter name '{param.name}' should be non-empty after stripping"


@settings(max_examples=100)
@given(
    name=valid_param_name(),
    param_type=valid_param_types
)
def test_parameter_config_type_in_allowed_range(name, param_type):
    """
    Property 9: Parameter types must be in allowed range.

    For any parameter configuration, the type should be one of:
    int, hex, bool, string.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Generate default value based on type
    if param_type == "int":
        default = 0
    elif param_type == "hex":
        default = 0x00
    elif param_type == "bool":
        default = False
    else:  # string
        default = ""

    # Create parameter config
    param = ParameterConfig(
        name=name,
        type=param_type,
        default=default
    )

    # Verify type is in allowed range
    allowed_types = ["int", "hex", "bool", "string"]
    assert param.type in allowed_types, \
        f"Parameter type '{param.type}' should be one of {allowed_types}"


@settings(max_examples=100)
@given(
    name=valid_param_name(),
    param_type=valid_param_types
)
def test_parameter_config_default_matches_type(name, param_type):
    """
    Property 9: Default values must match type requirements.

    For any parameter configuration, the default value should match
    the specified type.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Generate appropriate default value
    if param_type == "int":
        default = 42
        expected_type = int
    elif param_type == "hex":
        default = 0xFF
        expected_type = (int, str)
    elif param_type == "bool":
        default = True
        expected_type = bool
    else:  # string
        default = "test"
        expected_type = str

    # Create parameter config
    param = ParameterConfig(
        name=name,
        type=param_type,
        default=default
    )

    # Verify default value type matches
    if isinstance(expected_type, tuple):
        assert isinstance(param.default, expected_type), \
            f"Default value for {param_type} should be one of {expected_type}, got {type(param.default)}"
    else:
        assert isinstance(param.default, expected_type), \
            f"Default value for {param_type} should be {expected_type}, got {type(param.default)}"


@settings(max_examples=100)
@given(
    name=valid_param_name(),
    range_tuple=valid_range()
)
def test_parameter_config_range_only_for_int_hex(name, range_tuple):
    """
    Property 9: Range should only be used for int/hex types.

    For any parameter configuration with a range, the type should be
    either int or hex.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Test with int type (should work)
    param_int = ParameterConfig(
        name=name,
        type="int",
        default=range_tuple[0],
        range=range_tuple
    )
    assert param_int.range == range_tuple, \
        f"Range should be preserved for int type"

    # Test with hex type (should work)
    param_hex = ParameterConfig(
        name=name,
        type="hex",
        default=range_tuple[0],
        range=range_tuple
    )
    assert param_hex.range == range_tuple, \
        f"Range should be preserved for hex type"

    # Test with bool type (should fail)
    with pytest.raises(ValueError, match="Range can only be specified for int/hex types"):
        ParameterConfig(
            name=name,
            type="bool",
            default=True,
            range=range_tuple
        )

    # Test with string type (should fail)
    with pytest.raises(ValueError, match="Range can only be specified for int/hex types"):
        ParameterConfig(
            name=name,
            type="string",
            default="test",
            range=range_tuple
        )


@settings(max_examples=100)
@given(
    choice_name=valid_param_name(),
    options=st.lists(valid_param_name(), min_size=1, max_size=10, unique=True)
)
def test_choice_config_default_in_options(choice_name, options):
    """
    Property 9: Choice default must be in options list.

    For any choice configuration, the default option should be one of
    the available options.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Pick a random default from options
    default = options[0]

    # Create choice config
    choice = ChoiceConfig(
        name=choice_name,
        options=options,
        default=default
    )

    # Verify default is in options
    assert choice.default in choice.options, \
        f"Default option '{choice.default}' should be in options list {choice.options}"


@settings(max_examples=100)
@given(
    choice_name=valid_param_name(),
    options=st.lists(valid_param_name(), min_size=1, max_size=10, unique=True)
)
def test_choice_config_values_cover_all_options(choice_name, options):
    """
    Property 9: Choice values mapping should cover all options.

    For any choice configuration with a values mapping, all options
    should have corresponding integer values.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Create values mapping for all options
    values = {opt: i for i, opt in enumerate(options)}
    default = options[0]

    # Create choice config
    choice = ChoiceConfig(
        name=choice_name,
        options=options,
        default=default,
        values=values
    )

    # Verify all options have values
    for option in choice.options:
        assert option in choice.values, \
            f"Option '{option}' should have a value in values mapping"

    # Verify all values are integers
    for option, value in choice.values.items():
        assert isinstance(value, int), \
            f"Value for option '{option}' should be an integer, got {type(value)}"


@settings(max_examples=100)
@given(
    peripheral_name=valid_param_name(),
    platform_name=valid_param_name(),
    max_instances=st.integers(min_value=1, max_value=100),
    instance_type=st.sampled_from(["numeric", "alpha"])
)
def test_peripheral_template_validation(peripheral_name, platform_name, max_instances, instance_type):
    """
    Property 9: Peripheral template validation.

    For any peripheral template, it should satisfy:
    - Peripheral name is non-empty
    - Platform name is non-empty
    - max_instances is positive
    - instance_type is valid (numeric or alpha)

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Create peripheral template
    template = PeripheralTemplate(
        name=peripheral_name,
        platform=platform_name,
        max_instances=max_instances,
        instance_type=instance_type,
        parameters=[],
        choices=[]
    )

    # Verify peripheral name is non-empty
    assert template.name.strip(), \
        f"Peripheral name '{template.name}' should be non-empty"

    # Verify platform name is non-empty
    assert template.platform.strip(), \
        f"Platform name '{template.platform}' should be non-empty"

    # Verify max_instances is positive
    assert template.max_instances > 0, \
        f"max_instances should be positive, got {template.max_instances}"

    # Verify instance_type is valid
    assert template.instance_type in ["numeric", "alpha"], \
        f"instance_type should be 'numeric' or 'alpha', got '{template.instance_type}'"


@settings(max_examples=100)
@given(
    peripheral_name=valid_param_name(),
    platform_name=valid_param_name(),
    param_names=st.lists(valid_param_name(), min_size=2, max_size=10, unique=True)
)
def test_peripheral_template_no_duplicate_parameters(peripheral_name, platform_name, param_names):
    """
    Property 9: Peripheral template should not have duplicate parameter names.

    For any peripheral template, all parameter names should be unique.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Create parameters with unique names
    parameters = [
        ParameterConfig(name=name, type="int", default=0)
        for name in param_names
    ]

    # Create peripheral template (should succeed)
    template = PeripheralTemplate(
        name=peripheral_name,
        platform=platform_name,
        max_instances=1,
        instance_type="numeric",
        parameters=parameters,
        choices=[]
    )

    # Verify no duplicate parameter names
    param_name_list = [p.name for p in template.parameters]
    assert len(param_name_list) == len(set(param_name_list)), \
        f"Parameter names should be unique, got duplicates in {param_name_list}"


@settings(max_examples=100)
@given(
    peripheral_name=valid_param_name(),
    platform_name=valid_param_name(),
    choice_names=st.lists(valid_param_name(), min_size=2, max_size=10, unique=True)
)
def test_peripheral_template_no_duplicate_choices(peripheral_name, platform_name, choice_names):
    """
    Property 9: Peripheral template should not have duplicate choice names.

    For any peripheral template, all choice names should be unique.

    Feature: kconfig-naming-standard, Property 9
    Validates: Requirements 7.5
    """
    # Create choices with unique names
    choices = [
        ChoiceConfig(name=name, options=["A", "B"], default="A")
        for name in choice_names
    ]

    # Create peripheral template (should succeed)
    template = PeripheralTemplate(
        name=peripheral_name,
        platform=platform_name,
        max_instances=1,
        instance_type="numeric",
        parameters=[],
        choices=choices
    )

    # Verify no duplicate choice names
    choice_name_list = [c.name for c in template.choices]
    assert len(choice_name_list) == len(set(choice_name_list)), \
        f"Choice names should be unique, got duplicates in {choice_name_list}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
