"""
file:            test_validation_controller.py
brief:           验证控制器的属性测试
author:          Nexus Team
version:         1.0.0
date:            2026-01-18

copyright:       Copyright (c) 2026 Nexus Team

details:         测试验证控制器的正确性属性，包括测试失败传播、
                 错误处理和流程协调。
"""

import pytest
from hypothesis import given, settings, strategies as st
from pathlib import Path
import tempfile
import shutil
from unittest.mock import Mock, patch, MagicMock

import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "scripts"))

from validation.models import (
    ValidationConfig,
    TestResult,
    TestFailure,
    CoverageData,
    CodeLocation
)
from validation.validation_controller import (
    ValidationController,
    ValidationControllerError
)


# 测试数据生成策略

@st.composite
def test_result_strategy(draw):
    """
    \brief           生成测试结果策略
    """
    total_tests = draw(st.integers(min_value=1, max_value=100))
    failed_tests = draw(st.integers(min_value=0, max_value=total_tests))
    skipped_tests = draw(st.integers(min_value=0, max_value=total_tests - failed_tests))
    passed_tests = total_tests - failed_tests - skipped_tests

    failures = []
    for i in range(failed_tests):
        failures.append(TestFailure(
            test_name=f"test_{i}",
            error_message=draw(st.text(min_size=1, max_size=100)),
            stack_trace=draw(st.text(max_size=200)),
            counterexample=draw(st.one_of(st.none(), st.text(max_size=100)))
        ))

    return TestResult(
        suite_name=draw(st.text(min_size=1, max_size=50)),
        total_tests=total_tests,
        passed_tests=passed_tests,
        failed_tests=failed_tests,
        skipped_tests=skipped_tests,
        execution_time=draw(st.floats(min_value=0.0, max_value=100.0)),
        failures=failures
    )


@st.composite
def validation_config_strategy(draw):
    """
    \brief           生成验证配置策略
    """
    return ValidationConfig(
        build_dir=draw(st.text(min_size=1, max_size=50)),
        source_dir=draw(st.text(min_size=1, max_size=50)),
        coverage_enabled=draw(st.booleans()),
        coverage_threshold=draw(st.floats(min_value=0.0, max_value=1.0)),
        test_timeout=draw(st.integers(min_value=1, max_value=1000)),
        parallel_jobs=draw(st.integers(min_value=0, max_value=16)),
        platforms=draw(st.lists(st.text(min_size=1, max_size=20), min_size=1, max_size=5)),
        fail_fast=draw(st.booleans()),
        report_dir=draw(st.text(min_size=1, max_size=50)),
        verbose=draw(st.booleans())
    )


class TestValidationControllerProperties:
    """
    \brief           验证控制器属性测试类
    """

    @settings(max_examples=100)
    @given(
        test_results=st.lists(test_result_strategy(), min_size=1, max_size=5),
        fail_fast=st.booleans()
    )
    def test_property_test_failure_propagation(self, test_results, fail_fast):
        """
        Feature: system-validation, Property 4: 测试失败传播

        *对于任何*测试失败，验证脚本应该立即停止执行（除非配置为继续），
        记录失败详情，并返回非零退出码。

        **验证需求: 8.6**
        """
        # 创建临时目录
        with tempfile.TemporaryDirectory() as temp_dir:
            config = ValidationConfig(
                build_dir=str(Path(temp_dir) / "build"),
                source_dir=str(Path(temp_dir) / "source"),
                coverage_enabled=False,
                fail_fast=fail_fast,
                report_dir=str(Path(temp_dir) / "reports"),
                verbose=False
            )

            # 创建mock的测试执行器
            with patch('validation.validation_controller.TestExecutor') as MockExecutor, \
                 patch('validation.validation_controller.ReportGenerator') as MockReportGen:

                # 设置mock行为
                mock_executor = MockExecutor.return_value
                mock_executor.build_tests.return_value = True

                # 模拟测试结果
                test_results_iter = iter(test_results)

                def get_next_result():
                    try:
                        return next(test_results_iter)
                    except StopIteration:
                        return TestResult(
                            suite_name="empty",
                            total_tests=0,
                            passed_tests=0,
                            failed_tests=0,
                            skipped_tests=0,
                            execution_time=0.0,
                            failures=[]
                        )

                mock_executor.run_unit_tests.side_effect = lambda: get_next_result()
                mock_executor.run_property_tests.side_effect = lambda: get_next_result()
                mock_executor.run_integration_tests.side_effect = lambda: get_next_result()

                mock_report_gen = MockReportGen.return_value
                mock_report_gen.generate_summary_report.return_value = "summary.txt"
                mock_report_gen.generate_failure_report.return_value = "failures.txt"
                mock_report_gen.generate_performance_report.return_value = "performance.txt"
                mock_report_gen.generate_junit_report.return_value = "junit.xml"
                mock_report_gen.generate_html_report.return_value = "report.html"

                controller = ValidationController(config)

                # 检查是否有失败的测试
                has_failures = any(r.failed_tests > 0 for r in test_results)

                if fail_fast and has_failures:
                    # 如果启用fail_fast且有失败，应该抛出异常
                    with pytest.raises(ValidationControllerError):
                        controller.run_all_tests()
                else:
                    # 否则应该正常完成
                    result = controller.run_all_tests()

                    # 验证结果的成功状态
                    if has_failures:
                        assert not result.success, \
                            "有测试失败时，验证结果应该标记为失败"
                    else:
                        assert result.success, \
                            "所有测试通过时，验证结果应该标记为成功"

    @settings(max_examples=50)
    @given(
        build_success=st.booleans(),
        fail_fast=st.booleans()
    )
    def test_property_build_failure_handling(self, build_success, fail_fast):
        """
        测试构建失败的处理

        *对于任何*构建失败，验证控制器应该记录错误并停止执行
        """
        with tempfile.TemporaryDirectory() as temp_dir:
            config = ValidationConfig(
                build_dir=str(Path(temp_dir) / "build"),
                source_dir=str(Path(temp_dir) / "source"),
                coverage_enabled=False,
                fail_fast=fail_fast,
                report_dir=str(Path(temp_dir) / "reports"),
                verbose=False
            )

            with patch('validation.validation_controller.TestExecutor') as MockExecutor, \
                 patch('validation.validation_controller.ReportGenerator'):

                mock_executor = MockExecutor.return_value
                mock_executor.build_tests.return_value = build_success

                controller = ValidationController(config)

                if not build_success:
                    # 构建失败应该抛出异常
                    with pytest.raises(ValidationControllerError):
                        controller.run_all_tests()
                else:
                    # 构建成功应该继续执行（但可能因为其他原因失败）
                    # 这里我们只验证不会因为构建失败而抛出异常
                    pass

    @settings(max_examples=50)
    @given(
        coverage_enabled=st.booleans(),
        coverage_threshold=st.floats(min_value=0.0, max_value=1.0),
        actual_coverage=st.floats(min_value=0.0, max_value=1.0),
        fail_fast=st.booleans()
    )
    def test_property_coverage_threshold_enforcement(
        self,
        coverage_enabled,
        coverage_threshold,
        actual_coverage,
        fail_fast
    ):
        """
        测试覆盖率阈值强制

        *对于任何*覆盖率数据，如果低于阈值且启用fail_fast，
        验证应该失败
        """
        with tempfile.TemporaryDirectory() as temp_dir:
            config = ValidationConfig(
                build_dir=str(Path(temp_dir) / "build"),
                source_dir=str(Path(temp_dir) / "source"),
                coverage_enabled=coverage_enabled,
                coverage_threshold=coverage_threshold,
                fail_fast=fail_fast,
                report_dir=str(Path(temp_dir) / "reports"),
                verbose=False
            )

            with patch('validation.validation_controller.TestExecutor') as MockExecutor, \
                 patch('validation.validation_controller.CoverageAnalyzer') as MockCoverage, \
                 patch('validation.validation_controller.ReportGenerator') as MockReportGen:

                # 设置mock行为
                mock_executor = MockExecutor.return_value
                mock_executor.build_tests.return_value = True
                mock_executor.run_unit_tests.return_value = TestResult(
                    suite_name="unit",
                    total_tests=10,
                    passed_tests=10,
                    failed_tests=0,
                    skipped_tests=0,
                    execution_time=1.0,
                    failures=[]
                )
                mock_executor.run_property_tests.return_value = TestResult(
                    suite_name="property",
                    total_tests=5,
                    passed_tests=5,
                    failed_tests=0,
                    skipped_tests=0,
                    execution_time=0.5,
                    failures=[]
                )
                mock_executor.run_integration_tests.return_value = TestResult(
                    suite_name="integration",
                    total_tests=3,
                    passed_tests=3,
                    failed_tests=0,
                    skipped_tests=0,
                    execution_time=0.3,
                    failures=[]
                )

                if coverage_enabled:
                    mock_coverage_analyzer = MockCoverage.return_value
                    mock_coverage_analyzer.collect_coverage_data.return_value = CoverageData(
                        line_coverage=actual_coverage,
                        branch_coverage=actual_coverage,
                        function_coverage=actual_coverage,
                        uncovered_lines=[],
                        uncovered_branches=[]
                    )
                    mock_coverage_analyzer.check_threshold.return_value = (
                        actual_coverage >= coverage_threshold
                    )
                    mock_coverage_analyzer.generate_coverage_report.return_value = "coverage.html"
                    mock_coverage_analyzer.identify_uncovered_regions.return_value = []

                mock_report_gen = MockReportGen.return_value
                mock_report_gen.generate_summary_report.return_value = "summary.txt"
                mock_report_gen.generate_failure_report.return_value = "failures.txt"
                mock_report_gen.generate_performance_report.return_value = "performance.txt"
                mock_report_gen.generate_junit_report.return_value = "junit.xml"
                mock_report_gen.generate_html_report.return_value = "report.html"

                controller = ValidationController(config)

                # 检查是否应该失败
                should_fail = (
                    coverage_enabled and
                    fail_fast and
                    actual_coverage < coverage_threshold
                )

                if should_fail:
                    with pytest.raises(ValidationControllerError):
                        controller.run_all_tests()
                else:
                    result = controller.run_all_tests()

                    # 验证结果
                    if coverage_enabled and actual_coverage < coverage_threshold:
                        assert not result.success, \
                            "覆盖率未达标时，验证结果应该标记为失败"
                    else:
                        assert result.success, \
                            "覆盖率达标或未启用时，验证结果应该标记为成功"


class TestValidationControllerUnit:
    """
    \brief           验证控制器单元测试类
    """

    def test_controller_initialization(self):
        """
        测试控制器初始化
        """
        with tempfile.TemporaryDirectory() as temp_dir:
            config = ValidationConfig(
                build_dir=str(Path(temp_dir) / "build"),
                source_dir=str(Path(temp_dir) / "source"),
                report_dir=str(Path(temp_dir) / "reports")
            )

            controller = ValidationController(config)

            assert controller.config == config
            assert controller.test_executor is not None
            assert controller.report_generator is not None

    def test_controller_with_coverage_enabled(self):
        """
        测试启用覆盖率的控制器初始化
        """
        with tempfile.TemporaryDirectory() as temp_dir:
            config = ValidationConfig(
                build_dir=str(Path(temp_dir) / "build"),
                source_dir=str(Path(temp_dir) / "source"),
                coverage_enabled=True,
                report_dir=str(Path(temp_dir) / "reports")
            )

            controller = ValidationController(config)

            assert controller.coverage_analyzer is not None

    def test_controller_with_coverage_disabled(self):
        """
        测试禁用覆盖率的控制器初始化
        """
        with tempfile.TemporaryDirectory() as temp_dir:
            config = ValidationConfig(
                build_dir=str(Path(temp_dir) / "build"),
                source_dir=str(Path(temp_dir) / "source"),
                coverage_enabled=False,
                report_dir=str(Path(temp_dir) / "reports")
            )

            controller = ValidationController(config)

            assert controller.coverage_analyzer is None


if __name__ == '__main__':
    pytest.main([__file__, '-v'])

