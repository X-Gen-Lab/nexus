"""
\file            test_coverage_analyzer.py
\brief           覆盖率分析器的属性测试
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         使用 Hypothesis 进行基于属性的测试，验证覆盖率分析器的
                 正确性属性。
"""

import pytest
from hypothesis import given, settings, strategies as st
from pathlib import Path
import tempfile
import os

from scripts.validation.coverage_analyzer import CoverageAnalyzer, CoverageAnalyzerError
from scripts.validation.models import ValidationConfig, CoverageData, CodeLocation


class TestCoverageAnalyzerProperties:
    """
    \brief           覆盖率分析器属性测试类
    """

    @settings(max_examples=100)
    @given(
        threshold=st.floats(min_value=0.0, max_value=1.0),
        line_coverage=st.floats(min_value=0.0, max_value=1.0),
        branch_coverage=st.floats(min_value=0.0, max_value=1.0),
        function_coverage=st.floats(min_value=0.0, max_value=1.0)
    )
    def test_property_threshold_enforcement(
        self,
        threshold: float,
        line_coverage: float,
        branch_coverage: float,
        function_coverage: float
    ):
        """
        Feature: system-validation, Property 2: 覆盖率阈值强制

        *对于任何*子系统，如果其测试覆盖率低于配置的阈值，
        则验证应该失败并标记未覆盖的代码区域。

        **验证需求: 7.7**
        """
        # 创建临时配置
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                coverage_threshold=threshold,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 创建模拟的覆盖率数据
            coverage_data = CoverageData(
                line_coverage=line_coverage,
                branch_coverage=branch_coverage,
                function_coverage=function_coverage,
                uncovered_lines=[],
                uncovered_branches=[]
            )

            # 设置覆盖率数据
            analyzer._coverage_data = coverage_data

            # 检查阈值
            result = analyzer.check_threshold(threshold)

            # 属性验证：只有当所有覆盖率类型都达到阈值时，结果才应该为 True
            expected = (
                line_coverage >= threshold and
                branch_coverage >= threshold and
                function_coverage >= threshold
            )

            assert result == expected, (
                f"阈值检查结果不正确: threshold={threshold:.2f}, "
                f"line={line_coverage:.2f}, branch={branch_coverage:.2f}, "
                f"function={function_coverage:.2f}, expected={expected}, got={result}"
            )

    @settings(max_examples=100)
    @given(
        line_coverage=st.floats(min_value=0.0, max_value=1.0),
        branch_coverage=st.floats(min_value=0.0, max_value=1.0),
        function_coverage=st.floats(min_value=0.0, max_value=1.0),
        uncovered_lines_count=st.integers(min_value=0, max_value=100),
        uncovered_branches_count=st.integers(min_value=0, max_value=100)
    )
    def test_property_coverage_data_consistency(
        self,
        line_coverage: float,
        branch_coverage: float,
        function_coverage: float,
        uncovered_lines_count: int,
        uncovered_branches_count: int
    ):
        """
        Feature: system-validation, Property 7: 覆盖率数据一致性

        *对于任何*代码文件，行覆盖率、分支覆盖率和函数覆盖率数据应该
        相互一致（例如，如果一个函数被覆盖，则其至少一行代码应该被覆盖）。

        **验证需求: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6**
        """
        from hypothesis import assume

        # 确保数据一致性：如果覆盖率为 100%，则未覆盖数量必须为 0
        assume(not (line_coverage == 1.0 and uncovered_lines_count > 0))
        assume(not (branch_coverage == 1.0 and uncovered_branches_count > 0))

        # 创建临时配置
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 创建模拟的未覆盖位置
            uncovered_lines = [
                CodeLocation(
                    file_path=f"test_file_{i}.c",
                    line_number=i + 1
                )
                for i in range(uncovered_lines_count)
            ]

            uncovered_branches = [
                CodeLocation(
                    file_path=f"test_file_{i}.c",
                    line_number=i + 1
                )
                for i in range(uncovered_branches_count)
            ]

            # 创建覆盖率数据
            coverage_data = CoverageData(
                line_coverage=line_coverage,
                branch_coverage=branch_coverage,
                function_coverage=function_coverage,
                uncovered_lines=uncovered_lines,
                uncovered_branches=uncovered_branches
            )

            # 设置覆盖率数据
            analyzer._coverage_data = coverage_data

            # 属性 1: 覆盖率值应该在 0.0 到 1.0 之间
            assert 0.0 <= coverage_data.line_coverage <= 1.0
            assert 0.0 <= coverage_data.branch_coverage <= 1.0
            assert 0.0 <= coverage_data.function_coverage <= 1.0

            # 属性 2: 未覆盖区域的数量应该与覆盖率一致
            # 如果覆盖率为 1.0，则不应该有未覆盖的行或分支
            if line_coverage == 1.0:
                assert len(coverage_data.uncovered_lines) == 0, (
                    "行覆盖率为 100% 时不应该有未覆盖的行"
                )

            if branch_coverage == 1.0:
                assert len(coverage_data.uncovered_branches) == 0, (
                    "分支覆盖率为 100% 时不应该有未覆盖的分支"
                )

            # 属性 3: 识别未覆盖区域应该返回所有未覆盖的位置
            uncovered_regions = analyzer.identify_uncovered_regions()

            # 未覆盖区域的数量应该至少等于未覆盖行的数量
            assert len(uncovered_regions) >= len(uncovered_lines), (
                f"未覆盖区域数量 ({len(uncovered_regions)}) 应该至少等于"
                f"未覆盖行数量 ({len(uncovered_lines)})"
            )

            # 属性 4: 获取覆盖率摘要应该返回一致的数据
            summary = analyzer.get_coverage_summary()

            assert summary["line_coverage"] == line_coverage
            assert summary["branch_coverage"] == branch_coverage
            assert summary["function_coverage"] == function_coverage
            assert summary["uncovered_lines_count"] == uncovered_lines_count
            assert summary["uncovered_branches_count"] == uncovered_branches_count

    @settings(max_examples=100)
    @given(
        threshold=st.floats(min_value=0.0, max_value=1.0)
    )
    def test_property_threshold_boundary(self, threshold: float):
        """
        测试覆盖率阈值的边界条件

        验证当覆盖率恰好等于阈值时，检查应该通过。
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                coverage_threshold=threshold,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 创建覆盖率恰好等于阈值的数据
            coverage_data = CoverageData(
                line_coverage=threshold,
                branch_coverage=threshold,
                function_coverage=threshold,
                uncovered_lines=[],
                uncovered_branches=[]
            )

            analyzer._coverage_data = coverage_data

            # 边界条件：覆盖率等于阈值应该通过
            result = analyzer.check_threshold(threshold)
            assert result is True, (
                f"覆盖率等于阈值 {threshold:.2f} 时应该通过检查"
            )

    def test_property_uncovered_regions_uniqueness(self):
        """
        测试未覆盖区域的唯一性

        验证 identify_uncovered_regions 不会返回重复的位置。
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 创建有重复位置的覆盖率数据
            uncovered_lines = [
                CodeLocation(file_path="test.c", line_number=10),
                CodeLocation(file_path="test.c", line_number=20),
            ]

            uncovered_branches = [
                CodeLocation(file_path="test.c", line_number=10),  # 与行重复
                CodeLocation(file_path="test.c", line_number=30),
            ]

            coverage_data = CoverageData(
                line_coverage=0.5,
                branch_coverage=0.5,
                function_coverage=0.5,
                uncovered_lines=uncovered_lines,
                uncovered_branches=uncovered_branches
            )

            analyzer._coverage_data = coverage_data

            # 获取未覆盖区域
            uncovered_regions = analyzer.identify_uncovered_regions()

            # 验证没有重复
            locations_set = {
                (loc.file_path, loc.line_number)
                for loc in uncovered_regions
            }

            assert len(uncovered_regions) == len(locations_set), (
                "未覆盖区域列表中不应该有重复的位置"
            )

    def test_error_when_coverage_not_collected(self):
        """
        测试在未收集覆盖率数据时的错误处理
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 未收集覆盖率数据时应该抛出异常
            with pytest.raises(CoverageAnalyzerError, match="未收集覆盖率数据"):
                analyzer.check_threshold(0.8)

            with pytest.raises(CoverageAnalyzerError, match="未收集覆盖率数据"):
                analyzer.identify_uncovered_regions()

            with pytest.raises(CoverageAnalyzerError, match="未收集覆盖率数据"):
                analyzer.get_coverage_summary()


class TestCoverageAnalyzerUnitTests:
    """
    \brief           覆盖率分析器单元测试类
    """

    def test_enable_coverage(self):
        """
        测试启用覆盖率收集
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)
            analyzer.enable_coverage()

            # 应该不抛出异常

    def test_disable_coverage(self):
        """
        测试禁用覆盖率收集
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=False,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)
            analyzer.enable_coverage()

            # 应该不抛出异常

    def test_get_coverage_data_none(self):
        """
        测试获取未收集的覆盖率数据
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 未收集数据时应该返回 None
            assert analyzer.get_coverage_data() is None

    def test_coverage_data_after_collection(self):
        """
        测试收集后的覆盖率数据
        """
        with tempfile.TemporaryDirectory() as tmpdir:
            config = ValidationConfig(
                build_dir=tmpdir,
                source_dir=tmpdir,
                coverage_enabled=True,
                verbose=False
            )

            analyzer = CoverageAnalyzer(config)

            # 手动设置覆盖率数据（模拟收集）
            coverage_data = CoverageData(
                line_coverage=0.85,
                branch_coverage=0.75,
                function_coverage=0.90,
                uncovered_lines=[],
                uncovered_branches=[]
            )

            analyzer._coverage_data = coverage_data

            # 获取数据应该返回设置的数据
            retrieved_data = analyzer.get_coverage_data()
            assert retrieved_data is not None
            assert retrieved_data.line_coverage == 0.85
            assert retrieved_data.branch_coverage == 0.75
            assert retrieved_data.function_coverage == 0.90

