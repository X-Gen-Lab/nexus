"""
JUnit XML报告生成器

生成JUnit XML格式的验证报告，用于CI/CD系统集成。
支持Jenkins、GitHub Actions、GitLab CI等主流CI平台。
"""

import xml.etree.ElementTree as ET
from xml.dom import minidom
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any

from ..interfaces import ReportGenerator
from ..models import (
    ValidationReport, ValidationResult, ValidationStatus,
    Platform, ValidationSummary
)


class JUnitReporter(ReportGenerator):
    """JUnit XML报告生成器

    生成符合JUnit XML格式的验证报告，支持主流CI/CD平台集成。

    JUnit XML格式是CI/CD系统中最广泛支持的测试报告格式，
    支持Jenkins、GitHub Actions、GitLab CI、Azure DevOps等平台。
    """

    def __init__(self, testsuite_name: str = "script_validation"):
        """初始化JUnit报告生成器

        Args:
            testsuite_name: 测试套件名称，默认为"script_validation"
        """
        self._testsuite_name = testsuite_name

    def get_report_format(self) -> str:
        """获取报告格式"""
        return "junit"

    def generate_report(self, report_data: ValidationReport) -> str:
        """生成JUnit XML报告

        Args:
            report_data: 验证报告数据

        Returns:
            JUnit XML格式的报告内容
        """
        # 创建根元素 testsuites
        testsuites = ET.Element("testsuites")
        testsuites.set("name", "Script Validation Results")
        testsuites.set("time", str(report_data.summary.execution_time))
        testsuites.set("tests", str(report_data.summary.total_scripts))
        testsuites.set("failures", str(report_data.summary.failed))
        testsuites.set("errors", str(report_data.summary.errors))
        testsuites.set("skipped", str(report_data.summary.skipped))

        # 按平台分组创建测试套件
        platform_results = self._group_results_by_platform(report_data.results)

        for platform, results in platform_results.items():
            testsuite = self._create_testsuite(platform, results, report_data)
            testsuites.append(testsuite)

        # 格式化XML输出
        return self._prettify_xml(testsuites)

    def save_report(self, report_content: str, output_path: str) -> bool:
        """保存报告到文件

        Args:
            report_content: JUnit XML报告内容
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

    def _group_results_by_platform(
        self, results: List[ValidationResult]
    ) -> Dict[str, List[ValidationResult]]:
        """按平台分组验证结果

        Args:
            results: 验证结果列表

        Returns:
            平台到结果列表的映射
        """
        grouped = {}
        for result in results:
            platform_name = result.platform.value
            if platform_name not in grouped:
                grouped[platform_name] = []
            grouped[platform_name].append(result)
        return grouped

    def _create_testsuite(
        self,
        platform: str,
        results: List[ValidationResult],
        report_data: ValidationReport
    ) -> ET.Element:
        """创建测试套件元素

        Args:
            platform: 平台名称
            results: 该平台的验证结果列表
            report_data: 完整的验证报告数据

        Returns:
            testsuite XML元素
        """
        testsuite = ET.Element("testsuite")
        testsuite.set("name", f"{self._testsuite_name}.{platform}")
        testsuite.set("timestamp", report_data.timestamp.isoformat())

        # 计算该平台的统计信息
        total = len(results)
        failures = sum(1 for r in results if r.status == ValidationStatus.FAILED)
        errors = sum(1 for r in results if r.status == ValidationStatus.ERROR)
        skipped = sum(1 for r in results if r.status == ValidationStatus.SKIPPED)
        total_time = sum(r.execution_time for r in results)

        testsuite.set("tests", str(total))
        testsuite.set("failures", str(failures))
        testsuite.set("errors", str(errors))
        testsuite.set("skipped", str(skipped))
        testsuite.set("time", str(total_time))

        # 添加属性元素
        properties = ET.SubElement(testsuite, "properties")
        self._add_property(properties, "platform", platform)
        self._add_property(properties, "os_version", report_data.environment.os_version)
        self._add_property(properties, "python_version", report_data.environment.python_version)

        # 添加测试用例
        for result in results:
            testcase = self._create_testcase(result)
            testsuite.append(testcase)

        return testsuite

    def _create_testcase(self, result: ValidationResult) -> ET.Element:
        """创建测试用例元素

        Args:
            result: 验证结果

        Returns:
            testcase XML元素
        """
        testcase = ET.Element("testcase")
        testcase.set("name", result.script.name)
        testcase.set("classname", f"{self._testsuite_name}.{result.platform.value}.{result.script.category.value}")
        testcase.set("time", str(result.execution_time))

        # 根据状态添加相应的子元素
        if result.status == ValidationStatus.FAILED:
            failure = ET.SubElement(testcase, "failure")
            failure.set("message", f"Validation failed for {result.script.name}")
            failure.set("type", "ValidationFailure")
            failure.text = result.error or result.output or "Validation failed"

        elif result.status == ValidationStatus.ERROR:
            error = ET.SubElement(testcase, "error")
            error.set("message", f"Error executing {result.script.name}")
            error.set("type", "ExecutionError")
            error.text = result.error or "Unknown error occurred"

        elif result.status == ValidationStatus.SKIPPED:
            skipped = ET.SubElement(testcase, "skipped")
            skipped.set("message", f"Skipped {result.script.name}")
            if result.error:
                skipped.text = result.error

        # 添加系统输出
        if result.output:
            system_out = ET.SubElement(testcase, "system-out")
            system_out.text = self._sanitize_output(result.output)

        # 添加系统错误输出
        if result.error:
            system_err = ET.SubElement(testcase, "system-err")
            system_err.text = self._sanitize_output(result.error)

        return testcase

    def _add_property(self, properties: ET.Element, name: str, value: str) -> None:
        """添加属性元素

        Args:
            properties: properties父元素
            name: 属性名称
            value: 属性值
        """
        prop = ET.SubElement(properties, "property")
        prop.set("name", name)
        prop.set("value", value)

    def _sanitize_output(self, text: str) -> str:
        """清理输出文本，移除无效的XML字符

        Args:
            text: 原始文本

        Returns:
            清理后的文本
        """
        if not text:
            return ""

        # 移除无效的XML字符
        valid_chars = []
        for char in text:
            code = ord(char)
            # 有效的XML字符范围
            if (code == 0x9 or code == 0xA or code == 0xD or
                (0x20 <= code <= 0xD7FF) or
                (0xE000 <= code <= 0xFFFD) or
                (0x10000 <= code <= 0x10FFFF)):
                valid_chars.append(char)

        return ''.join(valid_chars)

    def _prettify_xml(self, element: ET.Element) -> str:
        """格式化XML输出

        Args:
            element: XML根元素

        Returns:
            格式化的XML字符串
        """
        rough_string = ET.tostring(element, encoding='unicode')
        reparsed = minidom.parseString(rough_string)
        return reparsed.toprettyxml(indent="  ", encoding=None)
