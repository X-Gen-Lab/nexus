"""
脚本交付验证系统

一个全面的跨平台脚本验证框架，专门为Nexus嵌入式系统项目设计。
该系统将自动化验证所有项目脚本在Windows、WSL和Linux环境中的功能性、兼容性和可靠性。
"""

__version__ = "1.0.0"
__author__ = "Nexus Team"

from .models import (
    Script,
    ScriptMetadata,
    ValidationResult,
    CompatibilityMatrix,
    ValidationReport,
    ScriptType,
    Platform,
    ValidationStatus,
    ScriptCategory,
)

from .controllers import ValidationController
from .managers import ScriptManager, PlatformManager
from .handlers import (
    ErrorHandler,
    ResourceManager,
    ScriptError,
    DependencyMissingError,
    PermissionDeniedError,
    ScriptSyntaxError,
    TimeoutError,
    PlatformError,
    PlatformNotSupportedError,
    CommandNotFoundError,
    PathFormatError,
    EnvironmentVariableError,
    ValidationSystemError,
    ConfigurationError,
    ResourceExhaustedError,
    NetworkError,
    ErrorResponse,
    ErrorSeverity,
)

from .reporters import HTMLReporter, JSONReporter, SummaryReporter, JUnitReporter

from .ci_integration import (
    CIIntegration,
    CIDetector,
    CIEnvironment,
    CIOutputFormatter,
    ExitCode,
    is_ci_environment,
    get_ci_platform,
    get_exit_code_from_summary,
)

from .integration import (
    ValidationWorkflow,
    ComponentRegistry,
    ValidationBuilder,
    create_workflow,
    run_validation,
    discover_scripts,
    check_platform_availability,
    get_system_info,
)

__all__ = [
    "Script",
    "ScriptMetadata",
    "ValidationResult",
    "CompatibilityMatrix",
    "ValidationReport",
    "ScriptType",
    "Platform",
    "ValidationStatus",
    "ScriptCategory",
    "ValidationController",
    "ScriptManager",
    "PlatformManager",
    "ErrorHandler",
    "ResourceManager",
    "ScriptError",
    "DependencyMissingError",
    "PermissionDeniedError",
    "ScriptSyntaxError",
    "TimeoutError",
    "PlatformError",
    "PlatformNotSupportedError",
    "CommandNotFoundError",
    "PathFormatError",
    "EnvironmentVariableError",
    "ValidationSystemError",
    "ConfigurationError",
    "ResourceExhaustedError",
    "NetworkError",
    "ErrorResponse",
    "ErrorSeverity",
    "HTMLReporter",
    "JSONReporter",
    "SummaryReporter",
    "JUnitReporter",
    "CIIntegration",
    "CIDetector",
    "CIEnvironment",
    "CIOutputFormatter",
    "ExitCode",
    "is_ci_environment",
    "get_ci_platform",
    "get_exit_code_from_summary",
    # Integration module
    "ValidationWorkflow",
    "ComponentRegistry",
    "ValidationBuilder",
    "create_workflow",
    "run_validation",
    "discover_scripts",
    "check_platform_availability",
    "get_system_info",
]
