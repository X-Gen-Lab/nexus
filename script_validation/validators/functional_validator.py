"""
功能验证器

验证脚本基本功能执行、检查脚本输出和退出代码、验证预期的工件生成。
"""

import os
import time
import psutil
from pathlib import Path
from typing import List, Dict, Any, Optional
from ..interfaces import BaseValidator
from ..models import (
    Script, Platform, ValidationResult, ValidationStatus,
    ScriptCategory, ExecutionResult
)


class FunctionalValidator(BaseValidator):
    """功能验证器 - 验证脚本基本功能执行"""

    def __init__(self, platform_manager=None):
        """初始化功能验证器"""
        self.platform_manager = platform_manager
        self.expected_artifacts = self._get_expected_artifacts()

    def validate(self, script: Script, platform: Platform) -> ValidationResult:
        """执行功能验证"""
        start_time = time.time()

        try:
            # 获取平台适配器
            if not self.platform_manager:
                raise ValueError("Platform manager not available")

            adapter = self.platform_manager.get_platform_adapter(platform)
            if not adapter:
                return ValidationResult(
                    script=script,
                    platform=platform,
                    validator=self.get_validator_name(),
                    status=ValidationStatus.ERROR,
                    execution_time=0.0,
                    memory_usage=0,
                    output="",
                    error="Platform adapter not available"
                )

            # 记录执行前状态
            initial_state = self._capture_initial_state(script)

            # 执行脚本
            execution_result = adapter.execute_script(script, [])

            # 验证基本功能
            validation_details = self._validate_basic_functionality(
                script, execution_result, initial_state
            )

            # 验证工件生成
            artifact_validation = self._validate_artifacts(script, initial_state)
            validation_details.update(artifact_validation)

            # 确定验证状态
            status = self._determine_validation_status(execution_result, validation_details)

            execution_time = time.time() - start_time

            return ValidationResult(
                script=script,
                platform=platform,
                validator=self.get_validator_name(),
                status=status,
                execution_time=execution_time,
                memory_usage=execution_result.memory_usage,
                output=execution_result.stdout,
                error=execution_result.stderr if execution_result.exit_code != 0 else None,
                details=validation_details
            )

        except Exception as e:
            execution_time = time.time() - start_time
            return ValidationResult(
                script=script,
                platform=platform,
                validator=self.get_validator_name(),
                status=ValidationStatus.ERROR,
                execution_time=execution_time,
                memory_usage=0,
                output="",
                error=str(e),
                details={"exception": str(e)}
            )

    def get_validator_name(self) -> str:
        """获取验证器名称"""
        return "FunctionalValidator"

    def _capture_initial_state(self, script: Script) -> Dict[str, Any]:
        """捕获脚本执行前的初始状态"""
        script_dir = script.path.parent

        initial_state = {
            "working_directory": str(script_dir),
            "existing_files": [],
            "existing_directories": []
        }

        try:
            # 记录现有文件和目录
            if script_dir.exists():
                for item in script_dir.rglob("*"):
                    if item.is_file():
                        initial_state["existing_files"].append(str(item))
                    elif item.is_dir():
                        initial_state["existing_directories"].append(str(item))
        except Exception as e:
            initial_state["capture_error"] = str(e)

        return initial_state

    def _validate_basic_functionality(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证脚本基本功能"""
        validation_details = {
            "exit_code_valid": execution_result.exit_code == 0,
            "has_output": bool(execution_result.stdout.strip()),
            "execution_completed": True,
            "execution_time_reasonable": execution_result.execution_time < 300.0,  # 5分钟超时
        }

        # 根据脚本类别进行特定验证
        category_validation = self._validate_by_category(script, execution_result, initial_state)
        validation_details.update(category_validation)

        return validation_details

    def _validate_by_category(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """根据脚本类别进行特定验证"""
        validation_details = {}

        if script.category == ScriptCategory.BUILD:
            validation_details.update(self._validate_build_script(script, execution_result, initial_state))
        elif script.category == ScriptCategory.TEST:
            validation_details.update(self._validate_test_script(script, execution_result, initial_state))
        elif script.category == ScriptCategory.FORMAT:
            validation_details.update(self._validate_format_script(script, execution_result, initial_state))
        elif script.category == ScriptCategory.CLEAN:
            validation_details.update(self._validate_clean_script(script, execution_result, initial_state))
        elif script.category == ScriptCategory.DOCS:
            validation_details.update(self._validate_docs_script(script, execution_result, initial_state))
        elif script.category == ScriptCategory.SETUP:
            validation_details.update(self._validate_setup_script(script, execution_result, initial_state))

        return validation_details

    def _validate_build_script(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证构建脚本"""
        validation_details = {
            "build_artifacts_created": False,
            "build_successful": execution_result.exit_code == 0
        }

        # 检查是否生成了构建工件
        script_dir = script.path.parent
        build_dirs = ["build", "_build", "dist", "target", "out"]

        for build_dir in build_dirs:
            build_path = script_dir / build_dir
            if build_path.exists() and any(build_path.iterdir()):
                validation_details["build_artifacts_created"] = True
                validation_details["build_directory"] = str(build_path)
                break

        return validation_details

    def _validate_test_script(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证测试脚本"""
        validation_details = {
            "tests_executed": False,
            "test_results_available": False
        }

        # 检查输出中是否包含测试结果指示器
        output = execution_result.stdout.lower()
        test_indicators = ["test", "passed", "failed", "ok", "error", "assertion"]

        validation_details["tests_executed"] = any(indicator in output for indicator in test_indicators)

        # 检查是否生成了测试报告文件
        script_dir = script.path.parent
        report_patterns = ["test_results", "test-results", "coverage", "junit"]

        for pattern in report_patterns:
            for file_path in script_dir.rglob(f"*{pattern}*"):
                if file_path.is_file():
                    validation_details["test_results_available"] = True
                    validation_details["test_report_file"] = str(file_path)
                    break

        return validation_details

    def _validate_format_script(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证格式化脚本"""
        validation_details = {
            "formatting_completed": execution_result.exit_code == 0,
            "no_source_corruption": True
        }

        # 检查源文件是否仍然存在且可读
        script_dir = script.path.parent
        source_extensions = [".c", ".cpp", ".h", ".hpp", ".py", ".js", ".ts"]

        try:
            for ext in source_extensions:
                for source_file in script_dir.rglob(f"*{ext}"):
                    if source_file.is_file():
                        # 尝试读取文件以确保没有损坏
                        source_file.read_text(encoding='utf-8', errors='ignore')
        except Exception as e:
            validation_details["no_source_corruption"] = False
            validation_details["corruption_error"] = str(e)

        return validation_details

    def _validate_clean_script(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证清理脚本"""
        validation_details = {
            "cleanup_successful": execution_result.exit_code == 0,
            "source_files_preserved": True,
            "build_artifacts_removed": False
        }

        script_dir = script.path.parent

        # 检查源文件是否被保留
        source_extensions = [".c", ".cpp", ".h", ".hpp", ".py", ".js", ".ts", ".md"]
        source_files_exist = False

        for ext in source_extensions:
            if any(script_dir.rglob(f"*{ext}")):
                source_files_exist = True
                break

        validation_details["source_files_preserved"] = source_files_exist

        # 检查构建工件是否被清理
        build_dirs = ["build", "_build", "dist", "target", "out", "__pycache__"]
        artifacts_removed = True

        for build_dir in build_dirs:
            build_path = script_dir / build_dir
            if build_path.exists() and any(build_path.iterdir()):
                artifacts_removed = False
                break

        validation_details["build_artifacts_removed"] = artifacts_removed

        return validation_details

    def _validate_docs_script(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证文档脚本"""
        validation_details = {
            "docs_generated": False,
            "docs_accessible": False
        }

        # 检查是否生成了文档文件
        script_dir = script.path.parent
        doc_dirs = ["docs", "doc", "documentation", "api"]
        doc_extensions = [".html", ".pdf", ".md", ".rst"]

        for doc_dir in doc_dirs:
            doc_path = script_dir / doc_dir
            if doc_path.exists():
                for ext in doc_extensions:
                    if any(doc_path.rglob(f"*{ext}")):
                        validation_details["docs_generated"] = True
                        validation_details["docs_directory"] = str(doc_path)

                        # 检查文档是否可访问（至少有一个主页面）
                        main_files = ["index.html", "index.md", "README.md"]
                        for main_file in main_files:
                            if (doc_path / main_file).exists():
                                validation_details["docs_accessible"] = True
                                break
                        break

        return validation_details

    def _validate_setup_script(
        self,
        script: Script,
        execution_result: ExecutionResult,
        initial_state: Dict[str, Any]
    ) -> Dict[str, Any]:
        """验证设置脚本"""
        validation_details = {
            "setup_completed": execution_result.exit_code == 0,
            "dependencies_installed": False,
            "environment_configured": False
        }

        # 检查输出中是否包含安装成功的指示器
        output = execution_result.stdout.lower()
        install_indicators = [
            "successfully installed", "installation complete", "setup complete",
            "installed", "configured", "ready"
        ]

        validation_details["dependencies_installed"] = any(
            indicator in output for indicator in install_indicators
        )

        # 检查是否创建了配置文件
        script_dir = script.path.parent
        config_files = [".env", "config.json", "settings.ini", "requirements.txt"]

        for config_file in config_files:
            if (script_dir / config_file).exists():
                validation_details["environment_configured"] = True
                break

        return validation_details

    def _validate_artifacts(self, script: Script, initial_state: Dict[str, Any]) -> Dict[str, Any]:
        """验证预期的工件生成"""
        validation_details = {
            "expected_artifacts_present": False,
            "artifacts_list": []
        }

        expected_artifacts = self.expected_artifacts.get(script.category, [])
        script_dir = script.path.parent

        found_artifacts = []
        for artifact_pattern in expected_artifacts:
            for artifact_path in script_dir.rglob(artifact_pattern):
                if artifact_path.is_file():
                    found_artifacts.append(str(artifact_path))

        validation_details["artifacts_list"] = found_artifacts
        validation_details["expected_artifacts_present"] = len(found_artifacts) > 0

        return validation_details

    def _determine_validation_status(
        self,
        execution_result: ExecutionResult,
        validation_details: Dict[str, Any]
    ) -> ValidationStatus:
        """确定验证状态"""
        # 如果脚本执行失败，直接返回失败
        if execution_result.exit_code != 0:
            return ValidationStatus.FAILED

        # 检查关键验证项
        critical_checks = [
            validation_details.get("exit_code_valid", False),
            validation_details.get("execution_completed", False)
        ]

        if not all(critical_checks):
            return ValidationStatus.FAILED

        # 检查类别特定的验证项
        category_specific_passed = True

        # 构建脚本需要生成工件
        if validation_details.get("build_artifacts_created") is False and "build_successful" in validation_details:
            category_specific_passed = False

        # 测试脚本需要执行测试
        if validation_details.get("tests_executed") is False and "test_results_available" in validation_details:
            category_specific_passed = False

        # 格式化脚本不能损坏源文件
        if validation_details.get("no_source_corruption") is False:
            category_specific_passed = False

        # 清理脚本需要保留源文件
        if validation_details.get("source_files_preserved") is False:
            category_specific_passed = False

        return ValidationStatus.PASSED if category_specific_passed else ValidationStatus.FAILED

    def _get_expected_artifacts(self) -> Dict[ScriptCategory, List[str]]:
        """获取各类脚本的预期工件模式"""
        return {
            ScriptCategory.BUILD: [
                "build/*", "_build/*", "dist/*", "target/*", "out/*",
                "*.exe", "*.dll", "*.so", "*.a", "*.lib"
            ],
            ScriptCategory.TEST: [
                "*test_results*", "*test-results*", "*coverage*", "*junit*",
                "*.xml", "*.json", "*.html"
            ],
            ScriptCategory.DOCS: [
                "docs/*", "doc/*", "documentation/*", "api/*",
                "*.html", "*.pdf"
            ],
            ScriptCategory.SETUP: [
                ".env", "config.json", "settings.ini", "requirements.txt",
                "node_modules/*", "venv/*", ".venv/*"
            ]
        }
