"""
JSON报告生成器

生成结构化的JSON格式验证报告，支持程序化消费和API集成。
"""

import json
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List
from datetime import datetime

from ..interfaces import ReportGenerator
from ..models import (
    ValidationReport, ValidationResult, ValidationStatus,
    Platform, Script, CompatibilityMatrix, ValidationSummary,
    EnvironmentInfo, ScriptMetadata, ScriptType, ScriptCategory
)


class JSONReporter(ReportGenerator):
    """JSON报告生成器

    生成结构化的JSON格式报告，包含完整的验证结果、
    兼容性矩阵和元数据，支持程序化消费和API集成。
    """

    def __init__(self, indent: int = 2, ensure_ascii: bool = False):
        """初始化JSON报告生成器

        Args:
            indent: JSON缩进空格数，默认为2
            ensure_ascii: 是否确保ASCII编码，默认为False以支持中文
        """
        self._indent = indent
        self._ensure_ascii = ensure_ascii

    def get_report_format(self) -> str:
        """获取报告格式"""
        return "json"

    def generate_report(self, report_data: ValidationReport) -> str:
        """生成JSON报告

        Args:
            report_data: 验证报告数据

        Returns:
            JSON格式的报告内容
        """
        report_dict = self._serialize_report(report_data)
        return json.dumps(
            report_dict,
            indent=self._indent,
            ensure_ascii=self._ensure_ascii,
            default=self._json_serializer
        )

    def save_report(self, report_content: str, output_path: str) -> bool:
        """保存报告到文件

        Args:
            report_content: JSON报告内容
            output_path: 输出文件路径

        Returns:
            保存是否成功
        """
        try:
            path = Path(output_path)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(report_content, encoding='utf-8')
            return True
        except Exception:
            return False


    def _serialize_report(self, report_data: ValidationReport) -> Dict[str, Any]:
        """序列化验证报告为字典

        Args:
            report_data: 验证报告数据

        Returns:
            可JSON序列化的字典
        """
        return {
            "report_format": "json",
            "version": "1.0.0",
            "generated_at": report_data.timestamp.isoformat(),
            "metadata": self._serialize_metadata(report_data),
            "environment": self._serialize_environment(report_data.environment),
            "summary": self._serialize_summary(report_data.summary),
            "compatibility_matrix": self._serialize_compatibility_matrix(
                report_data.compatibility_matrix
            ),
            "results": [
                self._serialize_validation_result(result)
                for result in report_data.results
            ],
            "recommendations": report_data.recommendations
        }

    def _serialize_metadata(self, report_data: ValidationReport) -> Dict[str, Any]:
        """序列化报告元数据

        Args:
            report_data: 验证报告数据

        Returns:
            元数据字典
        """
        return {
            "report_type": "script_validation",
            "project": "Nexus嵌入式系统项目",
            "timestamp": report_data.timestamp.isoformat(),
            "total_scripts": report_data.summary.total_scripts,
            "platforms_tested": [
                p.value for p in report_data.compatibility_matrix.platforms
            ]
        }

    def _serialize_environment(self, env: EnvironmentInfo) -> Dict[str, Any]:
        """序列化环境信息

        Args:
            env: 环境信息

        Returns:
            环境信息字典
        """
        return {
            "platform": env.platform.value,
            "os_version": env.os_version,
            "python_version": env.python_version,
            "shell_version": env.shell_version,
            "available_commands": env.available_commands
        }

    def _serialize_summary(self, summary: ValidationSummary) -> Dict[str, Any]:
        """序列化验证摘要

        Args:
            summary: 验证摘要

        Returns:
            摘要字典
        """
        return {
            "total_scripts": summary.total_scripts,
            "passed": summary.passed,
            "failed": summary.failed,
            "skipped": summary.skipped,
            "errors": summary.errors,
            "execution_time_seconds": summary.execution_time,
            "pass_rate": self._calculate_pass_rate(summary)
        }

    def _calculate_pass_rate(self, summary: ValidationSummary) -> float:
        """计算通过率

        Args:
            summary: 验证摘要

        Returns:
            通过率（0.0-1.0）
        """
        total = summary.total_scripts
        if total == 0:
            return 0.0
        return round(summary.passed / total, 4)


    def _serialize_compatibility_matrix(
        self, matrix: CompatibilityMatrix
    ) -> Dict[str, Any]:
        """序列化兼容性矩阵

        Args:
            matrix: 兼容性矩阵

        Returns:
            兼容性矩阵字典
        """
        # 构建脚本列表
        scripts_list = [
            {
                "name": script.name,
                "path": str(script.path),
                "type": script.type.value,
                "category": script.category.value
            }
            for script in matrix.scripts
        ]

        # 构建平台列表
        platforms_list = [p.value for p in matrix.platforms]

        # 构建矩阵数据
        matrix_data = {}
        for script in matrix.scripts:
            script_results = {}
            for platform in matrix.platforms:
                result = matrix.get_result(script.name, platform)
                if result:
                    script_results[platform.value] = {
                        "status": result.status.value,
                        "execution_time": result.execution_time,
                        "memory_usage": result.memory_usage
                    }
                else:
                    script_results[platform.value] = {
                        "status": "not_tested",
                        "execution_time": 0.0,
                        "memory_usage": 0
                    }
            matrix_data[script.name] = script_results

        return {
            "scripts": scripts_list,
            "platforms": platforms_list,
            "matrix": matrix_data
        }

    def _serialize_validation_result(
        self, result: ValidationResult
    ) -> Dict[str, Any]:
        """序列化单个验证结果

        Args:
            result: 验证结果

        Returns:
            验证结果字典
        """
        return {
            "script": self._serialize_script(result.script),
            "platform": result.platform.value,
            "validator": result.validator,
            "status": result.status.value,
            "execution_time_seconds": result.execution_time,
            "memory_usage_bytes": result.memory_usage,
            "output": result.output,
            "error": result.error,
            "details": result.details
        }

    def _serialize_script(self, script: Script) -> Dict[str, Any]:
        """序列化脚本信息

        Args:
            script: 脚本对象

        Returns:
            脚本信息字典
        """
        return {
            "name": script.name,
            "path": str(script.path),
            "type": script.type.value,
            "platform": script.platform.value,
            "category": script.category.value,
            "metadata": self._serialize_script_metadata(script.metadata),
            "dependencies": script.dependencies
        }

    def _serialize_script_metadata(
        self, metadata: ScriptMetadata
    ) -> Dict[str, Any]:
        """序列化脚本元数据

        Args:
            metadata: 脚本元数据

        Returns:
            元数据字典
        """
        return {
            "description": metadata.description,
            "usage": metadata.usage,
            "parameters": [
                {
                    "name": param.name,
                    "description": param.description,
                    "required": param.required,
                    "default_value": param.default_value
                }
                for param in metadata.parameters
            ],
            "examples": metadata.examples,
            "author": metadata.author,
            "version": metadata.version
        }

    def _json_serializer(self, obj: Any) -> Any:
        """自定义JSON序列化器

        处理无法直接序列化的对象类型。

        Args:
            obj: 要序列化的对象

        Returns:
            可序列化的值

        Raises:
            TypeError: 如果对象无法序列化
        """
        if isinstance(obj, datetime):
            return obj.isoformat()
        if isinstance(obj, Path):
            return str(obj)
        if isinstance(obj, Enum):
            return obj.value
        raise TypeError(f"Object of type {type(obj).__name__} is not JSON serializable")
