r"""
\file            naming_rules.py
\brief           Kconfig naming rules and conventions
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Defines all naming rules and patterns for Kconfig configuration
                 symbols according to Nexus standards.
"""

import re
from dataclasses import dataclass
from typing import Union, Optional, Tuple, Any, List, Dict


class NamingRules:
    r"""
    \brief           Kconfig naming rules definition

    \details         Provides static methods for generating standardized
                     configuration symbol names at platform, peripheral,
                     and instance levels.
    """

    # Platform-level naming patterns
    PLATFORM_ENABLE_PATTERN = r"^[A-Z][A-Z0-9_]*_ENABLE$"
    PLATFORM_NAME_PATTERN = r"^[A-Z][A-Z0-9_]*_PLATFORM_NAME$"
    PLATFORM_VERSION_PATTERN = r"^[A-Z][A-Z0-9_]*_PLATFORM_VERSION$"
    PLATFORM_FEATURE_PATTERN = r"^[A-Z][A-Z0-9_]*_[A-Z][A-Z0-9_]*_ENABLE$"

    # Peripheral-level naming patterns
    PERIPHERAL_ENABLE_PATTERN = r"^[A-Z][A-Z0-9_]*_[A-Z][A-Z0-9_]*_ENABLE$"
    PERIPHERAL_MAX_INSTANCES_PATTERN = r"^[A-Z][A-Z0-9_]*_[A-Z][A-Z0-9_]*_MAX_INSTANCES$"
    PERIPHERAL_PARAM_PATTERN = r"^[A-Z][A-Z0-9_]*_[A-Z][A-Z0-9_]*_[A-Z][A-Z0-9_]*$"

    # Instance-level naming patterns
    INSTANCE_ENABLE_PATTERN = r"^INSTANCE_NX_[A-Z][A-Z0-9_]*[0-9A-Z]+$"
    INSTANCE_PARAM_PATTERN = r"^[A-Z][A-Z0-9_]*[0-9A-Z]+_[A-Z][A-Z0-9_]*$"

    # Choice naming patterns
    CHOICE_OPTION_PATTERN = r"^NX_[A-Z][A-Z0-9_]*[0-9A-Z]+_[A-Z][A-Z0-9_]*_[A-Z][A-Z0-9_]*$"
    CHOICE_VALUE_PATTERN = r"^[A-Z][A-Z0-9_]*[0-9A-Z]+_[A-Z][A-Z0-9_]*_VALUE$"

    @staticmethod
    def platform_enable(platform: str) -> str:
        r"""
        \brief           Generate platform enable configuration symbol
        \param[in]       platform: Platform name (e.g., "NATIVE", "STM32")
        \return          Configuration symbol (e.g., "NATIVE_ENABLE")
        """
        return f"{NamingRules.normalize_name(platform)}_ENABLE"

    @staticmethod
    def platform_feature_enable(platform: str, feature: str) -> str:
        r"""
        \brief           Generate platform feature enable configuration symbol
        \param[in]       platform: Platform name
        \param[in]       feature: Feature name
        \return          Configuration symbol (e.g., "NATIVE_UART_ENABLE")
        """
        return f"{NamingRules.normalize_name(platform)}_{NamingRules.normalize_name(feature)}_ENABLE"

    @staticmethod
    def platform_name(platform: str) -> str:
        r"""
        \brief           Generate platform name configuration symbol
        \param[in]       platform: Platform name
        \return          Configuration symbol (e.g., "NATIVE_PLATFORM_NAME")
        """
        return f"{NamingRules.normalize_name(platform)}_PLATFORM_NAME"

    @staticmethod
    def platform_version(platform: str) -> str:
        r"""
        \brief           Generate platform version configuration symbol
        \param[in]       platform: Platform name
        \return          Configuration symbol (e.g., "NATIVE_PLATFORM_VERSION")
        """
        return f"{NamingRules.normalize_name(platform)}_PLATFORM_VERSION"

    @staticmethod
    def peripheral_enable(platform: str, peripheral: str) -> str:
        r"""
        \brief           Generate peripheral enable configuration symbol
        \param[in]       platform: Platform name
        \param[in]       peripheral: Peripheral name (e.g., "UART", "GPIO")
        \return          Configuration symbol (e.g., "NATIVE_UART_ENABLE")
        """
        return f"{NamingRules.normalize_name(platform)}_{NamingRules.normalize_name(peripheral)}_ENABLE"

    @staticmethod
    def peripheral_max_instances(platform: str, peripheral: str) -> str:
        r"""
        \brief           Generate peripheral max instances configuration symbol
        \param[in]       platform: Platform name
        \param[in]       peripheral: Peripheral name
        \return          Configuration symbol (e.g., "NATIVE_UART_MAX_INSTANCES")
        """
        return f"{NamingRules.normalize_name(platform)}_{NamingRules.normalize_name(peripheral)}_MAX_INSTANCES"

    @staticmethod
    def peripheral_param(platform: str, peripheral: str, param: str) -> str:
        r"""
        \brief           Generate peripheral parameter configuration symbol
        \param[in]       platform: Platform name
        \param[in]       peripheral: Peripheral name
        \param[in]       param: Parameter name
        \return          Configuration symbol (e.g., "NATIVE_UART_DEFAULT_BAUDRATE")
        """
        return f"{NamingRules.normalize_name(platform)}_{NamingRules.normalize_name(peripheral)}_{NamingRules.normalize_name(param)}"

    @staticmethod
    def instance_enable(peripheral: str, instance: Union[int, str]) -> str:
        r"""
        \brief           Generate instance enable configuration symbol
        \param[in]       peripheral: Peripheral name
        \param[in]       instance: Instance identifier (number or letter)
        \return          Configuration symbol (e.g., "INSTANCE_NX_UART_0", "INSTANCE_NX_GPIOA")
        """
        peripheral_norm = NamingRules.normalize_name(peripheral)

        # Handle GPIO special case with letter instances
        if peripheral_norm == "GPIO" and isinstance(instance, str) and instance.isalpha():
            # For GPIO ports like A, B, C
            return f"INSTANCE_NX_GPIO{instance.upper()}"
        elif peripheral_norm == "GPIO" and isinstance(instance, str) and "PIN" in instance.upper():
            # For GPIO pins like PIN0, PIN1
            return f"INSTANCE_NX_{instance.upper()}"
        else:
            # Standard numeric instance
            return f"INSTANCE_NX_{peripheral_norm}_{instance}"

    @staticmethod
    def instance_param(peripheral: str, instance: Union[int, str], param: str) -> str:
        r"""
        \brief           Generate instance parameter configuration symbol
        \param[in]       peripheral: Peripheral name
        \param[in]       instance: Instance identifier
        \param[in]       param: Parameter name
        \return          Configuration symbol (e.g., "UART0_BAUDRATE", "GPIOA_PIN0_MODE")
        """
        peripheral_norm = NamingRules.normalize_name(peripheral)
        param_norm = NamingRules.normalize_name(param)

        # Handle GPIO special case with letter instances
        if peripheral_norm == "GPIO" and isinstance(instance, str) and instance.isalpha():
            # For GPIO ports like A, B, C
            return f"GPIO{instance.upper()}_{param_norm}"
        else:
            # Standard instance parameter
            return f"{peripheral_norm}{instance}_{param_norm}"

    @staticmethod
    def choice_option(peripheral: str, instance: Union[int, str],
                     category: str, option: str) -> str:
        r"""
        \brief           Generate choice option configuration symbol
        \param[in]       peripheral: Peripheral name
        \param[in]       instance: Instance identifier
        \param[in]       category: Choice category name
        \param[in]       option: Option name
        \return          Configuration symbol (e.g., "NX_UART0_PARITY_NONE")
        """
        peripheral_norm = NamingRules.normalize_name(peripheral)
        category_norm = NamingRules.normalize_name(category)
        option_norm = NamingRules.normalize_name(option)

        # Handle GPIO special case with letter instances
        if peripheral_norm == "GPIO" and isinstance(instance, str) and instance.isalpha():
            # For GPIO ports like A, B, C
            return f"NX_GPIO{instance.upper()}_{category_norm}_{option_norm}"
        else:
            # Standard choice option
            return f"NX_{peripheral_norm}{instance}_{category_norm}_{option_norm}"

    @staticmethod
    def choice_value(peripheral: str, instance: Union[int, str],
                    category: str) -> str:
        r"""
        \brief           Generate choice value configuration symbol
        \param[in]       peripheral: Peripheral name
        \param[in]       instance: Instance identifier
        \param[in]       category: Choice category name
        \return          Configuration symbol (e.g., "UART0_PARITY_VALUE")
        """
        peripheral_norm = NamingRules.normalize_name(peripheral)
        category_norm = NamingRules.normalize_name(category)

        # Handle GPIO special case with letter instances
        if peripheral_norm == "GPIO" and isinstance(instance, str) and instance.isalpha():
            # For GPIO ports like A, B, C
            return f"GPIO{instance.upper()}_{category_norm}_VALUE"
        else:
            # Standard choice value
            return f"{peripheral_norm}{instance}_{category_norm}_VALUE"

    @staticmethod
    def validate_pattern(name: str, pattern: str) -> bool:
        r"""
        \brief           Validate configuration symbol against pattern
        \param[in]       name: Configuration symbol name
        \param[in]       pattern: Regular expression pattern
        \return          True if name matches pattern, False otherwise
        """
        return bool(re.match(pattern, name))

    @staticmethod
    def normalize_name(name: str) -> str:
        r"""
        \brief           Normalize name to uppercase with underscores
        \param[in]       name: Input name
        \return          Normalized name
        """
        # Convert to uppercase
        normalized = name.upper()
        # Replace spaces and hyphens with underscores
        normalized = normalized.replace(' ', '_').replace('-', '_')
        # Remove any characters that are not alphanumeric or underscore
        normalized = re.sub(r'[^A-Z0-9_]', '', normalized)
        # Remove consecutive underscores
        normalized = re.sub(r'_+', '_', normalized)
        # Remove leading/trailing underscores
        normalized = normalized.strip('_')
        # If result is empty or starts with a digit, prepend 'CONFIG_'
        if not normalized or (normalized and normalized[0].isdigit()):
            normalized = 'CONFIG_' + normalized
        return normalized



@dataclass
class ParameterConfig:
    r"""
    \brief           Parameter configuration data class

    \details         Defines configuration for a single parameter in a peripheral
                     template, including type, default value, range constraints,
                     and help text.
    """

    name: str                                   # Parameter name
    type: str                                   # Parameter type: int, hex, bool, string
    default: Any                                # Default value
    range: Optional[Tuple[int, int]] = None     # Range for int/hex types
    help: str = ""                              # Help text

    def __post_init__(self):
        r"""
        \brief           Validate parameter configuration after initialization

        \details         Ensures parameter name is non-empty, type is valid,
                         default value matches type, and range is only used
                         for int/hex types.
        """
        # Validate parameter name is non-empty
        if not self.name or not self.name.strip():
            raise ValueError("Parameter name cannot be empty")

        # Validate type is in allowed range
        allowed_types = ["int", "hex", "bool", "string"]
        if self.type not in allowed_types:
            raise ValueError(f"Parameter type must be one of {allowed_types}, got '{self.type}'")

        # Validate default value matches type
        if self.type == "int":
            if not isinstance(self.default, int):
                raise ValueError(f"Default value for int type must be an integer, got {type(self.default).__name__}")
        elif self.type == "hex":
            if not isinstance(self.default, (int, str)):
                raise ValueError(f"Default value for hex type must be an integer or hex string, got {type(self.default).__name__}")
        elif self.type == "bool":
            if not isinstance(self.default, bool):
                raise ValueError(f"Default value for bool type must be a boolean, got {type(self.default).__name__}")
        elif self.type == "string":
            if not isinstance(self.default, str):
                raise ValueError(f"Default value for string type must be a string, got {type(self.default).__name__}")

        # Validate range is only used for int/hex types
        if self.range is not None:
            if self.type not in ["int", "hex"]:
                raise ValueError(f"Range can only be specified for int/hex types, not for '{self.type}'")
            # Validate range is a tuple of two integers
            if not isinstance(self.range, tuple) or len(self.range) != 2:
                raise ValueError("Range must be a tuple of two integers")
            if not all(isinstance(x, int) for x in self.range):
                raise ValueError("Range values must be integers")
            if self.range[0] > self.range[1]:
                raise ValueError(f"Range minimum ({self.range[0]}) cannot be greater than maximum ({self.range[1]})")



@dataclass
class ChoiceConfig:
    r"""
    \brief           Choice configuration data class

    \details         Defines configuration for a choice item in a peripheral
                     template, including available options, default option,
                     and optional integer value mappings.
    """

    name: str                                   # Choice name
    options: List[str]                          # List of available options
    default: str                                # Default option
    help: str = ""                              # Help text
    values: Optional[Dict[str, int]] = None     # Option to integer value mapping

    def __post_init__(self):
        r"""
        \brief           Validate choice configuration after initialization

        \details         Ensures choice name is non-empty, options list is
                         non-empty, default option is in options list, and
                         values mapping covers all options if provided.
        """
        # Validate choice name is non-empty
        if not self.name or not self.name.strip():
            raise ValueError("Choice name cannot be empty")

        # Validate options list is non-empty
        if not self.options or len(self.options) == 0:
            raise ValueError("Choice must have at least one option")

        # Validate all options are non-empty strings
        for option in self.options:
            if not option or not option.strip():
                raise ValueError("Choice options cannot be empty strings")

        # Validate default option is in options list
        if self.default not in self.options:
            raise ValueError(f"Default option '{self.default}' must be one of the available options: {self.options}")

        # Validate values mapping if provided
        if self.values is not None:
            # Check that all options have corresponding values
            for option in self.options:
                if option not in self.values:
                    raise ValueError(f"Option '{option}' is missing from values mapping")

            # Check that all values are integers
            for option, value in self.values.items():
                if not isinstance(value, int):
                    raise ValueError(f"Value for option '{option}' must be an integer, got {type(value).__name__}")

            # Check that there are no extra values
            for option in self.values.keys():
                if option not in self.options:
                    raise ValueError(f"Value mapping contains unknown option '{option}'")



@dataclass
class PeripheralTemplate:
    r"""
    \brief           Peripheral template configuration data class

    \details         Defines complete configuration template for a peripheral,
                     including platform, instance configuration, parameters,
                     and choice items.
    """

    name: str                                   # Peripheral name (e.g., UART, GPIO)
    platform: str                               # Platform name (e.g., NATIVE, STM32)
    max_instances: int                          # Maximum number of instances
    instance_type: str                          # Instance type: "numeric" or "alpha"
    parameters: List[ParameterConfig]           # List of parameter configurations
    choices: List[ChoiceConfig]                 # List of choice configurations
    help_text: str = ""                         # Peripheral help text

    def __post_init__(self):
        r"""
        \brief           Validate peripheral template after initialization

        \details         Ensures peripheral name is non-empty, platform is
                         non-empty, max_instances is positive, instance_type
                         is valid, and all parameters and choices are valid.
        """
        # Validate peripheral name is non-empty
        if not self.name or not self.name.strip():
            raise ValueError("Peripheral name cannot be empty")

        # Validate platform name is non-empty
        if not self.platform or not self.platform.strip():
            raise ValueError("Platform name cannot be empty")

        # Validate max_instances is positive
        if not isinstance(self.max_instances, int) or self.max_instances <= 0:
            raise ValueError(f"max_instances must be a positive integer, got {self.max_instances}")

        # Validate instance_type is valid
        allowed_instance_types = ["numeric", "alpha"]
        if self.instance_type not in allowed_instance_types:
            raise ValueError(f"instance_type must be one of {allowed_instance_types}, got '{self.instance_type}'")

        # Validate parameters list contains only ParameterConfig instances
        if not isinstance(self.parameters, list):
            raise ValueError("parameters must be a list")
        for param in self.parameters:
            if not isinstance(param, ParameterConfig):
                raise ValueError(f"All parameters must be ParameterConfig instances, got {type(param).__name__}")

        # Validate choices list contains only ChoiceConfig instances
        if not isinstance(self.choices, list):
            raise ValueError("choices must be a list")
        for choice in self.choices:
            if not isinstance(choice, ChoiceConfig):
                raise ValueError(f"All choices must be ChoiceConfig instances, got {type(choice).__name__}")

        # Validate no duplicate parameter names
        param_names = [p.name for p in self.parameters]
        if len(param_names) != len(set(param_names)):
            duplicates = [name for name in param_names if param_names.count(name) > 1]
            raise ValueError(f"Duplicate parameter names found: {set(duplicates)}")

        # Validate no duplicate choice names
        choice_names = [c.name for c in self.choices]
        if len(choice_names) != len(set(choice_names)):
            duplicates = [name for name in choice_names if choice_names.count(name) > 1]
            raise ValueError(f"Duplicate choice names found: {set(duplicates)}")
