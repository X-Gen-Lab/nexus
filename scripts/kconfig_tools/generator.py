r"""
\file            generator.py
\brief           Kconfig file generator
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         Provides tools for generating Kconfig files from templates
                 according to Nexus naming standards.
"""

from dataclasses import dataclass, field
from typing import List, Dict, Optional, Any, Tuple, Union
from .naming_rules import NamingRules


@dataclass
class ParameterConfig:
    r"""
    \brief           Parameter configuration definition
    """
    name: str  # Parameter name
    type: str  # Parameter type: int, hex, bool, string
    default: Any  # Default value
    range: Optional[Tuple[int, int]] = None  # Range for int/hex types
    help: str = ""  # Help text


@dataclass
class ChoiceConfig:
    r"""
    \brief           Choice configuration definition
    """
    name: str  # Choice category name
    options: List[str]  # List of option names
    default: str  # Default option
    help: str = ""  # Help text
    values: Optional[Dict[str, int]] = None  # Option to integer value mapping


@dataclass
class PeripheralTemplate:
    r"""
    \brief           Peripheral template configuration
    """
    name: str  # Peripheral name (e.g., UART, GPIO)
    platform: str  # Platform name (e.g., NATIVE)
    max_instances: int  # Maximum number of instances
    instance_type: str  # Instance type: "numeric" or "alpha"
    parameters: List[ParameterConfig] = field(default_factory=list)  # Parameter list
    choices: List[ChoiceConfig] = field(default_factory=list)  # Choice list
    help_text: str = ""  # Peripheral help text


class KconfigGenerator:
    r"""
    \brief           Kconfig file generator

    \details         Generates Kconfig files from peripheral templates
                     following Nexus naming standards.
    """

    def __init__(self, template: PeripheralTemplate):
        r"""
        \brief           Initialize generator with template
        \param[in]       template: Peripheral template configuration
        """
        self.template = template
        self.rules = NamingRules()

    def generate_header(self) -> str:
        r"""
        \brief           Generate file header comment
        \return          Header comment string
        """
        peripheral_name = self.template.name.upper()
        platform_name = self.template.platform.capitalize()

        header = f"# {peripheral_name} Configuration for {platform_name} Platform\n\n"
        return header

    def generate_peripheral_enable(self) -> str:
        r"""
        \brief           Generate peripheral enable configuration
        \return          Peripheral enable configuration string
        """
        enable_symbol = self.rules.peripheral_enable(
            self.template.platform,
            self.template.name
        )
        platform_symbol = f"PLATFORM_{self.template.platform.upper()}"
        peripheral_name = self.template.name.upper()
        platform_name = self.template.platform.capitalize()

        config = f"config {enable_symbol}\n"
        config += f"    bool \"Enable {peripheral_name} support for {platform_name} platform\"\n"
        config += f"    default y\n"
        config += f"    depends on {platform_symbol}\n"
        config += f"    help\n"

        if self.template.help_text:
            config += f"      {self.template.help_text}\n"
        else:
            config += f"      Enable {peripheral_name} peripheral support for the {platform_name} platform.\n"

        config += f"\n"
        return config

    def generate_global_config(self) -> str:
        r"""
        \brief           Generate global configuration (e.g., max instances)
        \return          Global configuration string
        """
        enable_symbol = self.rules.peripheral_enable(
            self.template.platform,
            self.template.name
        )
        max_instances_symbol = self.rules.peripheral_max_instances(
            self.template.platform,
            self.template.name
        )
        peripheral_name = self.template.name.upper()
        platform_name = self.template.platform.capitalize()

        config = f"if {enable_symbol}\n\n"

        # Only generate max_instances config for numeric instance types
        if self.template.instance_type == "numeric":
            config += f"config {max_instances_symbol}\n"
            config += f"    int \"Maximum number of {peripheral_name} instances\"\n"
            config += f"    default {self.template.max_instances}\n"
            config += f"    range 1 {self.template.max_instances * 2}\n"
            config += f"    help\n"
            config += f"      Maximum number of {peripheral_name} instances supported by the\n"
            config += f"      {platform_name} platform. Each instance requires buffer memory.\n\n"

        return config

    def generate_instance_config(self, instance: Union[int, str]) -> str:
        r"""
        \brief           Generate complete instance configuration
        \param[in]       instance: Instance identifier
        \return          Instance configuration string
        """
        config = ""

        # Generate section comment
        if self.template.instance_type == "numeric":
            config += f"# {self.template.name.upper()}{instance} Configuration\n"
        else:
            config += f"# {self.template.name.upper()}{instance.upper()} Configuration\n"

        # Generate menuconfig entry
        config += self.generate_instance_menuconfig(instance)

        # Generate if block
        instance_enable = self.rules.instance_enable(self.template.name, instance)
        config += f"if {instance_enable}\n\n"

        # Generate parameters
        config += self.generate_instance_parameters(instance)

        # Generate choices
        config += self.generate_instance_choices(instance)

        # Close if block
        config += f"endif # {instance_enable}\n\n"

        return config

    def generate_instance_menuconfig(self, instance: Union[int, str]) -> str:
        r"""
        \brief           Generate instance menuconfig entry
        \param[in]       instance: Instance identifier
        \return          Menuconfig entry string
        """
        instance_enable = self.rules.instance_enable(self.template.name, instance)
        enable_symbol = self.rules.peripheral_enable(
            self.template.platform,
            self.template.name
        )
        peripheral_name = self.template.name.upper()

        # Determine instance label
        if self.template.instance_type == "numeric":
            instance_label = f"{peripheral_name}{instance}"
        else:
            instance_label = f"{peripheral_name}{instance.upper()}"

        # First instance defaults to 'y', others to 'n'
        default_value = "y" if (instance == 0 or instance == "A") else "n"

        config = f"menuconfig {instance_enable}\n"
        config += f"    bool \"Enable {instance_label}\"\n"
        config += f"    default {default_value}\n"
        config += f"    depends on {enable_symbol}\n"
        config += f"    help\n"

        if self.template.instance_type == "numeric":
            config += f"      Enable {peripheral_name} instance {instance}.\n\n"
        else:
            config += f"      Enable {peripheral_name} port {instance.upper()}.\n\n"

        return config

    def generate_instance_parameters(self, instance: Union[int, str]) -> str:
        r"""
        \brief           Generate instance parameter configurations
        \param[in]       instance: Instance identifier
        \return          Parameter configurations string
        """
        config = ""

        for param in self.template.parameters:
            param_symbol = self.rules.instance_param(
                self.template.name,
                instance,
                param.name
            )

            # Determine instance label
            if self.template.instance_type == "numeric":
                instance_label = f"{self.template.name.upper()}{instance}"
            else:
                instance_label = f"{self.template.name.upper()}{instance.upper()}"

            # Generate config entry
            config += f"config {param_symbol}\n"

            # Type and prompt
            if param.type == "bool":
                config += f"    bool \"{instance_label} {param.name.lower().replace('_', ' ')}\"\n"
            elif param.type == "int":
                config += f"    int \"{instance_label} {param.name.lower().replace('_', ' ')}\"\n"
            elif param.type == "hex":
                config += f"    hex \"{instance_label} {param.name.lower().replace('_', ' ')}\"\n"
            elif param.type == "string":
                config += f"    string \"{instance_label} {param.name.lower().replace('_', ' ')}\"\n"

            # Default value
            if param.type == "hex" and isinstance(param.default, int):
                config += f"    default 0x{param.default:X}\n"
            elif param.type == "string":
                config += f"    default \"{param.default}\"\n"
            elif param.type == "bool":
                config += f"    default {'y' if param.default else 'n'}\n"
            else:
                config += f"    default {param.default}\n"

            # Range if applicable
            if param.range is not None:
                config += f"    range {param.range[0]} {param.range[1]}\n"

            # Help text
            config += f"    help\n"
            if param.help:
                config += f"      {param.help}\n"
            else:
                config += f"      {param.name.replace('_', ' ').capitalize()} for {instance_label}.\n"

            config += f"\n"

        return config

    def generate_instance_choices(self, instance: Union[int, str]) -> str:
        r"""
        \brief           Generate instance choice configurations
        \param[in]       instance: Instance identifier
        \return          Choice configurations string
        """
        config = ""
        instance_enable = self.rules.instance_enable(self.template.name, instance)

        # Determine instance label
        if self.template.instance_type == "numeric":
            instance_label = f"{self.template.name.upper()}{instance}"
        else:
            instance_label = f"{self.template.name.upper()}{instance.upper()}"

        for choice in self.template.choices:
            # Generate choice block
            config += f"choice\n"
            config += f"    prompt \"{instance_label} {choice.name.lower().replace('_', ' ')}\"\n"

            # Default option
            default_option = self.rules.choice_option(
                self.template.name,
                instance,
                choice.name,
                choice.default
            )
            config += f"    default {default_option}\n"
            config += f"    depends on {instance_enable}\n\n"

            # Generate options
            for option in choice.options:
                option_symbol = self.rules.choice_option(
                    self.template.name,
                    instance,
                    choice.name,
                    option
                )

                # Format option label
                option_label = option.replace('_', ' ').capitalize()
                if option_label == "None":
                    option_label = "None"

                config += f"config {option_symbol}\n"
                config += f"    bool \"{option_label}\"\n\n"

            config += f"endchoice\n\n"

            # Generate value config
            value_symbol = self.rules.choice_value(
                self.template.name,
                instance,
                choice.name
            )

            config += f"config {value_symbol}\n"
            config += f"    int\n"

            # Generate default values
            for option in choice.options:
                option_symbol = self.rules.choice_option(
                    self.template.name,
                    instance,
                    choice.name,
                    option
                )

                # Get value from mapping or use index
                if choice.values and option in choice.values:
                    value = choice.values[option]
                else:
                    value = choice.options.index(option)

                config += f"    default {value} if {option_symbol}\n"

            config += f"\n"

        return config

    def generate_file(self, output_path: str) -> None:
        r"""
        \brief           Generate complete Kconfig file
        \param[in]       output_path: Output file path
        """
        content = ""

        # Generate header
        content += self.generate_header()

        # Generate peripheral enable
        content += self.generate_peripheral_enable()

        # Generate global config
        content += self.generate_global_config()

        # Generate instance configurations
        if self.template.instance_type == "numeric":
            # Numeric instances: 0, 1, 2, ...
            for i in range(self.template.max_instances):
                content += self.generate_instance_config(i)
        else:
            # Alpha instances: A, B, C, ...
            for i in range(self.template.max_instances):
                instance_letter = chr(ord('A') + i)
                content += self.generate_instance_config(instance_letter)

        # Close peripheral enable if block
        enable_symbol = self.rules.peripheral_enable(
            self.template.platform,
            self.template.name
        )
        content += f"endif # {enable_symbol}\n"

        # Write to file
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)
