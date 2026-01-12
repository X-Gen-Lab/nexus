"""
错误处理器

处理脚本执行错误和异常，提供清晰的错误消息和修复建议。
实现优雅的错误恢复机制。

需求：3.1-3.5
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, List, Dict, Any
from pathlib import Path
import traceback
import re


class ErrorSeverity(Enum):
    """错误严重程度"""
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


# ============================================================================
# 自定义异常类
# ============================================================================

class ScriptError(Exception):
    """脚本执行错误基类"""

    def __init__(self, message: str, script_path: Optional[Path] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.message = message
        self.script_path = script_path
        self.details = details or {}


class DependencyMissingError(ScriptError):
    """缺失依赖错误 - 需求 3.1"""

    def __init__(self, message: str, dependency: str,
                 script_path: Optional[Path] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, script_path, details)
        self.dependency = dependency


class PermissionDeniedError(ScriptError):
    """权限不足错误 - 需求 3.3"""

    def __init__(self, message: str, resource_path: Optional[Path] = None,
                 script_path: Optional[Path] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, script_path, details)
        self.resource_path = resource_path


class ScriptSyntaxError(ScriptError):
    """脚本语法错误"""

    def __init__(self, message: str, line_number: Optional[int] = None,
                 script_path: Optional[Path] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, script_path, details)
        self.line_number = line_number


class TimeoutError(ScriptError):
    """脚本执行超时错误 - 需求 3.4"""

    def __init__(self, message: str, timeout_seconds: int,
                 script_path: Optional[Path] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, script_path, details)
        self.timeout_seconds = timeout_seconds


class PlatformError(Exception):
    """平台兼容性错误基类"""

    def __init__(self, message: str, platform: Optional[str] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.message = message
        self.platform = platform
        self.details = details or {}


class PlatformNotSupportedError(PlatformError):
    """平台不支持错误 - 需求 3.5"""
    pass


class CommandNotFoundError(PlatformError):
    """命令不可用错误"""

    def __init__(self, message: str, command: str,
                 platform: Optional[str] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, platform, details)
        self.command = command


class PathFormatError(PlatformError):
    """路径格式错误"""

    def __init__(self, message: str, path: str,
                 platform: Optional[str] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, platform, details)
        self.path = path


class EnvironmentVariableError(PlatformError):
    """环境变量错误"""

    def __init__(self, message: str, variable_name: str,
                 platform: Optional[str] = None,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, platform, details)
        self.variable_name = variable_name


class ValidationSystemError(Exception):
    """验证系统错误基类"""

    def __init__(self, message: str, details: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.message = message
        self.details = details or {}


class ConfigurationError(ValidationSystemError):
    """配置错误"""
    pass


class ResourceExhaustedError(ValidationSystemError):
    """资源不足错误"""

    def __init__(self, message: str, resource_type: str,
                 details: Optional[Dict[str, Any]] = None):
        super().__init__(message, details)
        self.resource_type = resource_type


class NetworkError(ValidationSystemError):
    """网络错误"""
    pass


# ============================================================================
# 错误响应数据类
# ============================================================================

@dataclass
class ErrorResponse:
    """错误响应"""
    error_type: str
    message: str
    severity: ErrorSeverity
    suggestions: List[str] = field(default_factory=list)
    recovery_actions: List[str] = field(default_factory=list)
    details: Dict[str, Any] = field(default_factory=dict)
    can_recover: bool = False
    exit_code: int = 1

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'error_type': self.error_type,
            'message': self.message,
            'severity': self.severity.value,
            'suggestions': self.suggestions,
            'recovery_actions': self.recovery_actions,
            'details': self.details,
            'can_recover': self.can_recover,
            'exit_code': self.exit_code
        }

    def format_message(self) -> str:
        """格式化错误消息"""
        lines = [
            f"[{self.severity.value.upper()}] {self.error_type}",
            f"  消息: {self.message}"
        ]

        if self.suggestions:
            lines.append("  建议:")
            for suggestion in self.suggestions:
                lines.append(f"    - {suggestion}")

        if self.recovery_actions:
            lines.append("  恢复操作:")
            for action in self.recovery_actions:
                lines.append(f"    - {action}")

        return '\n'.join(lines)


# ============================================================================
# 错误处理器
# ============================================================================

class ErrorHandler:
    """
    错误处理器

    处理脚本执行错误和异常，提供清晰的错误消息和修复建议。
    实现优雅的错误恢复机制。

    需求：3.1-3.5
    """

    # 常见依赖的安装建议
    DEPENDENCY_INSTALL_SUGGESTIONS = {
        'python': {
            'windows': '从 https://www.python.org/downloads/ 下载并安装Python',
            'linux': '运行: sudo apt-get install python3 或 sudo yum install python3',
            'wsl': '运行: sudo apt-get install python3'
        },
        'python3': {
            'windows': '从 https://www.python.org/downloads/ 下载并安装Python 3',
            'linux': '运行: sudo apt-get install python3',
            'wsl': '运行: sudo apt-get install python3'
        },
        'git': {
            'windows': '从 https://git-scm.com/download/win 下载并安装Git',
            'linux': '运行: sudo apt-get install git',
            'wsl': '运行: sudo apt-get install git'
        },
        'cmake': {
            'windows': '从 https://cmake.org/download/ 下载并安装CMake',
            'linux': '运行: sudo apt-get install cmake',
            'wsl': '运行: sudo apt-get install cmake'
        },
        'make': {
            'windows': '安装MinGW或使用WSL',
            'linux': '运行: sudo apt-get install build-essential',
            'wsl': '运行: sudo apt-get install build-essential'
        },
        'gcc': {
            'windows': '安装MinGW或使用WSL',
            'linux': '运行: sudo apt-get install gcc',
            'wsl': '运行: sudo apt-get install gcc'
        },
        'node': {
            'windows': '从 https://nodejs.org/ 下载并安装Node.js',
            'linux': '运行: sudo apt-get install nodejs',
            'wsl': '运行: sudo apt-get install nodejs'
        },
        'npm': {
            'windows': '安装Node.js时会自动安装npm',
            'linux': '运行: sudo apt-get install npm',
            'wsl': '运行: sudo apt-get install npm'
        }
    }

    # 权限修复建议
    PERMISSION_FIX_SUGGESTIONS = {
        'windows': [
            '以管理员身份运行命令提示符或PowerShell',
            '检查文件/目录的安全属性',
            '确保当前用户有足够的权限'
        ],
        'linux': [
            '使用 sudo 运行命令',
            '使用 chmod 修改文件权限: chmod +x script.sh',
            '检查文件所有者: ls -la',
            '使用 chown 修改文件所有者'
        ],
        'wsl': [
            '使用 sudo 运行命令',
            '检查Windows和WSL之间的权限映射',
            '确保脚本有执行权限: chmod +x script.sh'
        ]
    }

    def __init__(self):
        """初始化错误处理器"""
        self._error_history: List[ErrorResponse] = []

    def handle_script_error(self, error: ScriptError,
                           platform: str = 'unknown') -> ErrorResponse:
        """
        处理脚本执行错误

        需求 3.1, 3.2, 3.3, 3.4
        """
        if isinstance(error, DependencyMissingError):
            return self._handle_dependency_missing(error, platform)
        elif isinstance(error, PermissionDeniedError):
            return self._handle_permission_denied(error, platform)
        elif isinstance(error, ScriptSyntaxError):
            return self._handle_syntax_error(error)
        elif isinstance(error, TimeoutError):
            return self._handle_timeout(error)
        else:
            return self._handle_generic_script_error(error)

    def handle_platform_error(self, error: PlatformError) -> ErrorResponse:
        """
        处理平台兼容性错误

        需求 3.5
        """
        if isinstance(error, PlatformNotSupportedError):
            return self._handle_platform_not_supported(error)
        elif isinstance(error, CommandNotFoundError):
            return self._handle_command_not_found(error)
        elif isinstance(error, PathFormatError):
            return self._handle_path_format_error(error)
        elif isinstance(error, EnvironmentVariableError):
            return self._handle_env_variable_error(error)
        else:
            return self._handle_generic_platform_error(error)

    def handle_validation_error(self, error: ValidationSystemError) -> ErrorResponse:
        """处理验证系统错误"""
        if isinstance(error, ConfigurationError):
            return self._handle_configuration_error(error)
        elif isinstance(error, ResourceExhaustedError):
            return self._handle_resource_exhausted(error)
        elif isinstance(error, NetworkError):
            return self._handle_network_error(error)
        else:
            return self._handle_generic_validation_error(error)

    def handle_exception(self, exception: Exception,
                        context: Optional[Dict[str, Any]] = None) -> ErrorResponse:
        """处理通用异常"""
        context = context or {}

        # 尝试识别异常类型并转换为特定错误
        error_response = self._classify_exception(exception, context)

        # 记录错误历史
        self._error_history.append(error_response)

        return error_response

    def get_error_history(self) -> List[ErrorResponse]:
        """获取错误历史"""
        return self._error_history.copy()

    def clear_error_history(self):
        """清除错误历史"""
        self._error_history.clear()

    def create_error_from_exit_code(self, exit_code: int, stderr: str,
                                    script_path: Optional[Path] = None,
                                    platform: str = 'unknown') -> Optional[ErrorResponse]:
        """
        根据退出代码和错误输出创建错误响应

        需求 3.2
        """
        if exit_code == 0:
            return None

        # 分析stderr内容
        error_type, suggestions = self._analyze_stderr(stderr, platform)

        return ErrorResponse(
            error_type=error_type,
            message=f"脚本执行失败，退出代码: {exit_code}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=self._get_recovery_actions(error_type, platform),
            details={
                'exit_code': exit_code,
                'stderr': stderr,
                'script_path': str(script_path) if script_path else None,
                'platform': platform
            },
            can_recover=False,
            exit_code=exit_code
        )


    # ========================================================================
    # 私有方法 - 特定错误处理
    # ========================================================================

    def _handle_dependency_missing(self, error: DependencyMissingError,
                                   platform: str) -> ErrorResponse:
        """
        处理缺失依赖错误

        需求 3.1: 提供清晰的错误消息并优雅退出
        """
        suggestions = []

        # 获取特定依赖的安装建议
        dep_lower = error.dependency.lower()
        if dep_lower in self.DEPENDENCY_INSTALL_SUGGESTIONS:
            platform_suggestions = self.DEPENDENCY_INSTALL_SUGGESTIONS[dep_lower]
            if platform in platform_suggestions:
                suggestions.append(platform_suggestions[platform])
            else:
                # 添加所有平台的建议
                for plat, suggestion in platform_suggestions.items():
                    suggestions.append(f"[{plat}] {suggestion}")
        else:
            suggestions.append(f"请安装 {error.dependency} 依赖")

        suggestions.append("检查系统PATH环境变量是否包含依赖的安装路径")

        response = ErrorResponse(
            error_type="DependencyMissing",
            message=f"缺失依赖: {error.dependency}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=[
                f"安装 {error.dependency}",
                "重新运行脚本"
            ],
            details={
                'dependency': error.dependency,
                'script_path': str(error.script_path) if error.script_path else None,
                'platform': platform,
                **error.details
            },
            can_recover=True,
            exit_code=127  # 标准的"命令未找到"退出代码
        )

        self._error_history.append(response)
        return response

    def _handle_permission_denied(self, error: PermissionDeniedError,
                                  platform: str) -> ErrorResponse:
        """
        处理权限不足错误

        需求 3.3: 提供可操作的错误消息
        """
        suggestions = self.PERMISSION_FIX_SUGGESTIONS.get(platform, [])

        if error.resource_path:
            suggestions.insert(0, f"检查资源路径的权限: {error.resource_path}")

        response = ErrorResponse(
            error_type="PermissionDenied",
            message=f"权限不足: {error.message}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=[
                "获取必要的权限",
                "以管理员/root身份运行" if platform == 'windows' else "使用sudo运行",
                "重新运行脚本"
            ],
            details={
                'resource_path': str(error.resource_path) if error.resource_path else None,
                'script_path': str(error.script_path) if error.script_path else None,
                'platform': platform,
                **error.details
            },
            can_recover=True,
            exit_code=126  # 标准的"权限被拒绝"退出代码
        )

        self._error_history.append(response)
        return response

    def _handle_syntax_error(self, error: ScriptSyntaxError) -> ErrorResponse:
        """处理脚本语法错误"""
        suggestions = [
            "检查脚本语法是否正确",
            "使用语法检查工具验证脚本"
        ]

        if error.line_number:
            suggestions.insert(0, f"检查第 {error.line_number} 行附近的代码")

        response = ErrorResponse(
            error_type="SyntaxError",
            message=f"脚本语法错误: {error.message}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=[
                "修复语法错误",
                "重新运行脚本"
            ],
            details={
                'line_number': error.line_number,
                'script_path': str(error.script_path) if error.script_path else None,
                **error.details
            },
            can_recover=True,
            exit_code=2
        )

        self._error_history.append(response)
        return response

    def _handle_timeout(self, error: TimeoutError) -> ErrorResponse:
        """
        处理脚本执行超时

        需求 3.4: 清理临时文件并在可能的情况下恢复原始状态
        """
        response = ErrorResponse(
            error_type="Timeout",
            message=f"脚本执行超时 ({error.timeout_seconds}秒)",
            severity=ErrorSeverity.ERROR,
            suggestions=[
                "检查脚本是否存在无限循环",
                "检查网络连接是否正常",
                "考虑增加超时时间",
                "检查脚本是否在等待用户输入"
            ],
            recovery_actions=[
                "终止脚本进程",
                "清理临时文件",
                "恢复原始状态",
                "使用更长的超时时间重试"
            ],
            details={
                'timeout_seconds': error.timeout_seconds,
                'script_path': str(error.script_path) if error.script_path else None,
                **error.details
            },
            can_recover=True,
            exit_code=124  # 标准的超时退出代码
        )

        self._error_history.append(response)
        return response

    def _handle_generic_script_error(self, error: ScriptError) -> ErrorResponse:
        """处理通用脚本错误"""
        response = ErrorResponse(
            error_type="ScriptError",
            message=error.message,
            severity=ErrorSeverity.ERROR,
            suggestions=[
                "检查脚本日志获取更多信息",
                "验证脚本输入参数是否正确",
                "确保所有依赖都已安装"
            ],
            recovery_actions=[
                "检查并修复问题",
                "重新运行脚本"
            ],
            details={
                'script_path': str(error.script_path) if error.script_path else None,
                **error.details
            },
            can_recover=False,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_platform_not_supported(self, error: PlatformNotSupportedError) -> ErrorResponse:
        """
        处理平台不支持错误

        需求 3.5: 提供替代解决方案或清晰说明
        """
        suggestions = [
            "检查脚本是否有其他平台的等效版本",
            "考虑使用跨平台脚本（如Python）"
        ]

        if error.platform == 'windows':
            suggestions.append("考虑使用WSL运行Linux脚本")
        elif error.platform in ['linux', 'wsl']:
            suggestions.append("考虑使用Wine运行Windows脚本（如果适用）")

        response = ErrorResponse(
            error_type="PlatformNotSupported",
            message=f"当前平台不支持此脚本: {error.message}",
            severity=ErrorSeverity.WARNING,
            suggestions=suggestions,
            recovery_actions=[
                "使用支持的平台运行脚本",
                "使用等效的平台特定脚本"
            ],
            details={
                'platform': error.platform,
                **error.details
            },
            can_recover=True,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_command_not_found(self, error: CommandNotFoundError) -> ErrorResponse:
        """处理命令不可用错误"""
        suggestions = []

        # 获取命令的安装建议
        cmd_lower = error.command.lower()
        if cmd_lower in self.DEPENDENCY_INSTALL_SUGGESTIONS:
            platform_suggestions = self.DEPENDENCY_INSTALL_SUGGESTIONS[cmd_lower]
            if error.platform and error.platform in platform_suggestions:
                suggestions.append(platform_suggestions[error.platform])

        suggestions.extend([
            f"确保 {error.command} 已安装",
            "检查PATH环境变量是否包含命令路径"
        ])

        response = ErrorResponse(
            error_type="CommandNotFound",
            message=f"命令不可用: {error.command}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=[
                f"安装 {error.command}",
                "更新PATH环境变量",
                "重新运行脚本"
            ],
            details={
                'command': error.command,
                'platform': error.platform,
                **error.details
            },
            can_recover=True,
            exit_code=127
        )

        self._error_history.append(response)
        return response

    def _handle_path_format_error(self, error: PathFormatError) -> ErrorResponse:
        """处理路径格式错误"""
        suggestions = []

        if error.platform == 'windows':
            suggestions.extend([
                "使用反斜杠(\\)作为路径分隔符",
                "确保路径以驱动器字母开头（如 C:\\）"
            ])
        elif error.platform in ['linux', 'wsl']:
            suggestions.extend([
                "使用正斜杠(/)作为路径分隔符",
                "确保路径以/开头（绝对路径）"
            ])
        else:
            # 未知平台，提供通用建议
            suggestions.extend([
                "检查路径格式是否符合当前平台要求",
                "确保路径分隔符正确"
            ])

        if error.platform == 'wsl':
            suggestions.append("使用 /mnt/c/ 格式访问Windows驱动器")

        response = ErrorResponse(
            error_type="PathFormatError",
            message=f"路径格式错误: {error.path}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=[
                "修正路径格式",
                "重新运行脚本"
            ],
            details={
                'path': error.path,
                'platform': error.platform,
                **error.details
            },
            can_recover=True,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_env_variable_error(self, error: EnvironmentVariableError) -> ErrorResponse:
        """处理环境变量错误"""
        suggestions = [
            f"设置环境变量 {error.variable_name}",
            "检查环境变量是否正确导出"
        ]

        if error.platform == 'windows':
            suggestions.append(f"使用 set {error.variable_name}=value 设置变量")
        else:
            suggestions.append(f"使用 export {error.variable_name}=value 设置变量")

        response = ErrorResponse(
            error_type="EnvironmentVariableError",
            message=f"环境变量错误: {error.variable_name}",
            severity=ErrorSeverity.ERROR,
            suggestions=suggestions,
            recovery_actions=[
                f"设置 {error.variable_name} 环境变量",
                "重新运行脚本"
            ],
            details={
                'variable_name': error.variable_name,
                'platform': error.platform,
                **error.details
            },
            can_recover=True,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_generic_platform_error(self, error: PlatformError) -> ErrorResponse:
        """处理通用平台错误"""
        response = ErrorResponse(
            error_type="PlatformError",
            message=error.message,
            severity=ErrorSeverity.ERROR,
            suggestions=[
                "检查平台兼容性",
                "查看脚本文档了解支持的平台"
            ],
            recovery_actions=[
                "使用支持的平台",
                "修复平台兼容性问题"
            ],
            details={
                'platform': error.platform,
                **error.details
            },
            can_recover=False,
            exit_code=1
        )

        self._error_history.append(response)
        return response


    def _handle_configuration_error(self, error: ConfigurationError) -> ErrorResponse:
        """处理配置错误"""
        response = ErrorResponse(
            error_type="ConfigurationError",
            message=f"配置错误: {error.message}",
            severity=ErrorSeverity.ERROR,
            suggestions=[
                "检查配置文件格式是否正确",
                "验证配置参数值是否有效",
                "参考文档了解正确的配置方式"
            ],
            recovery_actions=[
                "修复配置文件",
                "重新运行验证"
            ],
            details=error.details,
            can_recover=True,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_resource_exhausted(self, error: ResourceExhaustedError) -> ErrorResponse:
        """处理资源不足错误"""
        suggestions = []

        if error.resource_type == 'memory':
            suggestions.extend([
                "关闭其他占用内存的程序",
                "增加系统内存或交换空间",
                "减少并行执行的任务数量"
            ])
        elif error.resource_type == 'disk':
            suggestions.extend([
                "清理磁盘空间",
                "删除不需要的临时文件",
                "移动数据到其他磁盘"
            ])
        elif error.resource_type == 'cpu':
            suggestions.extend([
                "减少并行执行的任务数量",
                "等待其他进程完成",
                "优化脚本性能"
            ])
        elif error.resource_type == 'network':
            suggestions.extend([
                "检查网络连接是否正常",
                "检查防火墙设置",
                "验证代理配置是否正确"
            ])
        else:
            # 未知资源类型，提供通用建议
            suggestions.extend([
                "检查系统资源使用情况",
                "释放不必要的资源"
            ])

        response = ErrorResponse(
            error_type="ResourceExhausted",
            message=f"资源不足: {error.resource_type}",
            severity=ErrorSeverity.CRITICAL,
            suggestions=suggestions,
            recovery_actions=[
                "释放系统资源",
                "重新运行验证"
            ],
            details={
                'resource_type': error.resource_type,
                **error.details
            },
            can_recover=True,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_network_error(self, error: NetworkError) -> ErrorResponse:
        """处理网络错误"""
        response = ErrorResponse(
            error_type="NetworkError",
            message=f"网络错误: {error.message}",
            severity=ErrorSeverity.ERROR,
            suggestions=[
                "检查网络连接是否正常",
                "检查防火墙设置",
                "验证代理配置是否正确",
                "稍后重试"
            ],
            recovery_actions=[
                "修复网络连接",
                "配置正确的代理",
                "重新运行验证"
            ],
            details=error.details,
            can_recover=True,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    def _handle_generic_validation_error(self, error: ValidationSystemError) -> ErrorResponse:
        """处理通用验证系统错误"""
        response = ErrorResponse(
            error_type="ValidationSystemError",
            message=error.message,
            severity=ErrorSeverity.ERROR,
            suggestions=[
                "检查验证系统配置",
                "查看日志获取更多信息"
            ],
            recovery_actions=[
                "修复问题",
                "重新运行验证"
            ],
            details=error.details,
            can_recover=False,
            exit_code=1
        )

        self._error_history.append(response)
        return response

    # ========================================================================
    # 私有方法 - 辅助功能
    # ========================================================================

    def _classify_exception(self, exception: Exception,
                           context: Dict[str, Any]) -> ErrorResponse:
        """分类异常并创建适当的错误响应"""
        exc_type = type(exception).__name__
        exc_message = str(exception)

        # 尝试识别常见的异常类型
        if isinstance(exception, FileNotFoundError):
            return ErrorResponse(
                error_type="FileNotFound",
                message=f"文件未找到: {exc_message}",
                severity=ErrorSeverity.ERROR,
                suggestions=[
                    "检查文件路径是否正确",
                    "确保文件存在"
                ],
                recovery_actions=["创建或定位正确的文件"],
                details={'exception': exc_type, 'context': context},
                can_recover=True,
                exit_code=2
            )

        elif isinstance(exception, PermissionError):
            return ErrorResponse(
                error_type="PermissionDenied",
                message=f"权限被拒绝: {exc_message}",
                severity=ErrorSeverity.ERROR,
                suggestions=["检查文件/目录权限", "以管理员身份运行"],
                recovery_actions=["获取必要的权限"],
                details={'exception': exc_type, 'context': context},
                can_recover=True,
                exit_code=126
            )

        elif isinstance(exception, OSError):
            return ErrorResponse(
                error_type="OSError",
                message=f"操作系统错误: {exc_message}",
                severity=ErrorSeverity.ERROR,
                suggestions=["检查系统资源", "验证操作是否被支持"],
                recovery_actions=["修复系统问题"],
                details={'exception': exc_type, 'context': context},
                can_recover=False,
                exit_code=1
            )

        else:
            # 通用异常处理
            return ErrorResponse(
                error_type=exc_type,
                message=exc_message,
                severity=ErrorSeverity.ERROR,
                suggestions=["查看详细错误信息", "检查日志"],
                recovery_actions=["修复问题并重试"],
                details={
                    'exception': exc_type,
                    'traceback': traceback.format_exc(),
                    'context': context
                },
                can_recover=False,
                exit_code=1
            )

    def _analyze_stderr(self, stderr: str, platform: str) -> tuple:
        """分析stderr内容，识别错误类型"""
        stderr_lower = stderr.lower()

        # 检查常见错误模式
        if 'command not found' in stderr_lower or 'not recognized' in stderr_lower:
            # 尝试提取命令名
            match = re.search(r"'([^']+)'.*not found", stderr_lower)
            if match:
                cmd = match.group(1)
                return 'CommandNotFound', [f"安装 {cmd} 命令"]
            return 'CommandNotFound', ["检查命令是否已安装"]

        elif 'permission denied' in stderr_lower:
            suggestions = self.PERMISSION_FIX_SUGGESTIONS.get(platform, [])
            if not suggestions:
                suggestions = ["检查文件/目录权限", "以管理员身份运行"]
            return 'PermissionDenied', suggestions

        elif 'no such file or directory' in stderr_lower:
            return 'FileNotFound', ["检查文件路径是否正确", "确保文件存在"]

        elif 'syntax error' in stderr_lower:
            return 'SyntaxError', ["检查脚本语法", "使用语法检查工具"]

        elif 'out of memory' in stderr_lower or 'memory' in stderr_lower:
            return 'ResourceExhausted', ["释放内存", "减少并行任务"]

        elif 'timeout' in stderr_lower or 'timed out' in stderr_lower:
            return 'Timeout', ["增加超时时间", "检查网络连接"]

        elif 'connection' in stderr_lower or 'network' in stderr_lower:
            return 'NetworkError', ["检查网络连接", "验证代理设置"]

        else:
            return 'ScriptError', ["检查脚本日志", "验证输入参数"]

    def _get_recovery_actions(self, error_type: str, platform: str) -> List[str]:
        """获取恢复操作建议"""
        common_actions = ["检查并修复问题", "重新运行脚本"]

        specific_actions = {
            'CommandNotFound': ["安装缺失的命令", "更新PATH环境变量"],
            'PermissionDenied': ["获取必要的权限"],
            'FileNotFound': ["创建或定位正确的文件"],
            'SyntaxError': ["修复语法错误"],
            'ResourceExhausted': ["释放系统资源"],
            'Timeout': ["增加超时时间", "优化脚本性能"],
            'NetworkError': ["修复网络连接"]
        }

        actions = specific_actions.get(error_type, [])
        return actions + common_actions
