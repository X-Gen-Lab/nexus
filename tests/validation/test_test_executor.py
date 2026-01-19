"""
\file            test_test_executor.py
\brief           测试执行器的属性测试
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         使用Hypothesis进行基于属性的测试，验证测试执行器的正确性。
"""

import pytest
from hypothesis import given, settings, strategies as st
from pathlib import Path
import tempfile
import shutil
import os

# 导入被测试的模块
import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "scripts"))

from validation.test_executor import TestExecutor, TestExecutorError
from validation.models import ValidationConfig, TestResult, TestFailure


class TestTestExecutorProperties:
    """
    \brief           测试执行器的属性测试类
    """

    @settings(max_examples=100)
    @given(
        test_names=st.lists(
            st.text(
                alphabet=st.characters(whitelist_categories=('Lu', 'Ll', 'Nd')),
                min_size=5,
                max_size=20
            ),
            min_size=1,
            max_size=10
        )
    )
    def test_property_execution_completeness(self, test_names):
        """
        Feature: system-validation, Property 1: 测试执行完整性

        *对于任何*测试套件，当执行验证脚本时，所有已注册的测试用例
        都应该被执行，并且每个测试的结果都应该被记录。

        **验证需求: 8.2, 8.3, 8.4**
        """
        # 创建模拟的Google Test输出
        total_tests = len(test_names)
        passed_tests = total_tests  # 假设所有测试都通过

        # 构建模拟输出
        output_lines = [
            f"[==========] Running {total_tests} tests from 1 test suite.",
            "[----------] Global test environment set-up.",
        ]

        # 添加每个测试的运行信息
        for test_name in test_names:
            output_lines.append(f"[ RUN      ] TestSuite.{test_name}")
            output_lines.append(f"[       OK ] TestSuite.{test_name} (0 ms)")

        output_lines.extend([
            "[----------] Global test environment tear-down",
            f"[==========] {total_tests} tests from 1 test suite ran. ({total_tests * 10} ms total)",
            f"[  PASSED  ] {passed_tests} tests."
        ])

        output = "\n".join(output_lines)

        # 创建测试执行器
        config = ValidationConfig(
            build_dir="build",
            source_dir=".",
            verbose=False
        )
        executor = TestExecutor(config)

        # 解析输出
        result = executor.parse_test_output(output, "test_suite")

        # 验证属性：所有测试都被记录
        assert result.total_tests == total_tests, \
            f"期望 {total_tests} 个测试，但记录了 {result.total_tests} 个"

        assert result.passed_tests == passed_tests, \
            f"期望 {passed_tests} 个通过，但记录了 {result.passed_tests} 个"

        assert result.failed_tests == 0, \
            f"期望 0 个失败，但记录了 {result.failed_tests} 个"

        # 验证结果状态一致性
        assert result.passed_tests + result.failed_tests + result.skipped_tests == result.total_tests, \
            "测试结果计数不一致"

    @settings(max_examples=100)
    @given(
        passed_count=st.integers(min_value=0, max_value=50),
        failed_count=st.integers(min_value=0, max_value=50),
        skipped_count=st.integers(min_value=0, max_value=10)
    )
    def test_property_result_consistency(self, passed_count, failed_count, skipped_count):
        """
        Feature: system-validation, Property: 测试结果一致性

        *对于任何*测试结果，通过、失败和跳过的测试数量之和应该等于总测试数。

        **验证需求: 8.2, 8.3, 8.4**
        """
        total_tests = passed_count + failed_count + skipped_count

        if total_tests == 0:
            # 跳过空测试套件
            return

        # 构建模拟输出
        output_lines = [
            f"[==========] Running {total_tests} tests from 1 test suite.",
        ]

        # 添加通过的测试
        for i in range(passed_count):
            output_lines.append(f"[ RUN      ] TestSuite.PassedTest{i}")
            output_lines.append(f"[       OK ] TestSuite.PassedTest{i} (0 ms)")

        # 添加失败的测试
        for i in range(failed_count):
            output_lines.append(f"[ RUN      ] TestSuite.FailedTest{i}")
            output_lines.append(f"[  FAILED  ] TestSuite.FailedTest{i} (0 ms)")

        # 添加跳过的测试
        for i in range(skipped_count):
            output_lines.append(f"[ SKIPPED ] TestSuite.SkippedTest{i}")

        output_lines.extend([
            f"[==========] {total_tests} tests from 1 test suite ran. (100 ms total)",
            f"[  PASSED  ] {passed_count} tests."
        ])

        if failed_count > 0:
            output_lines.append(f"[  FAILED  ] {failed_count} tests, listed below:")
            for i in range(failed_count):
                output_lines.append(f"[  FAILED  ] TestSuite.FailedTest{i}")

        if skipped_count > 0:
            output_lines.append(f"[  SKIPPED ] {skipped_count} tests.")

        output = "\n".join(output_lines)

        # 创建测试执行器
        config = ValidationConfig(verbose=False)
        executor = TestExecutor(config)

        # 解析输出
        result = executor.parse_test_output(output, "test_suite")

        # 验证属性：结果一致性
        assert result.total_tests == total_tests, \
            f"总测试数不匹配: 期望 {total_tests}, 实际 {result.total_tests}"

        assert result.passed_tests + result.failed_tests + result.skipped_tests == result.total_tests, \
            f"测试计数不一致: {result.passed_tests} + {result.failed_tests} + {result.skipped_tests} != {result.total_tests}"

    @settings(max_examples=100)
    @given(
        execution_time_ms=st.integers(min_value=0, max_value=10000)
    )
    def test_property_execution_time_parsing(self, execution_time_ms):
        """
        Feature: system-validation, Property: 执行时间解析正确性

        *对于任何*有效的执行时间，解析器应该正确地将毫秒转换为秒。

        **验证需求: 8.2, 8.3, 8.4**
        """
        # 构建包含执行时间的输出
        output = f"""
[==========] Running 1 test from 1 test suite.
[ RUN      ] TestSuite.TestName
[       OK ] TestSuite.TestName (0 ms)
[==========] 1 test from 1 test suite ran. ({execution_time_ms} ms total)
[  PASSED  ] 1 test.
"""

        # 创建测试执行器
        config = ValidationConfig(verbose=False)
        executor = TestExecutor(config)

        # 解析输出
        result = executor.parse_test_output(output, "test_suite")

        # 验证属性：执行时间正确转换
        expected_time_seconds = execution_time_ms / 1000.0
        assert abs(result.execution_time - expected_time_seconds) < 0.001, \
            f"执行时间转换错误: 期望 {expected_time_seconds}s, 实际 {result.execution_time}s"

    def test_property_failure_extraction(self):
        """
        Feature: system-validation, Property: 失败信息提取完整性

        *对于任何*失败的测试，解析器应该提取测试名称和错误消息。

        **验证需求: 8.2, 8.3, 8.4**
        """
        # 构建包含失败测试的输出
        output = """
[==========] Running 2 tests from 1 test suite.
[ RUN      ] TestSuite.PassedTest
[       OK ] TestSuite.PassedTest (0 ms)
[ RUN      ] TestSuite.FailedTest
test_file.cpp:42: Failure
Expected: (5) == (10), actual: 5 vs 10
[  FAILED  ] TestSuite.FailedTest (1 ms)
[==========] 2 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 1 test.
[  FAILED  ] 1 test, listed below:
[  FAILED  ] TestSuite.FailedTest
"""

        # 创建测试执行器
        config = ValidationConfig(verbose=False)
        executor = TestExecutor(config)

        # 解析输出
        result = executor.parse_test_output(output, "test_suite")

        # 验证属性：失败信息被正确提取
        assert result.total_tests == 2
        assert result.passed_tests == 1
        assert result.failed_tests == 1
        assert len(result.failures) == 1

        failure = result.failures[0]
        assert failure.test_name == "TestSuite.FailedTest"
        assert failure.error_message != ""  # 应该提取到错误消息

    def test_property_counterexample_extraction(self):
        """
        Feature: system-validation, Property: 反例提取

        *对于任何*属性测试失败，解析器应该尝试提取反例信息。

        **验证需求: 8.2, 8.3, 8.4**
        """
        # 构建包含反例的输出
        output = """
[==========] Running 1 test from 1 test suite.
[ RUN      ] PropertyTest.TestName
Counterexample: input = [1, 2, 3, 4, 5]
[  FAILED  ] PropertyTest.TestName (10 ms)
[==========] 1 test from 1 test suite ran. (10 ms total)
[  FAILED  ] 1 test, listed below:
[  FAILED  ] PropertyTest.TestName
"""

        # 创建测试执行器
        config = ValidationConfig(verbose=False)
        executor = TestExecutor(config)

        # 解析输出
        result = executor.parse_test_output(output, "property_test")

        # 验证属性：反例被正确提取
        assert result.failed_tests == 1
        assert len(result.failures) == 1

        failure = result.failures[0]
        assert failure.counterexample is not None
        assert "input = [1, 2, 3, 4, 5]" in failure.counterexample


class TestTestExecutorUnitTests:
    """
    \brief           测试执行器的单元测试类
    """

    def test_parse_empty_output(self):
        """
        \brief           测试解析空输出
        """
        config = ValidationConfig(verbose=False)
        executor = TestExecutor(config)

        result = executor.parse_test_output("", "empty_suite")

        assert result.suite_name == "empty_suite"
        assert result.total_tests == 0
        assert result.passed_tests == 0
        assert result.failed_tests == 0

    def test_parse_ctest_summary(self):
        """
        \brief           测试解析CTest汇总信息
        """
        output = "100% tests passed, 0 tests failed out of 10"

        config = ValidationConfig(verbose=False)
        executor = TestExecutor(config)

        result = executor.parse_test_output(output, "ctest_suite")

        assert result.total_tests == 10
        assert result.passed_tests == 10
        assert result.failed_tests == 0

    def test_build_log_capture(self):
        """
        \brief           测试构建日志捕获
        """
        config = ValidationConfig(
            build_dir="nonexistent_build_dir",
            source_dir=".",
            verbose=False
        )
        executor = TestExecutor(config)

        # 尝试构建（应该失败）
        success = executor.build_tests()

        # 验证日志被捕获
        build_log = executor.get_build_log()
        assert len(build_log) > 0  # 应该有日志记录
