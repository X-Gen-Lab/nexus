"""
主验证控制器

协调整个验证流程的核心控制器。
集成所有验证器（Functional、Compatibility、Performance、Documentation）
和报告生成器（HTML、JSON、Summary），实现完整的验证流程协调。
"""

from typing import List, Dict, Any, Optional, Type
from datetime import datetime
from pathlib import Path
import time
import logging

from ..models import (
    ValidationConfig, ValidationResult, ValidationReport,
    Platform, Script, ValidationSummary, CompatibilityMatrix,
    ValidationStatus, ScriptType
)
from ..managers import ScriptManager, PlatformManager
from ..interfaces import BaseValidator, ReportGenerator


# 设置日志
logger = logging.getLogger(__name__)


class ValidationController:
    """主验证控制器

    协调整个验证流程，集成所有验证器和报告生成器，
    管理验证器和报告器的生命周期。

    Attributes:
        config: 验证配置
        script_manager: 脚本管理器
        platform_manager: 平台管理器
        validators: 验证器字典
        reporters: 报告生成器字典
    """

    def __init__(self, config: ValidationConfig):
        """初始化验证控制器

        Args:
            config: 验证配置对象
        """
        self.config = config
        self.script_manager = ScriptManager()
        self.platform_manager = PlatformManager()
        self._validation_start_time: Optional[float] = None
        self._validation_end_time: Optional[float] = None

        # 初始化验证器和报告器
        self.validators = self._initialize_validators()
        self.reporters = self._initialize_reporters()

        logger.info(f"ValidationController initialized with {len(self.validators)} validators and {len(self.reporters)} reporters")

    def run_validation(self) -> ValidationReport:
        """运行完整的验证流程

        执行所有配置的验证器，收集结果，生成报告。

        Returns:
            ValidationReport: 完整的验证报告
        """
        self._validation_start_time = time.time()
        logger.info("Starting validation run")

        try:
            # 发现脚本
            scripts = self.script_manager.discover_scripts(
                self.config.root_path,
                self.config.script_patterns
            )
            logger.info(f"Discovered {len(scripts)} scripts")

            # 初始化结果收集
            all_results = []
            compatibility_matrix = CompatibilityMatrix(
                scripts=scripts,
                platforms=self.config.target_platforms
            )

            # 对每个平台执行验证
            for platform in self.config.target_platforms:
                if self.platform_manager.is_platform_available(platform):
                    logger.info(f"Validating platform: {platform.value}")
                    platform_results = self.validate_platform(platform, scripts)
                    all_results.extend(platform_results)

                    # 更新兼容性矩阵
                    for result in platform_results:
                        compatibility_matrix.set_result(
                            result.script.name,
                            platform,
                            result
                        )
                else:
                    logger.warning(f"Platform {platform.value} is not available, skipping")

            # 生成摘要
            summary = self._generate_summary(all_results)

            # 创建验证报告
            report = ValidationReport(
                timestamp=datetime.now(),
                environment=self.platform_manager.get_current_environment_info(),
                summary=summary,
                results=all_results,
                compatibility_matrix=compatibility_matrix,
                recommendations=self._generate_recommendations(all_results)
            )

            self._validation_end_time = time.time()
            logger.info(f"Validation completed in {self._validation_end_time - self._validation_start_time:.2f}s")

            return report

        except Exception as e:
            logger.error(f"Validation run failed: {e}")
            raise

    def validate_platform(self, platform: Platform, scripts: List[Script]) -> List[ValidationResult]:
        """验证特定平台上的所有脚本

        Args:
            platform: 目标平台
            scripts: 要验证的脚本列表

        Returns:
            List[ValidationResult]: 验证结果列表
        """
        results = []

        for script in scripts:
            # 检查脚本是否适用于该平台
            if self._is_script_compatible(script, platform):
                result = self.validate_script(script, platform)
                results.append(result)

        return results

    def validate_script(self, script: Script, platform: Platform) -> ValidationResult:
        """验证特定脚本在特定平台上的执行

        使用所有启用的验证器验证脚本，合并结果。

        Args:
            script: 要验证的脚本
            platform: 目标平台

        Returns:
            ValidationResult: 合并后的验证结果
        """
        start_time = time.time()
        all_details = {}
        combined_output = []
        combined_errors = []
        overall_status = ValidationStatus.PASSED
        total_memory = 0
        validators_used = []

        # 获取启用的验证器
        enabled_validators = self._get_enabled_validators()

        if not enabled_validators:
            # 如果没有启用的验证器，返回跳过状态
            return ValidationResult(
                script=script,
                platform=platform,
                validator="none",
                status=ValidationStatus.SKIPPED,
                execution_time=0.0,
                memory_usage=0,
                output="No validators enabled",
                error=None
            )

        # 使用每个验证器进行验证
        for validator_name, validator in enabled_validators.items():
            try:
                logger.debug(f"Running {validator_name} on {script.name} for {platform.value}")
                result = validator.validate(script, platform)

                validators_used.append(validator_name)
                all_details[validator_name] = result.details

                if result.output:
                    combined_output.append(f"[{validator_name}] {result.output}")
                if result.error:
                    combined_errors.append(f"[{validator_name}] {result.error}")

                total_memory = max(total_memory, result.memory_usage)

                # 更新整体状态（取最严重的状态）
                overall_status = self._merge_status(overall_status, result.status)

            except Exception as e:
                logger.error(f"Validator {validator_name} failed for {script.name}: {e}")
                combined_errors.append(f"[{validator_name}] Exception: {str(e)}")
                overall_status = self._merge_status(overall_status, ValidationStatus.ERROR)

        execution_time = time.time() - start_time

        return ValidationResult(
            script=script,
            platform=platform,
            validator=", ".join(validators_used),
            status=overall_status,
            execution_time=execution_time,
            memory_usage=total_memory,
            output="\n".join(combined_output) if combined_output else "",
            error="\n".join(combined_errors) if combined_errors else None,
            details=all_details
        )

    def generate_reports(self, report: ValidationReport, output_dir: Optional[Path] = None) -> Dict[str, str]:
        """生成所有配置的报告

        Args:
            report: 验证报告数据
            output_dir: 输出目录，默认为配置中的root_path

        Returns:
            Dict[str, str]: 报告格式到文件路径的映射
        """
        if output_dir is None:
            output_dir = self.config.root_path / "build" / "validation_reports"

        output_dir.mkdir(parents=True, exist_ok=True)
        generated_reports = {}
        timestamp = report.timestamp.strftime("%Y%m%d_%H%M%S")

        for reporter_name, reporter in self.reporters.items():
            try:
                report_format = reporter.get_report_format()
                report_content = reporter.generate_report(report)

                # 确定文件扩展名
                extension = self._get_report_extension(report_format)
                output_path = output_dir / f"validation_report_{timestamp}.{extension}"

                if reporter.save_report(report_content, str(output_path)):
                    generated_reports[report_format] = str(output_path)
                    logger.info(f"Generated {report_format} report: {output_path}")
                else:
                    logger.error(f"Failed to save {report_format} report")

            except Exception as e:
                logger.error(f"Failed to generate {reporter_name} report: {e}")

        return generated_reports

    def get_exit_code(self, report: ValidationReport) -> int:
        """获取CI/CD适用的退出代码

        Args:
            report: 验证报告

        Returns:
            int: 退出代码 (0=成功, 1=失败, 2=错误)
        """
        if report.summary.errors > 0:
            return 2  # 执行错误
        elif report.summary.failed > 0:
            return 1  # 验证失败
        else:
            return 0  # 成功

    def _initialize_validators(self) -> Dict[str, BaseValidator]:
        """初始化所有验证器

        Returns:
            Dict[str, BaseValidator]: 验证器名称到实例的映射
        """
        validators = {}

        try:
            # 导入验证器
            from ..validators import (
                FunctionalValidator,
                CompatibilityValidator,
                PerformanceValidator
            )
            from ..validators.documentation_validator import DocumentationValidator

            # 初始化功能验证器
            validators['functional'] = FunctionalValidator(
                platform_manager=self.platform_manager
            )

            # 初始化兼容性验证器
            validators['compatibility'] = CompatibilityValidator(
                platform_manager=self.platform_manager
            )

            # 初始化性能验证器
            validators['performance'] = PerformanceValidator(
                platform_manager=self.platform_manager,
                timeout_seconds=self.config.timeout_seconds,
                max_memory_mb=self.config.max_memory_mb
            )

            # 初始化文档验证器
            validators['documentation'] = DocumentationValidator(
                platform_manager=self.platform_manager
            )

            logger.info(f"Initialized {len(validators)} validators: {list(validators.keys())}")

        except ImportError as e:
            logger.warning(f"Some validators could not be imported: {e}")
        except Exception as e:
            logger.error(f"Error initializing validators: {e}")

        return validators

    def _initialize_reporters(self) -> Dict[str, ReportGenerator]:
        """初始化所有报告生成器

        Returns:
            Dict[str, ReportGenerator]: 报告器名称到实例的映射
        """
        reporters = {}

        try:
            from ..reporters import HTMLReporter, JSONReporter, SummaryReporter, JUnitReporter

            # 根据配置初始化报告器
            if self.config.generate_html_report:
                reporters['html'] = HTMLReporter()

            if self.config.generate_json_report:
                reporters['json'] = JSONReporter()

            if self.config.generate_summary_report:
                reporters['summary'] = SummaryReporter()

            if self.config.generate_junit_report:
                reporters['junit'] = JUnitReporter()

            logger.info(f"Initialized {len(reporters)} reporters: {list(reporters.keys())}")

        except ImportError as e:
            logger.warning(f"Some reporters could not be imported: {e}")
        except Exception as e:
            logger.error(f"Error initializing reporters: {e}")

        return reporters

    def _get_enabled_validators(self) -> Dict[str, BaseValidator]:
        """获取启用的验证器

        根据配置返回应该使用的验证器。

        Returns:
            Dict[str, BaseValidator]: 启用的验证器
        """
        # 如果配置中指定了验证器，只返回指定的
        if hasattr(self.config, 'enabled_validators') and self.config.enabled_validators:
            return {
                name: validator
                for name, validator in self.validators.items()
                if name in self.config.enabled_validators
            }

        # 否则返回所有验证器
        return self.validators

    def _is_script_compatible(self, script: Script, platform: Platform) -> bool:
        """检查脚本是否与平台兼容

        Args:
            script: 脚本对象
            platform: 目标平台

        Returns:
            bool: 是否兼容
        """
        if platform == Platform.WINDOWS:
            return script.type in [ScriptType.BATCH, ScriptType.POWERSHELL, ScriptType.PYTHON]
        elif platform in [Platform.WSL, Platform.LINUX]:
            return script.type in [ScriptType.SHELL, ScriptType.PYTHON]

        return False

    def _merge_status(self, current: ValidationStatus, new: ValidationStatus) -> ValidationStatus:
        """合并验证状态，取最严重的状态

        状态严重程度: ERROR > FAILED > SKIPPED > PASSED

        Args:
            current: 当前状态
            new: 新状态

        Returns:
            ValidationStatus: 合并后的状态
        """
        severity = {
            ValidationStatus.PASSED: 0,
            ValidationStatus.SKIPPED: 1,
            ValidationStatus.FAILED: 2,
            ValidationStatus.ERROR: 3
        }

        if severity.get(new, 0) > severity.get(current, 0):
            return new
        return current

    def _generate_summary(self, results: List[ValidationResult]) -> ValidationSummary:
        """生成验证摘要

        Args:
            results: 验证结果列表

        Returns:
            ValidationSummary: 验证摘要
        """
        total = len(results)
        passed = sum(1 for r in results if r.status == ValidationStatus.PASSED)
        failed = sum(1 for r in results if r.status == ValidationStatus.FAILED)
        skipped = sum(1 for r in results if r.status == ValidationStatus.SKIPPED)
        errors = sum(1 for r in results if r.status == ValidationStatus.ERROR)
        total_time = sum(r.execution_time for r in results)

        return ValidationSummary(
            total_scripts=total,
            passed=passed,
            failed=failed,
            skipped=skipped,
            errors=errors,
            execution_time=total_time
        )

    def _generate_recommendations(self, results: List[ValidationResult]) -> List[str]:
        """生成改进建议

        分析验证结果，生成可操作的建议。

        Args:
            results: 验证结果列表

        Returns:
            List[str]: 建议列表
        """
        recommendations = []

        # 分析失败和错误的结果
        failed_results = [r for r in results if r.status == ValidationStatus.FAILED]
        error_results = [r for r in results if r.status == ValidationStatus.ERROR]

        if failed_results:
            recommendations.append(
                f"发现 {len(failed_results)} 个失败的验证，建议检查脚本兼容性和功能实现"
            )

            # 分析失败原因
            platform_failures = {}
            for r in failed_results:
                platform_name = r.platform.value
                if platform_name not in platform_failures:
                    platform_failures[platform_name] = 0
                platform_failures[platform_name] += 1

            for platform, count in platform_failures.items():
                if count > 1:
                    recommendations.append(
                        f"平台 {platform} 有 {count} 个脚本验证失败，建议检查平台特定配置"
                    )

        if error_results:
            recommendations.append(
                f"发现 {len(error_results)} 个执行错误，建议检查脚本依赖和环境配置"
            )

            # 检查常见错误模式
            permission_errors = [r for r in error_results if r.error and 'permission' in r.error.lower()]
            if permission_errors:
                recommendations.append(
                    "部分脚本存在权限问题，建议检查文件权限设置"
                )

            dependency_errors = [r for r in error_results if r.error and 'dependency' in r.error.lower()]
            if dependency_errors:
                recommendations.append(
                    "部分脚本存在依赖问题，建议检查依赖安装情况"
                )

        # 检查跳过的脚本
        skipped_results = [r for r in results if r.status == ValidationStatus.SKIPPED]
        if skipped_results:
            recommendations.append(
                f"有 {len(skipped_results)} 个脚本被跳过，建议检查平台可用性和脚本兼容性配置"
            )

        # 性能建议
        slow_scripts = [r for r in results if r.execution_time > 60]  # 超过60秒
        if slow_scripts:
            recommendations.append(
                f"有 {len(slow_scripts)} 个脚本执行时间超过60秒，建议优化脚本性能"
            )

        if not recommendations:
            recommendations.append("所有验证通过，脚本状态良好！")

        return recommendations

    def _get_report_extension(self, report_format: str) -> str:
        """获取报告文件扩展名

        Args:
            report_format: 报告格式

        Returns:
            str: 文件扩展名
        """
        extensions = {
            'html': 'html',
            'json': 'json',
            'summary': 'txt',
            'xml': 'xml'
        }
        return extensions.get(report_format, 'txt')

    # 生命周期管理方法

    def add_validator(self, name: str, validator: BaseValidator) -> None:
        """添加验证器

        Args:
            name: 验证器名称
            validator: 验证器实例
        """
        self.validators[name] = validator
        logger.info(f"Added validator: {name}")

    def remove_validator(self, name: str) -> bool:
        """移除验证器

        Args:
            name: 验证器名称

        Returns:
            bool: 是否成功移除
        """
        if name in self.validators:
            del self.validators[name]
            logger.info(f"Removed validator: {name}")
            return True
        return False

    def add_reporter(self, name: str, reporter: ReportGenerator) -> None:
        """添加报告生成器

        Args:
            name: 报告器名称
            reporter: 报告器实例
        """
        self.reporters[name] = reporter
        logger.info(f"Added reporter: {name}")

    def remove_reporter(self, name: str) -> bool:
        """移除报告生成器

        Args:
            name: 报告器名称

        Returns:
            bool: 是否成功移除
        """
        if name in self.reporters:
            del self.reporters[name]
            logger.info(f"Removed reporter: {name}")
            return True
        return False

    def get_validator(self, name: str) -> Optional[BaseValidator]:
        """获取指定验证器

        Args:
            name: 验证器名称

        Returns:
            Optional[BaseValidator]: 验证器实例或None
        """
        return self.validators.get(name)

    def get_reporter(self, name: str) -> Optional[ReportGenerator]:
        """获取指定报告生成器

        Args:
            name: 报告器名称

        Returns:
            Optional[ReportGenerator]: 报告器实例或None
        """
        return self.reporters.get(name)

    def list_validators(self) -> List[str]:
        """列出所有验证器名称

        Returns:
            List[str]: 验证器名称列表
        """
        return list(self.validators.keys())

    def list_reporters(self) -> List[str]:
        """列出所有报告生成器名称

        Returns:
            List[str]: 报告器名称列表
        """
        return list(self.reporters.keys())
