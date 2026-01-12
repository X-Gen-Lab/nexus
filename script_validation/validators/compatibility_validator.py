"""
兼容性验证器

验证跨平台脚本一致性和比较平台特定脚本的等效性。
"""

import time
import hashlib
import json
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple
from ..interfaces import BaseValidator
from ..models import (
    Script, Platform, ValidationResult, ValidationStatus,
    ScriptType, ExecutionResult
)


class CompatibilityValidator(BaseValidator):
    """兼容性验证器 - 验证跨平台脚本一致性"""

    def __init__(self, platform_manager=None):
        """初始化兼容性验证器"""
        self.platform_manager = platform_manager
        self.cross_platform_results = {}  # 存储跨平台执行结果
        self.equivalent_scripts = {}  # 存储等效脚本映射

    def validate(self, script: Script, platform: Platform) -> ValidationResult:
        """执行兼容性验证"""
        start_time = time.time()

        try:
            if not self.platform_manager:
                raise ValueError("Platform manager not available")

            # 执行跨平台一致性验证
            consistency_result = self._validate_cross_platform_consistency(script, platform)

            # 执行平台特定脚本等效性验证
            equivalence_result = self._validate_platform_equivalence(script, platform)

            # 合并验证结果
            validation_details = {
                **consistency_result,
                **equivalence_result
            }

            # 确定验证状态
            status = self._determine_compatibility_status(validation_details)

            execution_time = time.time() - start_time

            return ValidationResult(
                script=script,
                platform=platform,
                validator=self.get_validator_name(),
                status=status,
                execution_time=execution_time,
                memory_usage=0,  # 兼容性验证不直接执行脚本
                output=json.dumps(validation_details, indent=2),
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
        return "CompatibilityValidator"

    def _validate_cross_platform_consistency(
        self,
        script: Script,
        platform: Platform
    ) -> Dict[str, Any]:
        """验证跨平台脚本一致性"""
        validation_details = {
            "cross_platform_consistent": True,
            "tested_platforms": [],
            "consistency_issues": [],
            "output_comparison": {}
        }

        # 只对Python脚本进行跨平台一致性验证
        if script.type != ScriptType.PYTHON:
            validation_details["cross_platform_consistent"] = None
            validation_details["reason"] = "Not a cross-platform script type"
            return validation_details

        try:
            # 获取所有可用平台
            available_platforms = self._get_available_platforms()

            if len(available_platforms) < 2:
                validation_details["cross_platform_consistent"] = None
                validation_details["reason"] = "Insufficient platforms for comparison"
                return validation_details

            # 在所有可用平台上执行脚本
            platform_results = {}
            for test_platform in available_platforms:
                try:
                    adapter = self.platform_manager.get_platform_adapter(test_platform)
                    if adapter and adapter.is_available():
                        result = adapter.execute_script(script, [])
                        platform_results[test_platform] = result
                        validation_details["tested_platforms"].append(test_platform.value)
                except Exception as e:
                    validation_details["consistency_issues"].append(
                        f"Failed to execute on {test_platform.value}: {str(e)}"
                    )

            # 比较执行结果
            if len(platform_results) >= 2:
                consistency_analysis = self._analyze_cross_platform_consistency(platform_results)
                validation_details.update(consistency_analysis)
            else:
                validation_details["cross_platform_consistent"] = False
                validation_details["reason"] = "Could not execute on multiple platforms"

        except Exception as e:
            validation_details["cross_platform_consistent"] = False
            validation_details["error"] = str(e)

        return validation_details

    def _validate_platform_equivalence(
        self,
        script: Script,
        platform: Platform
    ) -> Dict[str, Any]:
        """验证平台特定脚本等效性"""
        validation_details = {
            "platform_equivalence_verified": True,
            "equivalent_scripts": [],
            "equivalence_issues": [],
            "functional_comparison": {}
        }

        try:
            # 查找等效脚本
            equivalent_scripts = self._find_equivalent_scripts(script)

            if not equivalent_scripts:
                validation_details["platform_equivalence_verified"] = None
                validation_details["reason"] = "No equivalent scripts found"
                return validation_details

            validation_details["equivalent_scripts"] = [
                str(eq_script.path) for eq_script in equivalent_scripts
            ]

            # 比较等效脚本的功能
            equivalence_analysis = self._analyze_script_equivalence(script, equivalent_scripts)
            validation_details.update(equivalence_analysis)

        except Exception as e:
            validation_details["platform_equivalence_verified"] = False
            validation_details["error"] = str(e)

        return validation_details

    def _get_available_platforms(self) -> List[Platform]:
        """获取所有可用平台"""
        available_platforms = []

        for platform in Platform:
            try:
                adapter = self.platform_manager.get_platform_adapter(platform)
                if adapter and adapter.is_available():
                    available_platforms.append(platform)
            except Exception:
                continue

        return available_platforms

    def _analyze_cross_platform_consistency(
        self,
        platform_results: Dict[Platform, ExecutionResult]
    ) -> Dict[str, Any]:
        """分析跨平台一致性"""
        analysis = {
            "cross_platform_consistent": True,
            "output_comparison": {},
            "exit_code_comparison": {},
            "consistency_score": 0.0
        }

        # 提取所有平台的结果
        platforms = list(platform_results.keys())
        results = list(platform_results.values())

        # 比较退出代码
        exit_codes = [result.exit_code for result in results]
        analysis["exit_code_comparison"] = {
            platform.value: result.exit_code
            for platform, result in platform_results.items()
        }

        exit_codes_consistent = len(set(exit_codes)) == 1

        # 比较输出内容（标准化后）
        normalized_outputs = []
        for platform, result in platform_results.items():
            normalized_output = self._normalize_output(result.stdout)
            normalized_outputs.append(normalized_output)
            analysis["output_comparison"][platform.value] = {
                "raw_output": result.stdout[:500],  # 限制长度
                "normalized_output": normalized_output[:500],
                "output_hash": hashlib.md5(normalized_output.encode()).hexdigest()
            }

        # 计算输出一致性
        output_hashes = [
            hashlib.md5(output.encode()).hexdigest()
            for output in normalized_outputs
        ]
        outputs_consistent = len(set(output_hashes)) == 1

        # 计算一致性分数
        consistency_factors = [exit_codes_consistent, outputs_consistent]
        analysis["consistency_score"] = sum(consistency_factors) / len(consistency_factors)

        # 确定整体一致性
        analysis["cross_platform_consistent"] = analysis["consistency_score"] >= 0.8

        if not analysis["cross_platform_consistent"]:
            issues = []
            if not exit_codes_consistent:
                issues.append("Exit codes differ across platforms")
            if not outputs_consistent:
                issues.append("Output content differs across platforms")
            analysis["consistency_issues"] = issues

        return analysis

    def _normalize_output(self, output: str) -> str:
        """标准化输出内容以便比较"""
        if not output:
            return ""

        # 移除平台特定的路径分隔符差异
        normalized = output.replace('\\', '/')

        # 移除行结束符差异
        normalized = normalized.replace('\r\n', '\n').replace('\r', '\n')

        # 移除多余的空白字符
        lines = [line.strip() for line in normalized.split('\n')]
        normalized = '\n'.join(line for line in lines if line)

        # 移除时间戳和临时文件路径等变化内容
        import re

        # 移除时间戳模式
        timestamp_patterns = [
            r'\d{4}-\d{2}-\d{2}[\s\T]\d{2}:\d{2}:\d{2}',
            r'\d{2}:\d{2}:\d{2}',
            r'\d+\.\d+s',
            r'\d+ms'
        ]

        for pattern in timestamp_patterns:
            normalized = re.sub(pattern, '[TIMESTAMP]', normalized)

        # 移除临时路径
        temp_patterns = [
            r'/tmp/[^\s]+',
            r'C:\\Users\\[^\\]+\\AppData\\Local\\Temp\\[^\s]+',
            r'/var/folders/[^\s]+'
        ]

        for pattern in temp_patterns:
            normalized = re.sub(pattern, '[TEMP_PATH]', normalized)

        return normalized

    def _find_equivalent_scripts(self, script: Script) -> List[Script]:
        """查找等效脚本"""
        equivalent_scripts = []
        script_dir = script.path.parent
        script_name_base = script.path.stem  # 不包含扩展名的文件名

        # 定义等效脚本的扩展名映射
        equivalent_extensions = {
            ScriptType.BATCH: ['.ps1', '.sh', '.py'],
            ScriptType.POWERSHELL: ['.bat', '.sh', '.py'],
            ScriptType.SHELL: ['.bat', '.ps1', '.py'],
            ScriptType.PYTHON: ['.bat', '.ps1', '.sh']
        }

        # 查找同名但不同扩展名的脚本
        target_extensions = equivalent_extensions.get(script.type, [])

        for ext in target_extensions:
            equivalent_path = script_dir / f"{script_name_base}{ext}"
            if equivalent_path.exists():
                # 创建等效脚本对象（简化版）
                equivalent_script = Script(
                    path=equivalent_path,
                    name=equivalent_path.name,
                    type=self._get_script_type_from_extension(ext),
                    platform=self._get_platform_from_extension(ext),
                    metadata=script.metadata,  # 复用元数据
                    dependencies=script.dependencies,
                    category=script.category
                )
                equivalent_scripts.append(equivalent_script)

        return equivalent_scripts

    def _get_script_type_from_extension(self, extension: str) -> ScriptType:
        """根据扩展名获取脚本类型"""
        extension_map = {
            '.bat': ScriptType.BATCH,
            '.ps1': ScriptType.POWERSHELL,
            '.sh': ScriptType.SHELL,
            '.py': ScriptType.PYTHON
        }
        return extension_map.get(extension, ScriptType.SHELL)

    def _get_platform_from_extension(self, extension: str) -> Platform:
        """根据扩展名获取主要平台"""
        platform_map = {
            '.bat': Platform.WINDOWS,
            '.ps1': Platform.WINDOWS,
            '.sh': Platform.LINUX,
            '.py': Platform.LINUX  # Python可以跨平台，但默认归类为Linux
        }
        return platform_map.get(extension, Platform.LINUX)

    def _analyze_script_equivalence(
        self,
        script: Script,
        equivalent_scripts: List[Script]
    ) -> Dict[str, Any]:
        """分析脚本等效性"""
        analysis = {
            "platform_equivalence_verified": True,
            "functional_comparison": {},
            "equivalence_score": 0.0,
            "equivalence_issues": []
        }

        try:
            # 执行原始脚本
            original_platform = script.platform
            original_adapter = self.platform_manager.get_platform_adapter(original_platform)

            if not original_adapter or not original_adapter.is_available():
                analysis["platform_equivalence_verified"] = False
                analysis["equivalence_issues"].append(f"Cannot execute original script on {original_platform.value}")
                return analysis

            original_result = original_adapter.execute_script(script, [])

            # 执行等效脚本并比较结果
            equivalent_results = {}
            successful_comparisons = 0
            total_comparisons = 0

            for eq_script in equivalent_scripts:
                try:
                    eq_platform = eq_script.platform
                    eq_adapter = self.platform_manager.get_platform_adapter(eq_platform)

                    if eq_adapter and eq_adapter.is_available():
                        eq_result = eq_adapter.execute_script(eq_script, [])
                        equivalent_results[eq_script.path.name] = eq_result

                        # 比较功能等效性
                        comparison = self._compare_script_results(original_result, eq_result)
                        analysis["functional_comparison"][eq_script.path.name] = comparison

                        if comparison["functionally_equivalent"]:
                            successful_comparisons += 1
                        else:
                            analysis["equivalence_issues"].extend(comparison.get("issues", []))

                        total_comparisons += 1

                except Exception as e:
                    analysis["equivalence_issues"].append(
                        f"Failed to execute equivalent script {eq_script.path.name}: {str(e)}"
                    )

            # 计算等效性分数
            if total_comparisons > 0:
                analysis["equivalence_score"] = successful_comparisons / total_comparisons
                analysis["platform_equivalence_verified"] = analysis["equivalence_score"] >= 0.8
            else:
                analysis["platform_equivalence_verified"] = False
                analysis["equivalence_issues"].append("No equivalent scripts could be executed")

        except Exception as e:
            analysis["platform_equivalence_verified"] = False
            analysis["equivalence_issues"].append(f"Analysis failed: {str(e)}")

        return analysis

    def _compare_script_results(
        self,
        original_result: ExecutionResult,
        equivalent_result: ExecutionResult
    ) -> Dict[str, Any]:
        """比较脚本执行结果的功能等效性"""
        comparison = {
            "functionally_equivalent": True,
            "exit_code_match": original_result.exit_code == equivalent_result.exit_code,
            "output_similarity": 0.0,
            "issues": []
        }

        # 比较退出代码
        if not comparison["exit_code_match"]:
            comparison["issues"].append(
                f"Exit codes differ: {original_result.exit_code} vs {equivalent_result.exit_code}"
            )

        # 比较输出相似性
        original_normalized = self._normalize_output(original_result.stdout)
        equivalent_normalized = self._normalize_output(equivalent_result.stdout)

        # 计算输出相似性（简单的字符串相似度）
        similarity = self._calculate_string_similarity(original_normalized, equivalent_normalized)
        comparison["output_similarity"] = similarity

        if similarity < 0.7:  # 70%相似度阈值
            comparison["issues"].append(
                f"Output similarity too low: {similarity:.2f}"
            )

        # 确定功能等效性
        comparison["functionally_equivalent"] = (
            comparison["exit_code_match"] and
            comparison["output_similarity"] >= 0.7
        )

        return comparison

    def _calculate_string_similarity(self, str1: str, str2: str) -> float:
        """计算字符串相似度"""
        if not str1 and not str2:
            return 1.0
        if not str1 or not str2:
            return 0.0

        # 使用简单的Jaccard相似度
        set1 = set(str1.split())
        set2 = set(str2.split())

        intersection = len(set1.intersection(set2))
        union = len(set1.union(set2))

        return intersection / union if union > 0 else 0.0

    def _determine_compatibility_status(self, validation_details: Dict[str, Any]) -> ValidationStatus:
        """确定兼容性验证状态"""
        if validation_details.get("error"):
            return ValidationStatus.ERROR

        # 检查跨平台一致性
        cross_platform_consistent = validation_details.get("cross_platform_consistent")
        if cross_platform_consistent is False:
            return ValidationStatus.FAILED

        # 检查平台等效性
        platform_equivalence_verified = validation_details.get("platform_equivalence_verified")
        if platform_equivalence_verified is False:
            return ValidationStatus.FAILED

        # 如果有任何一项无法验证（None），但没有错误，则跳过
        if cross_platform_consistent is None and platform_equivalence_verified is None:
            return ValidationStatus.SKIPPED

        return ValidationStatus.PASSED
