r"""
\file            __init__.py
\brief           Kconfig naming standard system package
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         This package provides tools for generating and validating
                 Kconfig files according to Nexus naming standards.
"""

__version__ = "1.0.0"
__author__ = "Nexus Team"

from .naming_rules import NamingRules
from .generator import KconfigGenerator, PeripheralTemplate, ParameterConfig, ChoiceConfig
from .validator import KconfigValidator, ValidationIssue

__all__ = [
    "NamingRules",
    "KconfigGenerator",
    "PeripheralTemplate",
    "ParameterConfig",
    "ChoiceConfig",
    "KconfigValidator",
    "ValidationIssue",
]
