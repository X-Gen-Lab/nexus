"""
\file            report_generator.py
\brief           系统验证框架的报告生成器
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         提供测试报告生成功能，包括汇总报告、失败详情报告、
                 性能报告、JUnit XML报告和HTML可视化报告。
"""

import os
import json
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import List, Dict, Optional
from datetime import datetime
from jinja2 import Environment, FileSystemLoader, select_autoescape

from .models import (
    ValidationResult,
    TestResult,
    TestFailure,
    CoverageData,
    ValidationConfig
)


class ReportGenerationError(Exception):
    """
    \brief           报告生成错误异常
    """
    pass


class ReportGenerator:
    """
    \brief           报告生成器
    """

    def __init__(self, config: ValidationConfig):
        """
        \brief           初始化报告生成器
        \param[in]       config: 验证配置对象
        """
        self.config = config
        self.report_dir = Path(config.report_dir)
        self.report_dir.mkdir(parents=True, exist_ok=True)

        # 初始化Jinja2模板环境
        template_dir = Path(__file__).parent / "templates"
        self.jinja_env = Environment(
            loader=FileSystemLoader(str(template_dir)),
            autoescape=select_autoescape(['html', 'xml'])
        )

    def generate_summary_report(self, results: List[TestResult]) -> str:
        """
        \brief           生成测试汇总报告
        \param[in]       results: 测试结果列表
        \return          报告文件路径
        \details         生成包含所有测试结果的汇总报告，统计通过/失败/跳过
                         的测试数量和总执行时间
        """
        try:
            # 计算汇总统计
            total_tests = sum(r.total_tests for r in results)
            passed_tests = sum(r.passed_tests for r in results)
            failed_tests = sum(r.failed_tests for r in results)
            skipped_tests = sum(r.skipped_tests for r in results)
            total_time = sum(r.execution_time for r in results)

            # 计算通过率
            pass_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0.0

            # 生成报告内容
            report_content = self._format_summary_text(
                results=results,
                total_tests=total_tests,
                passed_tests=passed_tests,
                failed_tests=failed_tests,
                skipped_tests=skipped_tests,
                total_time=total_time,
                pass_rate=pass_rate
            )

            # 保存报告
            report_path = self.report_dir / "summary.txt"
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report_content)

            # 同时保存JSON格式
            json_path = self.report_dir / "summary.json"
            self._save_summary_json(
                json_path,
                results,
                total_tests,
                passed_tests,
                failed_tests,
                skipped_tests,
                total_time,
                pass_rate
            )

            return str(report_path)

        except Exception as e:
            raise ReportGenerationError(f"生成汇总报告失败: {str(e)}")

    def generate_failure_report(self, results: List[TestResult]) -> str:
        """
        \brief           生成失败测试详情报告
        \param[in]       results: 测试结果列表
        \return          报告文件路径
        \details         列出所有失败的测试，包含错误消息、堆栈跟踪和
                         属性测试的反例
        """
        try:
            # 收集所有失败的测试
            all_failures: List[tuple[str, TestFailure]] = []
            for result in results:
                for failure in result.failures:
                    all_failures.append((result.suite_name, failure))

            if not all_failures:
                # 没有失败的测试
                report_content = "所有测试通过！没有失败的测试。\n"
            else:
                report_content = self._format_failure_text(all_failures)

            # 保存报告
            report_path = self.report_dir / "failures.txt"
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report_content)

            # 同时保存JSON格式
            json_path = self.report_dir / "failures.json"
            self._save_failures_json(json_path, all_failures)

            return str(report_path)

        except Exception as e:
            raise ReportGenerationError(f"生成失败报告失败: {str(e)}")

    def generate_performance_report(
        self,
        results: List[TestResult],
        slow_threshold: float = 1.0
    ) -> str:
        """
        \brief           生成性能报告
        \param[in]       results: 测试结果列表
        \param[in]       slow_threshold: 慢速测试阈值（秒）
        \return          报告文件路径
        \details         记录每个测试套件的执行时间，识别慢速测试
        """
        try:
            # 按执行时间排序
            sorted_results = sorted(
                results,
                key=lambda r: r.execution_time,
                reverse=True
            )

            # 识别慢速测试
            slow_tests = [r for r in sorted_results if r.execution_time > slow_threshold]

            # 生成报告内容
            report_content = self._format_performance_text(
                sorted_results,
                slow_tests,
                slow_threshold
            )

            # 保存报告
            report_path = self.report_dir / "performance.txt"
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(report_content)

            # 同时保存JSON格式
            json_path = self.report_dir / "performance.json"
            self._save_performance_json(
                json_path,
                sorted_results,
                slow_tests,
                slow_threshold
            )

            return str(report_path)

        except Exception as e:
            raise ReportGenerationError(f"生成性能报告失败: {str(e)}")

    def generate_junit_report(self, results: List[TestResult]) -> str:
        """
        \brief           生成JUnit XML报告
        \param[in]       results: 测试结果列表
        \return          报告文件路径
        \details         生成符合JUnit格式的XML报告，支持CI工具集成
        """
        try:
            # 创建根元素
            testsuites = ET.Element('testsuites')

            # 计算总体统计
            total_tests = sum(r.total_tests for r in results)
            total_failures = sum(r.failed_tests for r in results)
            total_skipped = sum(r.skipped_tests for r in results)
            total_time = sum(r.execution_time for r in results)

            testsuites.set('tests', str(total_tests))
            testsuites.set('failures', str(total_failures))
            testsuites.set('skipped', str(total_skipped))
            testsuites.set('time', f"{total_time:.3f}")
            testsuites.set('timestamp', datetime.now().isoformat())

            # 为每个测试套件创建testsuite元素
            for result in results:
                testsuite = self._create_junit_testsuite(result)
                testsuites.append(testsuite)

            # 生成XML树
            tree = ET.ElementTree(testsuites)
            ET.indent(tree, space="  ")

            # 保存报告
            report_path = self.report_dir / "junit.xml"
            tree.write(
                report_path,
                encoding='utf-8',
                xml_declaration=True
            )

            return str(report_path)

        except Exception as e:
            raise ReportGenerationError(f"生成JUnit报告失败: {str(e)}")

    def generate_html_report(self, validation_result: ValidationResult) -> str:
        """
        \brief           生成HTML可视化报告
        \param[in]       validation_result: 完整的验证结果
        \return          报告文件路径
        \details         使用Jinja2模板引擎创建交互式HTML报告，包含覆盖率可视化
        """
        try:
            # 准备模板数据
            template_data = self._prepare_html_template_data(validation_result)

            # 渲染HTML模板
            template = self.jinja_env.get_template('report.html.j2')
            html_content = template.render(**template_data)

            # 保存报告
            report_path = self.report_dir / "report.html"
            with open(report_path, 'w', encoding='utf-8') as f:
                f.write(html_content)

            return str(report_path)

        except Exception as e:
            raise ReportGenerationError(f"生成HTML报告失败: {str(e)}")

    def _format_summary_text(
        self,
        results: List[TestResult],
        total_tests: int,
        passed_tests: int,
        failed_tests: int,
        skipped_tests: int,
        total_time: float,
        pass_rate: float
    ) -> str:
        """
        \brief           格式化汇总报告文本
        """
        lines = [
            "=" * 80,
            "测试汇总报告",
            "=" * 80,
            "",
            f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "总体统计:",
            f"  总测试数:   {total_tests}",
            f"  通过:       {passed_tests}",
            f"  失败:       {failed_tests}",
            f"  跳过:       {skipped_tests}",
            f"  通过率:     {pass_rate:.2f}%",
            f"  总执行时间: {total_time:.3f} 秒",
            "",
            "=" * 80,
            "各测试套件详情:",
            "=" * 80,
            ""
        ]

        for result in results:
            suite_pass_rate = (
                result.passed_tests / result.total_tests * 100
                if result.total_tests > 0 else 0.0
            )
            lines.extend([
                f"测试套件: {result.suite_name}",
                f"  总测试数: {result.total_tests}",
                f"  通过:     {result.passed_tests}",
                f"  失败:     {result.failed_tests}",
                f"  跳过:     {result.skipped_tests}",
                f"  通过率:   {suite_pass_rate:.2f}%",
                f"  执行时间: {result.execution_time:.3f} 秒",
                ""
            ])

        lines.append("=" * 80)
        return "\n".join(lines)

    def _save_summary_json(
        self,
        json_path: Path,
        results: List[TestResult],
        total_tests: int,
        passed_tests: int,
        failed_tests: int,
        skipped_tests: int,
        total_time: float,
        pass_rate: float
    ) -> None:
        """
        \brief           保存汇总报告JSON格式
        """
        summary_data = {
            "timestamp": datetime.now().isoformat(),
            "summary": {
                "total_tests": total_tests,
                "passed_tests": passed_tests,
                "failed_tests": failed_tests,
                "skipped_tests": skipped_tests,
                "pass_rate": pass_rate,
                "total_time": total_time
            },
            "test_suites": [
                {
                    "name": r.suite_name,
                    "total_tests": r.total_tests,
                    "passed_tests": r.passed_tests,
                    "failed_tests": r.failed_tests,
                    "skipped_tests": r.skipped_tests,
                    "execution_time": r.execution_time
                }
                for r in results
            ]
        }

        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(summary_data, f, indent=2, ensure_ascii=False)

    def _format_failure_text(
        self,
        all_failures: List[tuple[str, TestFailure]]
    ) -> str:
        """
        \brief           格式化失败报告文本
        """
        lines = [
            "=" * 80,
            "失败测试详情报告",
            "=" * 80,
            "",
            f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"失败测试总数: {len(all_failures)}",
            "",
            "=" * 80,
            ""
        ]

        for i, (suite_name, failure) in enumerate(all_failures, 1):
            lines.extend([
                f"失败 #{i}",
                "-" * 80,
                f"测试套件: {suite_name}",
                f"测试名称: {failure.test_name}",
                "",
                "错误消息:",
                failure.error_message,
                ""
            ])

            if failure.stack_trace:
                lines.extend([
                    "堆栈跟踪:",
                    failure.stack_trace,
                    ""
                ])

            if failure.counterexample:
                lines.extend([
                    "属性测试反例:",
                    failure.counterexample,
                    ""
                ])

            lines.append("=" * 80)
            lines.append("")

        return "\n".join(lines)

    def _save_failures_json(
        self,
        json_path: Path,
        all_failures: List[tuple[str, TestFailure]]
    ) -> None:
        """
        \brief           保存失败报告JSON格式
        """
        failures_data = {
            "timestamp": datetime.now().isoformat(),
            "total_failures": len(all_failures),
            "failures": [
                {
                    "suite_name": suite_name,
                    "test_name": failure.test_name,
                    "error_message": failure.error_message,
                    "stack_trace": failure.stack_trace,
                    "counterexample": failure.counterexample
                }
                for suite_name, failure in all_failures
            ]
        }

        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(failures_data, f, indent=2, ensure_ascii=False)

    def _format_performance_text(
        self,
        sorted_results: List[TestResult],
        slow_tests: List[TestResult],
        slow_threshold: float
    ) -> str:
        """
        \brief           格式化性能报告文本
        """
        lines = [
            "=" * 80,
            "性能报告",
            "=" * 80,
            "",
            f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"慢速测试阈值: {slow_threshold:.3f} 秒",
            f"慢速测试数量: {len(slow_tests)}",
            "",
            "=" * 80,
            "测试套件执行时间（按时间降序）:",
            "=" * 80,
            ""
        ]

        for result in sorted_results:
            is_slow = " [慢速]" if result.execution_time > slow_threshold else ""
            lines.append(
                f"  {result.suite_name:40s} {result.execution_time:8.3f} 秒{is_slow}"
            )

        if slow_tests:
            lines.extend([
                "",
                "=" * 80,
                "慢速测试详情:",
                "=" * 80,
                ""
            ])

            for result in slow_tests:
                lines.extend([
                    f"测试套件: {result.suite_name}",
                    f"  执行时间: {result.execution_time:.3f} 秒",
                    f"  测试数量: {result.total_tests}",
                    f"  平均时间: {result.execution_time / result.total_tests:.3f} 秒/测试"
                        if result.total_tests > 0 else "  平均时间: N/A",
                    ""
                ])

        lines.append("=" * 80)
        return "\n".join(lines)

    def _save_performance_json(
        self,
        json_path: Path,
        sorted_results: List[TestResult],
        slow_tests: List[TestResult],
        slow_threshold: float
    ) -> None:
        """
        \brief           保存性能报告JSON格式
        """
        performance_data = {
            "timestamp": datetime.now().isoformat(),
            "slow_threshold": slow_threshold,
            "slow_tests_count": len(slow_tests),
            "test_suites": [
                {
                    "name": r.suite_name,
                    "execution_time": r.execution_time,
                    "total_tests": r.total_tests,
                    "avg_time_per_test": (
                        r.execution_time / r.total_tests if r.total_tests > 0 else 0.0
                    ),
                    "is_slow": r.execution_time > slow_threshold
                }
                for r in sorted_results
            ]
        }

        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(performance_data, f, indent=2, ensure_ascii=False)

    def _create_junit_testsuite(self, result: TestResult) -> ET.Element:
        """
        \brief           创建JUnit testsuite元素
        """
        testsuite = ET.Element('testsuite')
        testsuite.set('name', self._sanitize_xml_text(result.suite_name))
        testsuite.set('tests', str(result.total_tests))
        testsuite.set('failures', str(result.failed_tests))
        testsuite.set('skipped', str(result.skipped_tests))
        testsuite.set('time', f"{result.execution_time:.3f}")
        testsuite.set('timestamp', datetime.now().isoformat())

        # 为通过的测试创建testcase元素
        passed_count = result.passed_tests
        for i in range(passed_count):
            testcase = ET.SubElement(testsuite, 'testcase')
            testcase.set('name', f"test_{i+1}")
            testcase.set('classname', self._sanitize_xml_text(result.suite_name))
            testcase.set('time', "0.000")

        # 为失败的测试创建testcase元素
        for i, failure in enumerate(result.failures):
            testcase = ET.SubElement(testsuite, 'testcase')
            testcase.set('name', self._sanitize_xml_text(failure.test_name))
            testcase.set('classname', self._sanitize_xml_text(result.suite_name))
            testcase.set('time', "0.000")

            # 添加failure元素
            failure_elem = ET.SubElement(testcase, 'failure')
            failure_elem.set('message', self._sanitize_xml_text(failure.error_message))
            failure_elem.set('type', 'AssertionError')

            # 添加详细信息
            failure_text = self._sanitize_xml_text(failure.error_message)
            if failure.stack_trace:
                failure_text += f"\n\n堆栈跟踪:\n{self._sanitize_xml_text(failure.stack_trace)}"
            if failure.counterexample:
                failure_text += f"\n\n反例:\n{self._sanitize_xml_text(failure.counterexample)}"

            failure_elem.text = failure_text

        # 为跳过的测试创建testcase元素
        skipped_count = result.skipped_tests
        for i in range(skipped_count):
            testcase = ET.SubElement(testsuite, 'testcase')
            testcase.set('name', f"skipped_test_{i+1}")
            testcase.set('classname', self._sanitize_xml_text(result.suite_name))
            testcase.set('time', "0.000")

            skipped_elem = ET.SubElement(testcase, 'skipped')
            skipped_elem.set('message', 'Test skipped')

        return testsuite

    def _sanitize_xml_text(self, text: str) -> str:
        """
        \brief           清理XML文本，移除无效字符
        \details         移除XML 1.0不允许的控制字符
        """
        if not text:
            return ""

        # XML 1.0允许的字符范围
        # #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
        valid_chars = []
        for char in text:
            code = ord(char)
            if (code == 0x9 or code == 0xA or code == 0xD or
                (0x20 <= code <= 0xD7FF) or
                (0xE000 <= code <= 0xFFFD) or
                (0x10000 <= code <= 0x10FFFF)):
                valid_chars.append(char)
            else:
                # 替换无效字符为空格
                valid_chars.append(' ')

        return ''.join(valid_chars)

    def _prepare_html_template_data(
        self,
        validation_result: ValidationResult
    ) -> Dict:
        """
        \brief           准备HTML模板数据
        """
        results = validation_result.test_results

        # 计算汇总统计
        total_tests = sum(r.total_tests for r in results)
        passed_tests = sum(r.passed_tests for r in results)
        failed_tests = sum(r.failed_tests for r in results)
        skipped_tests = sum(r.skipped_tests for r in results)
        pass_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0.0

        # 收集所有失败
        all_failures = []
        for result in results:
            for failure in result.failures:
                all_failures.append({
                    'suite_name': result.suite_name,
                    'test_name': failure.test_name,
                    'error_message': failure.error_message,
                    'stack_trace': failure.stack_trace,
                    'counterexample': failure.counterexample
                })

        # 准备覆盖率数据
        coverage_data = None
        if validation_result.coverage_data:
            cov = validation_result.coverage_data
            coverage_data = {
                'line_coverage': cov.line_coverage * 100,
                'branch_coverage': cov.branch_coverage * 100,
                'function_coverage': cov.function_coverage * 100,
                'uncovered_lines_count': len(cov.uncovered_lines),
                'uncovered_branches_count': len(cov.uncovered_branches)
            }

        return {
            'timestamp': validation_result.timestamp.strftime('%Y-%m-%d %H:%M:%S'),
            'success': validation_result.success,
            'total_tests': total_tests,
            'passed_tests': passed_tests,
            'failed_tests': failed_tests,
            'skipped_tests': skipped_tests,
            'pass_rate': pass_rate,
            'execution_time': validation_result.execution_time,
            'test_results': [
                {
                    'suite_name': r.suite_name,
                    'total_tests': r.total_tests,
                    'passed_tests': r.passed_tests,
                    'failed_tests': r.failed_tests,
                    'skipped_tests': r.skipped_tests,
                    'execution_time': r.execution_time,
                    'pass_rate': (
                        r.passed_tests / r.total_tests * 100
                        if r.total_tests > 0 else 0.0
                    )
                }
                for r in results
            ],
            'failures': all_failures,
            'coverage': coverage_data
        }
