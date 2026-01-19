r"""
\file            test_executor.py
\brief           系统验证框架的测试执行器
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         负责构建测试可执行文件、执行各类测试（单元测试、属性测试、
                 集成测试）、捕获测试输出并解析测试结果。
"""

import subprocess
import os
import re
import json
from pathlib import Path
from typing import Optional, List, Dict, Any
from .models import TestResult, TestFailure, TestStatus, ValidationConfig
from .utils import safe_print


class TestExecutorError(Exception):
    r"""
    \brief           测试执行器错误异常
    """
    pass


class TestExecutor:
    r"""
    \brief           测试执行器
    \details         负责构建和执行测试，解析测试输出
    """

    def __init__(self, config: ValidationConfig):
        r"""
        \brief           初始化测试执行器
        \param[in]       config: 验证配置对象
        """
        self.config = config
        self.build_dir = Path(config.build_dir)
        self.source_dir = Path(config.source_dir)
        self._build_log: List[str] = []
        self._test_output: Dict[str, str] = {}

    def build_tests(self) -> bool:
        r"""
        \brief           构建测试可执行文件
        \return          构建是否成功
        \details         使用CMake配置和构建测试，处理构建错误和日志
        """
        try:
            self._build_log.clear()

            # 创建构建目录
            self.build_dir.mkdir(parents=True, exist_ok=True)

            if self.config.verbose:
                safe_print(f"构建目录: {self.build_dir}")
                safe_print(f"源代码目录: {self.source_dir}")

            # 步骤 1: CMake 配置
            if not self._run_cmake_configure():
                return False

            # 步骤 2: CMake 构建
            if not self._run_cmake_build():
                return False

            if self.config.verbose:
                safe_print("✓ 测试构建成功")

            return True

        except Exception as e:
            error_msg = f"构建测试时发生异常: {str(e)}"
            self._build_log.append(error_msg)
            if self.config.verbose:
                safe_print(f"✗ {error_msg}")
            return False

    def _run_cmake_configure(self) -> bool:
        r"""
        \brief           运行CMake配置步骤
        \return          配置是否成功
        """
        cmake_args = [
            "cmake",
            "-S", str(self.source_dir),
            "-B", str(self.build_dir),
            "-DNEXUS_PLATFORM=native",
            "-DNEXUS_BUILD_TESTS=ON"
        ]

        # 添加覆盖率选项
        if self.config.coverage_enabled:
            cmake_args.append("-DNEXUS_ENABLE_COVERAGE=ON")

        # 添加构建类型
        cmake_args.extend(["-DCMAKE_BUILD_TYPE=Debug"])

        if self.config.verbose:
            safe_print(f"运行 CMake 配置: {' '.join(cmake_args)}")

        try:
            result = subprocess.run(
                cmake_args,
                cwd=str(self.source_dir),
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=self.config.test_timeout
            )

            self._build_log.append("=== CMake 配置输出 ===")
            self._build_log.append(result.stdout)

            if result.returncode != 0:
                self._build_log.append("=== CMake 配置错误 ===")
                self._build_log.append(result.stderr)
                if self.config.verbose:
                    safe_print(f"✗ CMake 配置失败")
                    safe_print(result.stderr)
                return False

            if self.config.verbose:
                safe_print("✓ CMake 配置成功")

            return True

        except subprocess.TimeoutExpired:
            error_msg = f"CMake 配置超时（{self.config.test_timeout}秒）"
            self._build_log.append(error_msg)
            if self.config.verbose:
                safe_print(f"✗ {error_msg}")
            return False
        except FileNotFoundError:
            error_msg = "未找到 CMake 命令，请确保已安装 CMake"
            self._build_log.append(error_msg)
            if self.config.verbose:
                safe_print(f"✗ {error_msg}")
            return False

    def _run_cmake_build(self) -> bool:
        r"""
        \brief           运行CMake构建步骤
        \return          构建是否成功
        """
        cmake_build_args = [
            "cmake",
            "--build", str(self.build_dir),
            "--config", "Debug"
        ]

        # 添加并行构建选项
        if self.config.parallel_jobs > 0:
            cmake_build_args.extend(["--parallel", str(self.config.parallel_jobs)])

        if self.config.verbose:
            safe_print(f"运行 CMake 构建: {' '.join(cmake_build_args)}")

        try:
            result = subprocess.run(
                cmake_build_args,
                cwd=str(self.source_dir),
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=self.config.test_timeout
            )

            self._build_log.append("=== CMake 构建输出 ===")
            self._build_log.append(result.stdout)

            if result.returncode != 0:
                self._build_log.append("=== CMake 构建错误 ===")
                self._build_log.append(result.stderr)
                if self.config.verbose:
                    safe_print(f"✗ CMake 构建失败")
                    safe_print(result.stderr)
                return False

            if self.config.verbose:
                safe_print("✓ CMake 构建成功")

            return True

        except subprocess.TimeoutExpired:
            error_msg = f"CMake 构建超时（{self.config.test_timeout}秒）"
            self._build_log.append(error_msg)
            if self.config.verbose:
                safe_print(f"✗ {error_msg}")
            return False

    def get_build_log(self) -> List[str]:
        r"""
        \brief           获取构建日志
        \return          构建日志行列表
        """
        return self._build_log.copy()

    def get_test_output(self, suite_name: str) -> Optional[str]:
        r"""
        \brief           获取指定测试套件的输出
        \param[in]       suite_name: 测试套件名称
        \return          测试输出字符串，如果不存在则返回None
        """
        return self._test_output.get(suite_name)

    def run_unit_tests(self) -> TestResult:
        r"""
        \brief           运行单元测试
        \return          测试结果对象
        \details         使用CTest执行单元测试并收集结果
        """
        return self._run_tests_by_label("unit")

    def run_property_tests(self) -> TestResult:
        r"""
        \brief           运行属性测试
        \return          测试结果对象
        \details         使用CTest执行属性测试并收集结果
        """
        return self._run_tests_by_label("property")

    def run_integration_tests(self) -> TestResult:
        r"""
        \brief           运行集成测试
        \return          测试结果对象
        \details         使用CTest执行集成测试并收集结果
        """
        return self._run_tests_by_label("integration")

    def _run_tests_by_label(self, label: str) -> TestResult:
        r"""
        \brief           根据标签运行测试
        \param[in]       label: 测试标签（unit/property/integration）
        \return          测试结果对象
        """
        suite_name = f"{label}_tests"

        if self.config.verbose:
            safe_print(f"\n运行 {label} 测试...")

        # 构建 CTest 命令
        ctest_args = [
            "ctest",
            "--output-on-failure",
            "--verbose" if self.config.verbose else "--output-on-failure"
        ]

        # 添加标签过滤
        ctest_args.extend(["-L", label])

        # 添加并行选项
        if self.config.parallel_jobs > 0:
            ctest_args.extend(["-j", str(self.config.parallel_jobs)])

        try:
            result = subprocess.run(
                ctest_args,
                cwd=str(self.build_dir),
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                timeout=self.config.test_timeout
            )

            # 保存测试输出
            output = result.stdout + "\n" + result.stderr
            self._test_output[suite_name] = output

            if self.config.verbose:
                safe_print(output)

            # 解析测试结果
            test_result = self.parse_test_output(output, suite_name)

            # 如果启用了 fail_fast 且有测试失败，抛出异常
            if self.config.fail_fast and test_result.failed_tests > 0:
                raise TestExecutorError(
                    f"{label} 测试失败: {test_result.failed_tests} 个测试未通过"
                )

            return test_result

        except subprocess.TimeoutExpired:
            error_msg = f"{label} 测试超时（{self.config.test_timeout}秒）"
            if self.config.verbose:
                safe_print(f"✗ {error_msg}")

            # 返回错误结果
            return TestResult(
                suite_name=suite_name,
                total_tests=0,
                passed_tests=0,
                failed_tests=1,
                skipped_tests=0,
                execution_time=float(self.config.test_timeout),
                failures=[TestFailure(
                    test_name=f"{label}_tests",
                    error_message=error_msg,
                    stack_trace=""
                )]
            )
        except FileNotFoundError:
            error_msg = "未找到 CTest 命令，请确保已安装 CMake"
            if self.config.verbose:
                safe_print(f"✗ {error_msg}")

            return TestResult(
                suite_name=suite_name,
                total_tests=0,
                passed_tests=0,
                failed_tests=1,
                skipped_tests=0,
                execution_time=0.0,
                failures=[TestFailure(
                    test_name=f"{label}_tests",
                    error_message=error_msg,
                    stack_trace=""
                )]
            )

    def parse_test_output(self, output: str, suite_name: str = "unknown") -> TestResult:
        r"""
        \brief           解析测试输出
        \param[in]       output: 测试输出字符串
        \param[in]       suite_name: 测试套件名称
        \return          测试结果对象
        \details         解析Google Test和CTest输出格式，提取测试结果和失败信息
        """
        import time

        # 初始化计数器
        total_tests = 0
        passed_tests = 0
        failed_tests = 0
        skipped_tests = 0
        execution_time = 0.0
        failures: List[TestFailure] = []

        # 解析 CTest 汇总信息
        # 格式: "100% tests passed, 0 tests failed out of 10"
        ctest_summary_pattern = r'(\d+)% tests passed, (\d+) tests failed out of (\d+)'
        ctest_match = re.search(ctest_summary_pattern, output)

        if ctest_match:
            failed_count = int(ctest_match.group(2))
            total_count = int(ctest_match.group(3))
            passed_count = total_count - failed_count

            total_tests = total_count
            passed_tests = passed_count
            failed_tests = failed_count

        # 解析 Google Test 输出
        # 格式: "[==========] Running 10 tests from 2 test suites."
        gtest_running_pattern = r'\[==========\] Running (\d+) tests? from \d+ test suites?'
        gtest_running_match = re.search(gtest_running_pattern, output)

        if gtest_running_match:
            total_tests = int(gtest_running_match.group(1))

        # 格式: "[  PASSED  ] 8 tests."
        gtest_passed_pattern = r'\[  PASSED  \] (\d+) tests?'
        gtest_passed_match = re.search(gtest_passed_pattern, output)

        if gtest_passed_match:
            passed_tests = int(gtest_passed_match.group(1))

        # 格式: "[  FAILED  ] 2 tests, listed below:"
        gtest_failed_pattern = r'\[  FAILED  \] (\d+) tests?'
        gtest_failed_match = re.search(gtest_failed_pattern, output)

        if gtest_failed_match:
            failed_tests = int(gtest_failed_match.group(1))

        # 格式: "[  SKIPPED ] 1 test."
        gtest_skipped_pattern = r'\[  SKIPPED \] (\d+) tests?'
        gtest_skipped_match = re.search(gtest_skipped_pattern, output)

        if gtest_skipped_match:
            skipped_tests = int(gtest_skipped_match.group(1))

        # 解析执行时间
        # 格式: "[==========] 10 tests from 2 test suites ran. (123 ms total)"
        time_pattern = r'\((\d+) ms total\)'
        time_match = re.search(time_pattern, output)

        if time_match:
            execution_time = float(time_match.group(1)) / 1000.0  # 转换为秒

        # 解析失败的测试详情
        # 格式: "[  FAILED  ] TestSuite.TestName"
        # 注意：只匹配包含点号的测试名称，避免匹配汇总行
        failed_test_pattern = r'\[  FAILED  \] (\w+\.\w+)'
        failed_test_matches = re.finditer(failed_test_pattern, output)

        seen_failures = set()  # 避免重复添加
        for match in failed_test_matches:
            test_name = match.group(1)

            # 跳过已经处理过的失败测试
            if test_name in seen_failures:
                continue
            seen_failures.add(test_name)

            # 尝试提取错误消息
            error_message = self._extract_error_message(output, test_name)

            # 尝试提取堆栈跟踪
            stack_trace = self._extract_stack_trace(output, test_name)

            # 尝试提取属性测试的反例
            counterexample = self._extract_counterexample(output, test_name)

            failures.append(TestFailure(
                test_name=test_name,
                error_message=error_message,
                stack_trace=stack_trace,
                counterexample=counterexample
            ))

        # 如果没有从输出中解析到测试数量，尝试从失败列表推断
        if total_tests == 0 and len(failures) > 0:
            total_tests = len(failures)
            failed_tests = len(failures)

        return TestResult(
            suite_name=suite_name,
            total_tests=total_tests,
            passed_tests=passed_tests,
            failed_tests=failed_tests,
            skipped_tests=skipped_tests,
            execution_time=execution_time,
            failures=failures
        )

    def _extract_error_message(self, output: str, test_name: str) -> str:
        r"""
        \brief           从输出中提取错误消息
        \param[in]       output: 测试输出
        \param[in]       test_name: 测试名称
        \return          错误消息字符串
        """
        # 查找测试失败后的错误消息
        # 通常在 "Expected:" 或 "Actual:" 或断言失败消息中

        # 尝试查找断言失败消息
        # 格式: "path/to/file.cpp:123: Failure"
        failure_pattern = rf'{re.escape(test_name)}.*?(\w+\.cpp:\d+:.*?)(?=\[|$)'
        failure_match = re.search(failure_pattern, output, re.DOTALL)

        if failure_match:
            error_text = failure_match.group(1).strip()
            # 限制错误消息长度
            if len(error_text) > 500:
                error_text = error_text[:500] + "..."
            return error_text

        # 如果没有找到详细错误，返回通用消息
        return f"测试 {test_name} 失败"

    def _extract_stack_trace(self, output: str, test_name: str) -> str:
        r"""
        \brief           从输出中提取堆栈跟踪
        \param[in]       output: 测试输出
        \param[in]       test_name: 测试名称
        \return          堆栈跟踪字符串
        """
        # Google Test 通常不提供完整的堆栈跟踪
        # 但会提供文件和行号信息

        # 查找文件:行号模式
        # 格式: "path/to/file.cpp:123"
        location_pattern = r'([\w/\\\.]+\.cpp:\d+)'
        locations = re.findall(location_pattern, output)

        if locations:
            return "\n".join(locations[:5])  # 限制为前5个位置

        return ""

    def _extract_counterexample(self, output: str, test_name: str) -> Optional[str]:
        r"""
        \brief           从输出中提取属性测试的反例
        \param[in]       output: 测试输出
        \param[in]       test_name: 测试名称
        \return          反例字符串，如果不是属性测试则返回None
        """
        # 查找属性测试的反例
        # 通常包含 "Counterexample:" 或 "Shrunk:" 等关键字

        counterexample_pattern = r'(?:Counterexample|Shrunk|Failed with):\s*([^\n]+)'
        counterexample_match = re.search(counterexample_pattern, output)

        if counterexample_match:
            counterexample = counterexample_match.group(1).strip()
            # 限制反例长度
            if len(counterexample) > 300:
                counterexample = counterexample[:300] + "..."
            return counterexample

        return None

