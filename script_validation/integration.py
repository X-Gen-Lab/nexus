"""
组件集成模块

提供统一的API将所有验证系统组件连接在一起，实现完整的验证工作流程。
确保所有组件正确协作：
- 验证控制器与所有子系统集成
- 脚本发现、分类和管理
- 平台检测和适配器管理
- 验证器执行和结果收集
- 报告生成和输出
- CI/CD集成

需求：所有需求
"""

import logging
from pathlib import Path
from typing import List, Dict, Any, Optional, Callable
from datetime import datetime

from .models import (
    ValidationConfig, ValidationReport, ValidationResult, ValidationSummary,
    CompatibilityMatrix, Script, Platform, ValidationStatus, ScriptType,
    EnvironmentInfo
)
from .controllers import ValidationController
from .managers import ScriptManager, PlatformManager
from .validators import (
    FunctionalValidator, CompatibilityValidator,
    PerformanceValidator, DocumentationValidator
)
from .reporters import HTMLReporter, JSONReporter, SummaryReporter, JUnitReporter
from .handlers import ErrorHandler, ResourceManager
from .ci_integration import CIIntegration, ExitCode, is_ci_environment
from .discovery import ScriptDiscovery, ScriptParser, ScriptClassifier


# 设置日志
logger = logging.getLogger(__name__)


class ValidationWorkflow:
    """验证工作流程管理器

    提供完整的验证工作流程，集成所有子系统组件。
    支持自定义配置、回调函数和扩展点。

    Attributes:
        config: 验证配置
        controller: 验证控制器
        ci_integration: CI集成管理器
        error_handler: 错误处理器
        resource_manager: 资源管理器
    """

    def __init__(self, config: Optional[ValidationConfig] = None):
        """初始化验证工作流程

        Args:
            config: 验证配置，如果为None则使用默认配置
        """
        self.config = config or self._create_default_config()
        self.controller = ValidationController(self.config)
        self.ci_integration = CIIntegration()
        self.error_handler = ErrorHandler()
        self.resource_manager = ResourceManager()

        # 回调函数
        self._on_script_discovered: Optional[Callable[[Script], None]] = None
        self._on_validation_start: Optional[Callable[[Script, Platform], None]] = None
        self._on_validation_complete: Optional[Callable[[ValidationResult], None]] = None
        self._on_report_generated: Optional[Callable[[str, str], None]] = None

        logger.info("ValidationWorkflow initialized")

    def _create_default_config(self) -> ValidationConfig:
        """创建默认配置

        Returns:
            ValidationConfig: 默认验证配置
        """
        return ValidationConfig(
            root_path=Path.cwd(),
            target_platforms=[Platform.WINDOWS, Platform.WSL, Platform.LINUX],
            script_patterns=["*.bat", "*.ps1", "*.sh", "*.py"],
            generate_html_report=True,
            generate_json_report=True,
            generate_summary_report=True
        )

    def run(self) -> ValidationReport:
        """运行完整的验证工作流程

        执行以下步骤：
        1. 发现脚本
        2. 检测可用平台
        3. 执行验证
        4. 生成报告
        5. 返回结果

        Returns:
            ValidationReport: 完整的验证报告
        """
        logger.info("Starting validation workflow")

        try:
            # 运行验证
            report = self.controller.run_validation()

            # 生成报告
            output_dir = self.config.output_dir or (self.config.root_path / "build" / "validation_reports")
            generated_reports = self.controller.generate_reports(report, output_dir)

            # 触发报告生成回调
            for format_name, path in generated_reports.items():
                if self._on_report_generated:
                    self._on_report_generated(format_name, path)

            logger.info(f"Validation workflow completed. Generated {len(generated_reports)} reports")
            return report

        except Exception as e:
            logger.error(f"Validation workflow failed: {e}")
            # 清理资源
            self.resource_manager.cleanup_all_resources()
            raise RuntimeError(f"Validation failed: {str(e)}") from e
        finally:
            # 确保资源被清理
            self.resource_manager.cleanup_all_resources()

    def run_quick_validation(self) -> ValidationReport:
        """运行快速验证

        只执行功能验证器，跳过性能和文档验证。

        Returns:
            ValidationReport: 验证报告
        """
        # 临时修改配置
        original_validators = self.config.enabled_validators
        self.config.enabled_validators = ['functional']
        self.config.validation_mode = 'quick'

        try:
            # 重新初始化控制器
            self.controller = ValidationController(self.config)
            return self.run()
        finally:
            # 恢复配置
            self.config.enabled_validators = original_validators
            self.config.validation_mode = 'full'

    def run_platform_specific(self, platform: Platform) -> ValidationReport:
        """运行平台特定验证

        只验证指定平台上的脚本。

        Args:
            platform: 目标平台

        Returns:
            ValidationReport: 验证报告
        """
        # 临时修改配置
        original_platforms = self.config.target_platforms
        self.config.target_platforms = [platform]
        self.config.validation_mode = 'platform-specific'

        try:
            # 重新初始化控制器
            self.controller = ValidationController(self.config)
            return self.run()
        finally:
            # 恢复配置
            self.config.target_platforms = original_platforms
            self.config.validation_mode = 'full'

    def discover_scripts(self) -> List[Script]:
        """发现项目中的所有脚本

        Returns:
            List[Script]: 发现的脚本列表
        """
        scripts = self.controller.script_manager.discover_scripts(
            self.config.root_path,
            self.config.script_patterns
        )

        # 触发回调
        for script in scripts:
            if self._on_script_discovered:
                self._on_script_discovered(script)

        return scripts

    def check_platforms(self) -> Dict[Platform, bool]:
        """检查所有平台的可用性

        Returns:
            Dict[Platform, bool]: 平台到可用性的映射
        """
        availability = {}
        for platform in self.config.target_platforms:
            availability[platform] = self.controller.platform_manager.is_platform_available(platform)
        return availability

    def get_environment_info(self) -> EnvironmentInfo:
        """获取当前环境信息

        Returns:
            EnvironmentInfo: 环境信息
        """
        return self.controller.platform_manager.get_current_environment_info()

    def get_exit_code(self, report: ValidationReport) -> int:
        """获取CI/CD退出代码

        Args:
            report: 验证报告

        Returns:
            int: 退出代码
        """
        return self.ci_integration.get_exit_code(report)

    def print_summary(self, report: ValidationReport) -> None:
        """打印验证摘要

        Args:
            report: 验证报告
        """
        if self.ci_integration.is_ci:
            self.ci_integration.print_ci_summary(report)
        else:
            self._print_console_summary(report)

    def _print_console_summary(self, report: ValidationReport) -> None:
        """打印控制台摘要

        Args:
            report: 验证报告
        """
        summary = report.summary

        print("\n" + "=" * 60)
        print("验证摘要")
        print("=" * 60)

        print(f"\n总脚本数: {summary.total_scripts}")
        print(f"  ✓ 通过: {summary.passed}")
        print(f"  ✗ 失败: {summary.failed}")
        print(f"  ○ 跳过: {summary.skipped}")
        print(f"  ! 错误: {summary.errors}")
        print(f"\n总执行时间: {summary.execution_time:.2f}秒")

        if summary.total_scripts > 0:
            pass_rate = (summary.passed / summary.total_scripts) * 100
            print(f"通过率: {pass_rate:.1f}%")

        if report.recommendations:
            print("\n建议:")
            for rec in report.recommendations:
                print(f"  • {rec}")

        print("\n" + "=" * 60)

    # 回调设置方法

    def on_script_discovered(self, callback: Callable[[Script], None]) -> 'ValidationWorkflow':
        """设置脚本发现回调

        Args:
            callback: 回调函数

        Returns:
            self: 支持链式调用
        """
        self._on_script_discovered = callback
        return self

    def on_validation_start(self, callback: Callable[[Script, Platform], None]) -> 'ValidationWorkflow':
        """设置验证开始回调

        Args:
            callback: 回调函数

        Returns:
            self: 支持链式调用
        """
        self._on_validation_start = callback
        return self

    def on_validation_complete(self, callback: Callable[[ValidationResult], None]) -> 'ValidationWorkflow':
        """设置验证完成回调

        Args:
            callback: 回调函数

        Returns:
            self: 支持链式调用
        """
        self._on_validation_complete = callback
        return self

    def on_report_generated(self, callback: Callable[[str, str], None]) -> 'ValidationWorkflow':
        """设置报告生成回调

        Args:
            callback: 回调函数

        Returns:
            self: 支持链式调用
        """
        self._on_report_generated = callback
        return self


class ComponentRegistry:
    """组件注册表

    管理所有验证系统组件的注册和获取。
    支持自定义组件扩展。
    """

    _validators: Dict[str, type] = {}
    _reporters: Dict[str, type] = {}
    _adapters: Dict[Platform, type] = {}

    @classmethod
    def register_validator(cls, name: str, validator_class: type) -> None:
        """注册验证器

        Args:
            name: 验证器名称
            validator_class: 验证器类
        """
        cls._validators[name] = validator_class
        logger.info(f"Registered validator: {name}")

    @classmethod
    def register_reporter(cls, name: str, reporter_class: type) -> None:
        """注册报告生成器

        Args:
            name: 报告器名称
            reporter_class: 报告器类
        """
        cls._reporters[name] = reporter_class
        logger.info(f"Registered reporter: {name}")

    @classmethod
    def register_adapter(cls, platform: Platform, adapter_class: type) -> None:
        """注册平台适配器

        Args:
            platform: 平台类型
            adapter_class: 适配器类
        """
        cls._adapters[platform] = adapter_class
        logger.info(f"Registered adapter for platform: {platform.value}")

    @classmethod
    def get_validator(cls, name: str) -> Optional[type]:
        """获取验证器类

        Args:
            name: 验证器名称

        Returns:
            验证器类或None
        """
        return cls._validators.get(name)

    @classmethod
    def get_reporter(cls, name: str) -> Optional[type]:
        """获取报告器类

        Args:
            name: 报告器名称

        Returns:
            报告器类或None
        """
        return cls._reporters.get(name)

    @classmethod
    def get_adapter(cls, platform: Platform) -> Optional[type]:
        """获取适配器类

        Args:
            platform: 平台类型

        Returns:
            适配器类或None
        """
        return cls._adapters.get(platform)

    @classmethod
    def list_validators(cls) -> List[str]:
        """列出所有注册的验证器

        Returns:
            验证器名称列表
        """
        return list(cls._validators.keys())

    @classmethod
    def list_reporters(cls) -> List[str]:
        """列出所有注册的报告器

        Returns:
            报告器名称列表
        """
        return list(cls._reporters.keys())

    @classmethod
    def list_adapters(cls) -> List[Platform]:
        """列出所有注册的适配器

        Returns:
            平台列表
        """
        return list(cls._adapters.keys())


# 注册默认组件
def _register_default_components():
    """注册默认组件"""
    # 注册验证器
    ComponentRegistry.register_validator('functional', FunctionalValidator)
    ComponentRegistry.register_validator('compatibility', CompatibilityValidator)
    ComponentRegistry.register_validator('performance', PerformanceValidator)
    ComponentRegistry.register_validator('documentation', DocumentationValidator)

    # 注册报告器
    ComponentRegistry.register_reporter('html', HTMLReporter)
    ComponentRegistry.register_reporter('json', JSONReporter)
    ComponentRegistry.register_reporter('summary', SummaryReporter)
    ComponentRegistry.register_reporter('junit', JUnitReporter)

    # 注册适配器
    from .adapters import WindowsAdapter, WSLAdapter, LinuxAdapter
    ComponentRegistry.register_adapter(Platform.WINDOWS, WindowsAdapter)
    ComponentRegistry.register_adapter(Platform.WSL, WSLAdapter)
    ComponentRegistry.register_adapter(Platform.LINUX, LinuxAdapter)


# 模块加载时注册默认组件
_register_default_components()




# 便捷函数

def create_workflow(
    root_path: Optional[Path] = None,
    platforms: Optional[List[Platform]] = None,
    validators: Optional[List[str]] = None,
    report_formats: Optional[List[str]] = None,
    ci_mode: bool = False
) -> ValidationWorkflow:
    """创建验证工作流程

    便捷函数，用于快速创建配置好的验证工作流程。

    Args:
        root_path: 项目根目录
        platforms: 目标平台列表
        validators: 启用的验证器列表
        report_formats: 报告格式列表
        ci_mode: 是否为CI模式

    Returns:
        ValidationWorkflow: 配置好的工作流程
    """
    config = ValidationConfig(
        root_path=root_path or Path.cwd(),
        target_platforms=platforms or [Platform.WINDOWS, Platform.WSL, Platform.LINUX],
        enabled_validators=validators or ['functional', 'compatibility', 'performance', 'documentation'],
        generate_html_report='html' in (report_formats or ['html', 'json', 'summary']),
        generate_json_report='json' in (report_formats or ['html', 'json', 'summary']),
        generate_summary_report='summary' in (report_formats or ['html', 'json', 'summary']),
        generate_junit_report='junit' in (report_formats or []),
        ci_mode=ci_mode or is_ci_environment()
    )

    return ValidationWorkflow(config)


def run_validation(
    root_path: Optional[Path] = None,
    platforms: Optional[List[Platform]] = None,
    mode: str = 'full'
) -> ValidationReport:
    """运行验证

    便捷函数，用于快速运行验证。

    Args:
        root_path: 项目根目录
        platforms: 目标平台列表
        mode: 验证模式 ('full', 'quick', 'platform-specific')

    Returns:
        ValidationReport: 验证报告
    """
    workflow = create_workflow(root_path=root_path, platforms=platforms)

    if mode == 'quick':
        return workflow.run_quick_validation()
    elif mode == 'platform-specific' and platforms and len(platforms) == 1:
        return workflow.run_platform_specific(platforms[0])
    else:
        return workflow.run()


def discover_scripts(root_path: Optional[Path] = None) -> List[Script]:
    """发现脚本

    便捷函数，用于快速发现项目中的脚本。

    Args:
        root_path: 项目根目录

    Returns:
        List[Script]: 发现的脚本列表
    """
    workflow = create_workflow(root_path=root_path)
    return workflow.discover_scripts()


def check_platform_availability() -> Dict[Platform, bool]:
    """检查平台可用性

    便捷函数，用于快速检查所有平台的可用性。

    Returns:
        Dict[Platform, bool]: 平台可用性映射
    """
    workflow = create_workflow()
    return workflow.check_platforms()


def get_system_info() -> Dict[str, Any]:
    """获取系统信息

    便捷函数，用于获取当前系统的完整信息。

    Returns:
        Dict[str, Any]: 系统信息字典
    """
    workflow = create_workflow()
    env_info = workflow.get_environment_info()
    platform_availability = workflow.check_platforms()
    ci_info = workflow.ci_integration.get_ci_environment_info()

    return {
        'environment': {
            'platform': env_info.platform.value,
            'os_version': env_info.os_version,
            'python_version': env_info.python_version,
            'shell_version': env_info.shell_version,
            'available_commands': env_info.available_commands
        },
        'platform_availability': {
            p.value: available for p, available in platform_availability.items()
        },
        'ci': ci_info
    }


class ValidationBuilder:
    """验证构建器

    使用构建器模式创建和配置验证工作流程。
    """

    def __init__(self):
        """初始化构建器"""
        self._root_path: Optional[Path] = None
        self._platforms: List[Platform] = []
        self._validators: List[str] = []
        self._report_formats: List[str] = []
        self._timeout: int = 300
        self._max_memory: int = 1024
        self._ci_mode: bool = False
        self._verbose: bool = False
        self._parallel: bool = True

    def root_path(self, path: Path) -> 'ValidationBuilder':
        """设置根路径

        Args:
            path: 项目根目录

        Returns:
            self: 支持链式调用
        """
        self._root_path = path
        return self

    def platforms(self, *platforms: Platform) -> 'ValidationBuilder':
        """设置目标平台

        Args:
            platforms: 平台列表

        Returns:
            self: 支持链式调用
        """
        self._platforms = list(platforms)
        return self

    def validators(self, *validators: str) -> 'ValidationBuilder':
        """设置验证器

        Args:
            validators: 验证器名称列表

        Returns:
            self: 支持链式调用
        """
        self._validators = list(validators)
        return self

    def report_formats(self, *formats: str) -> 'ValidationBuilder':
        """设置报告格式

        Args:
            formats: 报告格式列表

        Returns:
            self: 支持链式调用
        """
        self._report_formats = list(formats)
        return self

    def timeout(self, seconds: int) -> 'ValidationBuilder':
        """设置超时时间

        Args:
            seconds: 超时秒数

        Returns:
            self: 支持链式调用
        """
        self._timeout = seconds
        return self

    def max_memory(self, mb: int) -> 'ValidationBuilder':
        """设置最大内存

        Args:
            mb: 最大内存（MB）

        Returns:
            self: 支持链式调用
        """
        self._max_memory = mb
        return self

    def ci_mode(self, enabled: bool = True) -> 'ValidationBuilder':
        """设置CI模式

        Args:
            enabled: 是否启用

        Returns:
            self: 支持链式调用
        """
        self._ci_mode = enabled
        return self

    def verbose(self, enabled: bool = True) -> 'ValidationBuilder':
        """设置详细输出

        Args:
            enabled: 是否启用

        Returns:
            self: 支持链式调用
        """
        self._verbose = enabled
        return self

    def parallel(self, enabled: bool = True) -> 'ValidationBuilder':
        """设置并行执行

        Args:
            enabled: 是否启用

        Returns:
            self: 支持链式调用
        """
        self._parallel = enabled
        return self

    def build(self) -> ValidationWorkflow:
        """构建验证工作流程

        Returns:
            ValidationWorkflow: 配置好的工作流程
        """
        config = ValidationConfig(
            root_path=self._root_path or Path.cwd(),
            target_platforms=self._platforms or [Platform.WINDOWS, Platform.WSL, Platform.LINUX],
            enabled_validators=self._validators or ['functional', 'compatibility', 'performance', 'documentation'],
            generate_html_report='html' in self._report_formats or not self._report_formats,
            generate_json_report='json' in self._report_formats or not self._report_formats,
            generate_summary_report='summary' in self._report_formats or not self._report_formats,
            generate_junit_report='junit' in self._report_formats,
            timeout_seconds=self._timeout,
            max_memory_mb=self._max_memory,
            ci_mode=self._ci_mode or is_ci_environment(),
            verbose=self._verbose,
            parallel_execution=self._parallel
        )

        return ValidationWorkflow(config)


# 导出
__all__ = [
    'ValidationWorkflow',
    'ComponentRegistry',
    'ValidationBuilder',
    'create_workflow',
    'run_validation',
    'discover_scripts',
    'check_platform_availability',
    'get_system_info',
]
