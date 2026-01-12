"""
错误处理和资源管理模块

包含错误处理器和资源管理器。
"""

from .error_handler import (
    ErrorHandler,
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
    ErrorSeverity
)
from .resource_manager import ResourceManager

__all__ = [
    'ErrorHandler',
    'ResourceManager',
    'ScriptError',
    'DependencyMissingError',
    'PermissionDeniedError',
    'ScriptSyntaxError',
    'TimeoutError',
    'PlatformError',
    'PlatformNotSupportedError',
    'CommandNotFoundError',
    'PathFormatError',
    'EnvironmentVariableError',
    'ValidationSystemError',
    'ConfigurationError',
    'ResourceExhaustedError',
    'NetworkError',
    'ErrorResponse',
    'ErrorSeverity'
]
