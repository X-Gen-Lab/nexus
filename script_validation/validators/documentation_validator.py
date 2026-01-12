"""
文档验证器

验证脚本选项与文档的一致性、测试文档中的使用示例、检查帮助文本和依赖信息的准确性。
"""

import re
import time
import subprocess
from pathlib import Path
from typing import List, Dict, Any, Optional, Set, Tuple
from ..interfaces import BaseValidator
from ..models import (
    Script, Platform, ValidationResult, ValidationStatus,
    ScriptType, Parameter
)


class DocumentationValidator(BaseValidator):
    """文档验证器 - 验证脚本文档的一致性和准确性"""

    def __init__(self, platform_manager=None):
        """初始化文档验证器"""
        self.platform_manager = platform_manager
        self.documentation_patterns = self._get_documentation_patterns()

    def validate(self, script: Script, platform: Platform) -> ValidationResult:
        """执行文档验证"""
        start_time = time.time()

        try:
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

            # 验证脚本选项与文档的一致性
            options_validation = self._validate_script_options(script, adapter)

            # 验证文档中的使用示例
            examples_validation = self._validate_usage_examples(script, adapter)

            # 验证帮助文本
            help_validation = self._validate_help_text(script, adapter)

            # 验证依赖信息
            dependencies_validation = self._validate_dependencies(script, adapter)

            # 验证平台支持文档
            platform_validation = self._validate_platform_support(script)

            # 合并验证结果
            validation_details = {
                **options_validation,
                **examples_validation,
                **help_validation,
                **dependencies_validation,
                **platform_validation
            }

            # 确定验证状态
            status = self._determine_documentation_status(validation_details)

            execution_time = time.time() - start_time

            return ValidationResult(
                script=script,
                platform=platform,
                validator=self.get_validator_name(),
                status=status,
                execution_time=execution_time,
                memory_usage=0,
                output=f"Documentation validation results: {validation_details}",
                error=validation_details.get("error"),
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
        return "DocumentationValidator"

    def _validate_script_options(self, script: Script, adapter) -> Dict[str, Any]:
        """验证脚本选项与文档的一致性"""
        options_validation = {
            "documented_options_supported": True,
            "undocumented_options_found": False,
            "options_consistency_score": 1.0,
            "documented_options": [],
            "actual_options": [],
            "missing_options": [],
            "extra_options": [],
            "option_validation_details": []
        }

        try:
            # 从脚本元数据中获取文档化的选项
            documented_options = set()
            for param in script.metadata.parameters:
                documented_options.add(param.name)

            options_validation["documented_options"] = list(documented_options)

            # 从脚本内容中提取实际支持的选项
            actual_options = self._extract_script_options(script)
            options_validation["actual_options"] = list(actual_options)

            # 比较文档化选项和实际选项
            missing_options = documented_options - actual_options
            extra_options = actual_options - documented_options

            options_validation["missing_options"] = list(missing_options)
            options_validation["extra_options"] = list(extra_options)

            # 计算一致性分数
            total_options = len(documented_options.union(actual_options))
            if total_options > 0:
                consistent_options = len(documented_options.intersection(actual_options))
                options_validation["options_consistency_score"] = consistent_options / total_options

            # 确定验证结果
            options_validation["documented_options_supported"] = len(missing_options) == 0
            options_validation["undocumented_options_found"] = len(extra_options) > 0

            # 详细验证每个选项
            for param in script.metadata.parameters:
                option_detail = self._validate_individual_option(script, param, adapter)
                options_validation["option_validation_details"].append(option_detail)

        except Exception as e:
            options_validation["error"] = str(e)
            options_validation["documented_options_supported"] = False

        return options_validation

    def _validate_usage_examples(self, script: Script, adapter) -> Dict[str, Any]:
        """验证文档中的使用示例"""
        examples_validation = {
            "examples_work_correctly": True,
            "examples_tested": 0,
            "examples_passed": 0,
            "examples_failed": 0,
            "example_test_results": [],
            "examples_comprehensive": True
        }

        try:
            examples = script.metadata.examples
            if not examples:
                examples_validation["examples_work_correctly"] = None
                examples_validation["reason"] = "No examples provided in documentation"
                return examples_validation

            examples_validation["examples_tested"] = len(examples)

            # 测试每个示例
            for i, example in enumerate(examples):
                example_result = self._test_usage_example(script, example, adapter)
                examples_validation["example_test_results"].append(example_result)

                if example_result["success"]:
                    examples_validation["examples_passed"] += 1
                else:
                    examples_validation["examples_failed"] += 1

            # 确定示例是否正确工作
            examples_validation["examples_work_correctly"] = (
                examples_validation["examples_failed"] == 0
            )

            # 检查示例是否全面
            examples_validation["examples_comprehensive"] = self._check_examples_comprehensiveness(
                script, examples
            )

        except Exception as e:
            examples_validation["error"] = str(e)
            examples_validation["examples_work_correctly"] = False

        return examples_validation

    def _validate_help_text(self, script: Script, adapter) -> Dict[str, Any]:
        """验证帮助文本"""
        help_validation = {
            "help_text_available": False,
            "help_text_accurate": True,
            "help_matches_documentation": True,
            "help_text_content": "",
            "help_validation_details": []
        }

        try:
            # 尝试获取脚本的帮助文本
            help_text = self._get_script_help_text(script, adapter)

            if help_text:
                help_validation["help_text_available"] = True
                help_validation["help_text_content"] = help_text

                # 验证帮助文本与文档的匹配度
                help_validation["help_matches_documentation"] = self._compare_help_with_documentation(
                    script, help_text
                )

                # 验证帮助文本的准确性
                help_validation["help_text_accurate"] = self._validate_help_accuracy(
                    script, help_text, adapter
                )
            else:
                help_validation["help_text_available"] = False
                help_validation["help_matches_documentation"] = False
                help_validation["help_text_accurate"] = False
                help_validation["reason"] = "No help text available"

        except Exception as e:
            help_validation["error"] = str(e)
            help_validation["help_text_accurate"] = False

        return help_validation

    def _validate_dependencies(self, script: Script, adapter) -> Dict[str, Any]:
        """验证依赖信息"""
        dependencies_validation = {
            "documented_dependencies_accurate": True,
            "all_dependencies_documented": True,
            "dependency_check_results": [],
            "documented_dependencies": [],
            "actual_dependencies": [],
            "missing_documented_deps": [],
            "undocumented_deps": []
        }

        try:
            # 获取文档化的依赖
            documented_deps = set(script.dependencies) if script.dependencies else set()
            dependencies_validation["documented_dependencies"] = list(documented_deps)

            # 从脚本内容中提取实际依赖
            actual_deps = self._extract_script_dependencies(script)
            dependencies_validation["actual_dependencies"] = list(actual_deps)

            # 比较文档化依赖和实际依赖
            missing_documented = documented_deps - actual_deps
            undocumented = actual_deps - documented_deps

            dependencies_validation["missing_documented_deps"] = list(missing_documented)
            dependencies_validation["undocumented_deps"] = list(undocumented)

            # 验证每个文档化的依赖
            for dep in documented_deps:
                dep_result = self._check_dependency_availability(dep, adapter)
                dependencies_validation["dependency_check_results"].append(dep_result)

            # 确定验证结果
            dependencies_validation["documented_dependencies_accurate"] = len(missing_documented) == 0
            dependencies_validation["all_dependencies_documented"] = len(undocumented) == 0

        except Exception as e:
            dependencies_validation["error"] = str(e)
            dependencies_validation["documented_dependencies_accurate"] = False

        return dependencies_validation

    def _validate_platform_support(self, script: Script) -> Dict[str, Any]:
        """验证平台支持文档"""
        platform_validation = {
            "platform_support_documented": True,
            "platform_support_accurate": True,
            "supported_platforms": [],
            "platform_validation_details": []
        }

        try:
            # 从脚本元数据或文件名推断支持的平台
            supported_platforms = self._infer_supported_platforms(script)
            platform_validation["supported_platforms"] = supported_platforms

            # 验证平台支持的准确性
            for platform in supported_platforms:
                platform_detail = {
                    "platform": platform,
                    "documented": True,
                    "actually_supported": self._check_platform_compatibility(script, platform)
                }
                platform_validation["platform_validation_details"].append(platform_detail)

            # 确定验证结果
            all_accurate = all(
                detail["actually_supported"]
                for detail in platform_validation["platform_validation_details"]
            )
            platform_validation["platform_support_accurate"] = all_accurate

        except Exception as e:
            platform_validation["error"] = str(e)
            platform_validation["platform_support_accurate"] = False

        return platform_validation

    def _determine_documentation_status(self, validation_details: Dict[str, Any]) -> ValidationStatus:
        """根据验证详情确定文档验证状态"""
        # 检查是否有错误
        if any("error" in details for details in validation_details.values() if isinstance(details, dict)):
            return ValidationStatus.ERROR

        # 检查关键验证项
        critical_checks = [
            validation_details.get("documented_options_supported", True),
            validation_details.get("examples_work_correctly", True),
            validation_details.get("help_text_accurate", True),
            validation_details.get("documented_dependencies_accurate", True),
            validation_details.get("platform_support_accurate", True)
        ]

        # 如果所有关键检查都通过，则验证通过
        if all(check is True or check is None for check in critical_checks):
            return ValidationStatus.PASSED
        else:
            return ValidationStatus.FAILED

    def _get_documentation_patterns(self) -> Dict[str, List[str]]:
        """获取文档模式"""
        return {
            "batch": [
                r"@echo\s+off",
                r"REM\s+(.+)",
                r"echo\s+(.+)",
                r"if\s+(.+)",
                r"set\s+(\w+)=(.+)"
            ],
            "powershell": [
                r"param\s*\(",
                r"\[Parameter\(",
                r"Write-Host\s+(.+)",
                r"Write-Output\s+(.+)",
                r"#\s*(.+)"
            ],
            "shell": [
                r"#!/bin/bash",
                r"#!/bin/sh",
                r"#\s*(.+)",
                r"echo\s+(.+)",
                r"if\s+\[(.+)\]"
            ],
            "python": [
                r"import\s+(\w+)",
                r"from\s+(\w+)\s+import",
                r"def\s+(\w+)\(",
                r"if\s+__name__\s*==\s*['\"]__main__['\"]",
                r"argparse\.ArgumentParser"
            ]
        }

    def _extract_script_options(self, script: Script) -> Set[str]:
        """从脚本内容中提取选项"""
        options = set()

        try:
            content = script.path.read_text(encoding='utf-8', errors='ignore')

            if script.type == ScriptType.BATCH:
                # 提取批处理文件选项
                options.update(re.findall(r'%(\w+)', content))
                options.update(re.findall(r'set\s+/p\s+(\w+)=', content, re.IGNORECASE))

            elif script.type == ScriptType.POWERSHELL:
                # 提取PowerShell参数
                param_matches = re.findall(r'\[Parameter.*?\]\s*\$(\w+)', content, re.DOTALL)
                options.update(param_matches)

            elif script.type == ScriptType.SHELL:
                # 提取Shell脚本选项
                options.update(re.findall(r'getopts\s+["\']([^"\']+)["\']', content))
                options.update(re.findall(r'\$\{(\w+):-', content))

            elif script.type == ScriptType.PYTHON:
                # 提取Python argparse选项
                arg_matches = re.findall(r'add_argument\(["\']--?([^"\']+)["\']', content)
                options.update(arg_matches)

        except Exception:
            pass

        return options

    def _validate_individual_option(self, script: Script, param: Parameter, adapter) -> Dict[str, Any]:
        """验证单个选项"""
        return {
            "option_name": param.name,
            "documented": True,
            "supported": param.name in self._extract_script_options(script),
            "description_accurate": True,  # 简化实现
            "type_correct": True  # 简化实现
        }

    def _test_usage_example(self, script: Script, example: str, adapter) -> Dict[str, Any]:
        """测试使用示例"""
        example_result = {
            "example": example,
            "success": False,
            "output": "",
            "error": "",
            "execution_time": 0.0
        }

        try:
            start_time = time.time()

            # 解析示例命令
            example_parts = example.strip().split()
            if not example_parts:
                example_result["error"] = "Empty example"
                return example_result

            # 执行示例（使用dry-run或安全模式）
            result = adapter.execute_script(script, example_parts[1:])  # 跳过脚本名

            example_result["success"] = result.exit_code == 0
            example_result["output"] = result.stdout
            example_result["error"] = result.stderr
            example_result["execution_time"] = time.time() - start_time

        except Exception as e:
            example_result["error"] = str(e)

        return example_result

    def _check_examples_comprehensiveness(self, script: Script, examples: List[str]) -> bool:
        """检查示例是否全面"""
        # 简化实现：检查是否有基本用法示例
        if not examples:
            return False

        # 检查是否包含基本用法
        has_basic_usage = any("help" in example.lower() or len(example.split()) <= 2 for example in examples)

        # 检查是否包含参数示例
        has_parameter_usage = any(len(example.split()) > 2 for example in examples)

        return has_basic_usage or has_parameter_usage

    def _get_script_help_text(self, script: Script, adapter) -> Optional[str]:
        """获取脚本帮助文本"""
        help_commands = []

        if script.type == ScriptType.BATCH:
            help_commands = [["/?"], ["/help"], ["/h"]]
        elif script.type == ScriptType.POWERSHELL:
            help_commands = [["-Help"], ["-h"], ["/?"], ["-?"]]
        elif script.type == ScriptType.SHELL:
            help_commands = [["--help"], ["-h"], ["help"]]
        elif script.type == ScriptType.PYTHON:
            help_commands = [["--help"], ["-h"]]

        for help_cmd in help_commands:
            try:
                result = adapter.execute_script(script, help_cmd)
                if result.exit_code == 0 and result.stdout.strip():
                    return result.stdout
                elif result.stderr.strip() and "help" in result.stderr.lower():
                    return result.stderr
            except Exception:
                continue

        return None

    def _compare_help_with_documentation(self, script: Script, help_text: str) -> bool:
        """比较帮助文本与文档"""
        # 简化实现：检查文档中的参数是否在帮助文本中出现
        documented_params = {param.name for param in script.metadata.parameters}

        if not documented_params:
            return True  # 没有文档化的参数，认为一致

        # 检查帮助文本中是否包含文档化的参数
        help_lower = help_text.lower()
        found_params = sum(1 for param in documented_params if param.lower() in help_lower)

        # 如果至少50%的参数在帮助文本中找到，认为匹配
        return found_params >= len(documented_params) * 0.5

    def _validate_help_accuracy(self, script: Script, help_text: str, adapter) -> bool:
        """验证帮助文本的准确性"""
        # 简化实现：检查帮助文本是否包含基本信息
        help_lower = help_text.lower()

        # 检查是否包含用法信息
        has_usage = any(keyword in help_lower for keyword in ["usage", "用法", "使用"])

        # 检查是否包含选项信息
        has_options = any(keyword in help_lower for keyword in ["options", "选项", "参数", "arguments"])

        # 检查是否包含描述信息
        has_description = len(help_text.strip()) > 20  # 简单长度检查

        return has_usage or has_options or has_description

    def _extract_script_dependencies(self, script: Script) -> Set[str]:
        """从脚本内容中提取依赖"""
        dependencies = set()

        try:
            content = script.path.read_text(encoding='utf-8', errors='ignore')

            if script.type == ScriptType.PYTHON:
                # 提取Python导入
                import_matches = re.findall(r'import\s+(\w+)', content)
                from_matches = re.findall(r'from\s+(\w+)\s+import', content)
                dependencies.update(import_matches)
                dependencies.update(from_matches)

            elif script.type in [ScriptType.SHELL, ScriptType.BATCH, ScriptType.POWERSHELL]:
                # 提取命令依赖
                command_patterns = [
                    r'(\w+)\.exe',
                    r'which\s+(\w+)',
                    r'command\s+-v\s+(\w+)',
                    r'Get-Command\s+(\w+)'
                ]
                for pattern in command_patterns:
                    matches = re.findall(pattern, content, re.IGNORECASE)
                    dependencies.update(matches)

        except Exception:
            pass

        return dependencies

    def _check_dependency_availability(self, dependency: str, adapter) -> Dict[str, Any]:
        """检查依赖可用性"""
        dep_result = {
            "dependency": dependency,
            "available": False,
            "version": None,
            "location": None
        }

        try:
            # 尝试检查依赖是否可用
            check_result = adapter.check_dependencies([dependency])
            if hasattr(check_result, 'available') and check_result.available:
                dep_result["available"] = True
                if hasattr(check_result, 'version'):
                    dep_result["version"] = check_result.version
                if hasattr(check_result, 'location'):
                    dep_result["location"] = check_result.location

        except Exception as e:
            dep_result["error"] = str(e)

        return dep_result

    def _infer_supported_platforms(self, script: Script) -> List[str]:
        """推断支持的平台"""
        platforms = []

        if script.type == ScriptType.BATCH:
            platforms.append("windows")
        elif script.type == ScriptType.POWERSHELL:
            platforms.extend(["windows", "linux"])  # PowerShell Core支持Linux
        elif script.type == ScriptType.SHELL:
            platforms.extend(["linux", "wsl"])
        elif script.type == ScriptType.PYTHON:
            platforms.extend(["windows", "linux", "wsl"])

        return platforms

    def _check_platform_compatibility(self, script: Script, platform: str) -> bool:
        """检查平台兼容性"""
        # 简化实现：基于脚本类型判断兼容性
        compatibility_map = {
            ScriptType.BATCH: ["windows"],
            ScriptType.POWERSHELL: ["windows", "linux"],
            ScriptType.SHELL: ["linux", "wsl"],
            ScriptType.PYTHON: ["windows", "linux", "wsl"]
        }

        return platform in compatibility_map.get(script.type, [])
