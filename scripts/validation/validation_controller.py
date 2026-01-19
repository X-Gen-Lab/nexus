r"""
\file            validation_controller.py
\brief           系统验证框架的主验证控制器
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         协调整个验证流程，集成测试执行器、覆盖率分析器和报告生成器。
                 实现错误处理、恢复机制和fail-fast模式。
"""

import sys
import logging
from pathlib import Path
from typing import Optional, List
from datetime import datetime

from .models import ValidationResult, ValidationConfig, TestResult, CoverageData
from .test_executor import TestExecutor, TestExecutorError
from .coverage_analyzer import CoverageAnalyzer, CoverageAnalyzerError
from .report_generator import ReportGenerator, ReportGenerationError
from .utils import safe_print


class ValidationControllerError(Exception):
    r"""
    \brief           验证控制器错误异常
    """
    pass


class ValidationController:
    r"""
    \brief           验证控制器
    \details         协调测试执行、覆盖率分析和报告生成的整个验证流程
    """

    def __init__(self, config: ValidationConfig):
        r"""
        \brief           初始化验证控制器
        \param[in]       config: 验证配置对象
        """
        self.config = config
        self.logger = self._setup_logger()

        # 初始化组件
        self.test_executor = TestExecutor(config)
        self.coverage_analyzer = CoverageAnalyzer(config) if config.coverage_enabled else None
        self.report_generator = ReportGenerator(config)

        # 验证结果
        self._test_results: List[TestResult] = []
        self._coverage_data: Optional[CoverageData] = None
        self._start_time: Optional[datetime] = None
        self._end_time: Optional[datetime] = None

    def _setup_logger(self) -> logging.Logger:
        r"""
        \brief           设置日志记录器
        \return          配置好的日志记录器
        """
        logger = logging.getLogger('validation_controller')

        # 设置日志级别
        if self.config.verbose:
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.INFO)

        # 创建控制台处理器
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(logging.DEBUG if self.config.verbose else logging.INFO)

        # 创建格式化器
        formatter = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
        console_handler.setFormatter(formatter)

        # 添加处理器
        logger.addHandler(console_handler)

        # 创建文件处理器
        log_dir = Path(self.config.report_dir)
        log_dir.mkdir(parents=True, exist_ok=True)
        log_file = log_dir / "validation.log"

        file_handler = logging.FileHandler(log_file, mode='w', encoding='utf-8')
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

        return logger

    def run_all_tests(self) -> ValidationResult:
        r"""
        \brief           运行所有测试套件
        \return          验证结果对象
        \details         按顺序执行单元测试、属性测试和集成测试，
                         收集覆盖率数据并生成报告
        """
        self._start_time = datetime.now()
        self.logger.info("=" * 80)
        self.logger.info("开始系统验证")
        self.logger.info("=" * 80)

        try:
            # 步骤 1: 构建测试
            self._build_tests()

            # 步骤 2: 运行测试
            self._run_tests()

            # 步骤 3: 收集覆盖率
            if self.coverage_analyzer:
                self._collect_coverage()

            # 步骤 4: 生成报告
            self._generate_reports()

            # 步骤 5: 检查结果
            success = self._check_results()

            self._end_time = datetime.now()
            execution_time = (self._end_time - self._start_time).total_seconds()

            # 创建验证结果
            validation_result = ValidationResult(
                success=success,
                test_results=self._test_results,
                coverage_data=self._coverage_data,
                execution_time=execution_time,
                timestamp=self._start_time
            )

            self._log_final_summary(validation_result)

            return validation_result

        except TestExecutorError as e:
            self.logger.error(f"测试执行失败: {str(e)}")
            self._handle_test_executor_error(e)
            raise ValidationControllerError(f"测试执行失败: {str(e)}")

        except CoverageAnalyzerError as e:
            self.logger.error(f"覆盖率分析失败: {str(e)}")
            self._handle_coverage_error(e)
            raise ValidationControllerError(f"覆盖率分析失败: {str(e)}")

        except ReportGenerationError as e:
            self.logger.error(f"报告生成失败: {str(e)}")
            self._handle_report_error(e)
            raise ValidationControllerError(f"报告生成失败: {str(e)}")

        except Exception as e:
            self.logger.error(f"验证过程中发生未预期的错误: {str(e)}")
            raise ValidationControllerError(f"验证失败: {str(e)}")

    def _build_tests(self) -> None:
        r"""
        \brief           构建测试可执行文件
        \details         使用测试执行器构建所有测试
        """
        self.logger.info("\n" + "=" * 80)
        self.logger.info("步骤 1: 构建测试")
        self.logger.info("=" * 80)

        if not self.test_executor.build_tests():
            error_msg = "测试构建失败"
            self.logger.error(error_msg)

            # 记录构建日志
            build_log = self.test_executor.get_build_log()
            if build_log:
                self.logger.debug("构建日志:")
                for line in build_log:
                    self.logger.debug(line)

            raise TestExecutorError(error_msg)

        self.logger.info("✓ 测试构建成功")

    def _run_tests(self) -> None:
        r"""
        \brief           运行所有测试套件
        \details         按顺序运行单元测试、属性测试和集成测试
        """
        self.logger.info("\n" + "=" * 80)
        self.logger.info("步骤 2: 运行测试")
        self.logger.info("=" * 80)

        # 运行单元测试
        self.logger.info("\n--- 运行单元测试 ---")
        try:
            unit_result = self.test_executor.run_unit_tests()
            self._test_results.append(unit_result)
            self._log_test_result(unit_result)

            if self.config.fail_fast and unit_result.failed_tests > 0:
                raise TestExecutorError(
                    f"单元测试失败: {unit_result.failed_tests} 个测试未通过"
                )

        except TestExecutorError as e:
            if self.config.fail_fast:
                raise
            self.logger.warning(f"单元测试执行出错: {str(e)}")

        # 运行属性测试
        self.logger.info("\n--- 运行属性测试 ---")
        try:
            property_result = self.test_executor.run_property_tests()
            self._test_results.append(property_result)
            self._log_test_result(property_result)

            if self.config.fail_fast and property_result.failed_tests > 0:
                raise TestExecutorError(
                    f"属性测试失败: {property_result.failed_tests} 个测试未通过"
                )

        except TestExecutorError as e:
            if self.config.fail_fast:
                raise
            self.logger.warning(f"属性测试执行出错: {str(e)}")

        # 运行集成测试
        self.logger.info("\n--- 运行集成测试 ---")
        try:
            integration_result = self.test_executor.run_integration_tests()
            self._test_results.append(integration_result)
            self._log_test_result(integration_result)

            if self.config.fail_fast and integration_result.failed_tests > 0:
                raise TestExecutorError(
                    f"集成测试失败: {integration_result.failed_tests} 个测试未通过"
                )

        except TestExecutorError as e:
            if self.config.fail_fast:
                raise
            self.logger.warning(f"集成测试执行出错: {str(e)}")

        self.logger.info("\n✓ 所有测试套件执行完成")

    def _collect_coverage(self) -> None:
        r"""
        \brief           收集覆盖率数据
        \details         使用覆盖率分析器收集和分析覆盖率数据
        """
        if not self.coverage_analyzer:
            return

        self.logger.info("\n" + "=" * 80)
        self.logger.info("步骤 3: 收集覆盖率数据")
        self.logger.info("=" * 80)

        try:
            # 收集覆盖率数据
            self._coverage_data = self.coverage_analyzer.collect_coverage_data()

            # 生成覆盖率报告
            self.logger.info("\n--- 生成覆盖率报告 ---")
            html_report = self.coverage_analyzer.generate_coverage_report('html')
            if html_report:
                self.logger.info(f"HTML 覆盖率报告: {html_report}")

            xml_report = self.coverage_analyzer.generate_coverage_report('xml')
            if xml_report:
                self.logger.info(f"XML 覆盖率报告: {xml_report}")

            # 检查覆盖率阈值
            self.logger.info("\n--- 检查覆盖率阈值 ---")
            threshold_met = self.coverage_analyzer.check_threshold(
                self.config.coverage_threshold
            )

            if not threshold_met:
                self.logger.warning(
                    f"覆盖率未达到阈值 {self.config.coverage_threshold * 100:.2f}%"
                )

                # 识别未覆盖区域
                uncovered = self.coverage_analyzer.identify_uncovered_regions()
                self.logger.warning(f"未覆盖区域数量: {len(uncovered)}")

                if self.config.fail_fast:
                    raise CoverageAnalyzerError(
                        f"覆盖率未达到阈值 {self.config.coverage_threshold * 100:.2f}%"
                    )
            else:
                self.logger.info("✓ 覆盖率达到阈值")

        except CoverageAnalyzerError as e:
            if self.config.fail_fast:
                raise
            self.logger.warning(f"覆盖率收集出错: {str(e)}")

    def _generate_reports(self) -> None:
        r"""
        \brief           生成所有报告
        \details         生成汇总报告、失败报告、性能报告、JUnit报告和HTML报告
        """
        self.logger.info("\n" + "=" * 80)
        self.logger.info("步骤 4: 生成报告")
        self.logger.info("=" * 80)

        try:
            # 生成汇总报告
            self.logger.info("\n--- 生成汇总报告 ---")
            summary_report = self.report_generator.generate_summary_report(
                self._test_results
            )
            self.logger.info(f"汇总报告: {summary_report}")

            # 生成失败报告
            self.logger.info("\n--- 生成失败报告 ---")
            failure_report = self.report_generator.generate_failure_report(
                self._test_results
            )
            self.logger.info(f"失败报告: {failure_report}")

            # 生成性能报告
            self.logger.info("\n--- 生成性能报告 ---")
            performance_report = self.report_generator.generate_performance_report(
                self._test_results
            )
            self.logger.info(f"性能报告: {performance_report}")

            # 生成JUnit报告
            self.logger.info("\n--- 生成JUnit报告 ---")
            junit_report = self.report_generator.generate_junit_report(
                self._test_results
            )
            self.logger.info(f"JUnit报告: {junit_report}")

            # 生成HTML报告
            self.logger.info("\n--- 生成HTML报告 ---")
            validation_result = ValidationResult(
                success=True,  # 临时值，稍后会更新
                test_results=self._test_results,
                coverage_data=self._coverage_data,
                execution_time=0.0,  # 临时值
                timestamp=self._start_time or datetime.now()
            )
            html_report = self.report_generator.generate_html_report(validation_result)
            self.logger.info(f"HTML报告: {html_report}")

            self.logger.info("\n✓ 所有报告生成完成")

        except ReportGenerationError as e:
            self.logger.warning(f"报告生成出错: {str(e)}")
            if self.config.fail_fast:
                raise

    def _check_results(self) -> bool:
        r"""
        \brief           检查验证结果
        \return          验证是否成功
        \details         检查是否有测试失败或覆盖率未达标
        """
        self.logger.info("\n" + "=" * 80)
        self.logger.info("步骤 5: 检查结果")
        self.logger.info("=" * 80)

        # 检查测试失败
        total_failures = sum(r.failed_tests for r in self._test_results)

        if total_failures > 0:
            self.logger.error(f"✗ {total_failures} 个测试失败")
            return False

        # 检查覆盖率阈值
        if self.coverage_analyzer and self._coverage_data:
            threshold_met = self.coverage_analyzer.check_threshold(
                self.config.coverage_threshold
            )
            if not threshold_met:
                self.logger.error(
                    f"✗ 覆盖率未达到阈值 {self.config.coverage_threshold * 100:.2f}%"
                )
                return False

        self.logger.info("✓ 所有检查通过")
        return True

    def _log_test_result(self, result: TestResult) -> None:
        r"""
        \brief           记录测试结果
        \param[in]       result: 测试结果对象
        """
        self.logger.info(f"测试套件: {result.suite_name}")
        self.logger.info(f"  总测试数: {result.total_tests}")
        self.logger.info(f"  通过:     {result.passed_tests}")
        self.logger.info(f"  失败:     {result.failed_tests}")
        self.logger.info(f"  跳过:     {result.skipped_tests}")
        self.logger.info(f"  执行时间: {result.execution_time:.3f} 秒")

        if result.failed_tests > 0:
            self.logger.warning(f"  ✗ {result.failed_tests} 个测试失败")
        else:
            self.logger.info("  ✓ 所有测试通过")

    def _log_final_summary(self, validation_result: ValidationResult) -> None:
        r"""
        \brief           记录最终汇总信息
        \param[in]       validation_result: 验证结果对象
        """
        self.logger.info("\n" + "=" * 80)
        self.logger.info("验证完成")
        self.logger.info("=" * 80)

        total_tests = sum(r.total_tests for r in validation_result.test_results)
        passed_tests = sum(r.passed_tests for r in validation_result.test_results)
        failed_tests = sum(r.failed_tests for r in validation_result.test_results)
        skipped_tests = sum(r.skipped_tests for r in validation_result.test_results)

        self.logger.info(f"总测试数:   {total_tests}")
        self.logger.info(f"通过:       {passed_tests}")
        self.logger.info(f"失败:       {failed_tests}")
        self.logger.info(f"跳过:       {skipped_tests}")
        self.logger.info(f"总执行时间: {validation_result.execution_time:.3f} 秒")

        if validation_result.coverage_data:
            cov = validation_result.coverage_data
            self.logger.info(f"行覆盖率:   {cov.line_coverage * 100:.2f}%")
            self.logger.info(f"分支覆盖率: {cov.branch_coverage * 100:.2f}%")
            self.logger.info(f"函数覆盖率: {cov.function_coverage * 100:.2f}%")

        if validation_result.success:
            self.logger.info("\n✓ 验证成功")
        else:
            self.logger.error("\n✗ 验证失败")

        self.logger.info("=" * 80)

    def _handle_test_executor_error(self, error: TestExecutorError) -> None:
        r"""
        \brief           处理测试执行器错误
        \param[in]       error: 测试执行器错误
        \details         记录错误信息并尝试恢复
        """
        self.logger.error(f"测试执行器错误: {str(error)}")

        # 尝试保存已收集的测试结果
        if self._test_results:
            try:
                self.logger.info("尝试保存已收集的测试结果...")
                self.report_generator.generate_summary_report(self._test_results)
                self.report_generator.generate_failure_report(self._test_results)
                self.logger.info("✓ 部分测试结果已保存")
            except Exception as e:
                self.logger.error(f"保存测试结果失败: {str(e)}")

    def _handle_coverage_error(self, error: CoverageAnalyzerError) -> None:
        r"""
        \brief           处理覆盖率分析器错误
        \param[in]       error: 覆盖率分析器错误
        \details         记录错误信息并尝试恢复
        """
        self.logger.error(f"覆盖率分析器错误: {str(error)}")

        # 覆盖率错误不应阻止报告生成
        self.logger.info("继续生成测试报告（不包含覆盖率数据）...")

    def _handle_report_error(self, error: ReportGenerationError) -> None:
        r"""
        \brief           处理报告生成器错误
        \param[in]       error: 报告生成器错误
        \details         记录错误信息
        """
        self.logger.error(f"报告生成器错误: {str(error)}")
        self.logger.warning("部分报告可能未生成")

    def get_test_results(self) -> List[TestResult]:
        r"""
        \brief           获取测试结果列表
        \return          测试结果列表
        """
        return self._test_results.copy()

    def get_coverage_data(self) -> Optional[CoverageData]:
        r"""
        \brief           获取覆盖率数据
        \return          覆盖率数据对象，如果未收集则返回None
        """
        return self._coverage_data

