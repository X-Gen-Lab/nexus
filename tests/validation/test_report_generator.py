"""
\\file            test_report_generator.py
\\brief           报告生成器的测试
\\author          Nexus Team
\\version         1.0.0
\\date            2026-01-18

\\copyright       Copyright (c) 2026 Nexus Team

\\details         包含报告生成器的单元测试和属性测试
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from datetime import datetime
from hypothesis import given, settings, strategies as st
import xml.etree.ElementTree as ET
import json

import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "scripts"))

from validation.report_generator import ReportGenerator, ReportGenerationError
from validation.models import (
    ValidationConfig,
    ValidationResult,
    TestResult,
    TestFailure,
    CoverageData,
    CodeLocation
)


@pytest.fixture
def temp_report_dir():
    """创建临时报告目录"""
    temp_dir = tempfile.mkdtemp()
    yield temp_dir
    shutil.rmtree(temp_dir)


@pytest.fixture
def config(temp_report_dir):
    """创建测试配置"""
    return ValidationConfig(
        build_dir="build",
        source_dir=".",
        report_dir=temp_report_dir
    )


@pytest.fixture
def generator(config):
    """创建报告生成器实例"""
    return ReportGenerator(config)


@pytest.fixture
def sample_test_results():
    """创建示例测试结果"""
    return [
        TestResult(
            suite_name="unit_tests",
            total_tests=10,
            passed_tests=8,
            failed_tests=2,
            skipped_tests=0,
            execution_time=1.5,
            failures=[
                TestFailure(
                    test_name="test_foo",
                    error_message="Assertion failed",
                    stack_trace="at line 42",
                    counterexample=None
                ),
                TestFailure(
                    test_name="test_bar",
                    error_message="Property violated",
                    stack_trace="at line 100",
                    counterexample="x=5, y=10"
                )
            ]
        ),
        TestResult(
            suite_name="integration_tests",
            total_tests=5,
            passed_tests=5,
            failed_tests=0,
            skipped_tests=0,
            execution_time=2.3
        )
    ]


@pytest.fixture
def sample_coverage_data():
    """创建示例覆盖率数据"""
    return CoverageData(
        line_coverage=0.85,
        branch_coverage=0.75,
        function_coverage=0.90,
        uncovered_lines=[
            CodeLocation(file_path="test.c", line_number=42),
            CodeLocation(file_path="test.c", line_number=43)
        ],
        uncovered_branches=[
            CodeLocation(file_path="test.c", line_number=50)
        ]
    )


class TestReportGenerator:
    """报告生成器单元测试"""

    def test_initialization(self, config):
        """测试报告生成器初始化"""
        generator = ReportGenerator(config)
        assert generator.config == config
        assert generator.report_dir.exists()

    def test_generate_summary_report(self, generator, sample_test_results):
        """测试生成汇总报告"""
        report_path = generator.generate_summary_report(sample_test_results)
        assert Path(report_path).exists()

        # 验证文本报告内容
        with open(report_path, 'r', encoding='utf-8') as f:
            content = f.read()
            assert "测试汇总报告" in content
            assert "总测试数:" in content
            assert "15" in content  # 10 + 5
            assert "通过:" in content
            assert "13" in content  # 8 + 5

        # 验证JSON报告存在
        json_path = Path(report_path).parent / "summary.json"
        assert json_path.exists()

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            assert data["summary"]["total_tests"] == 15
            assert data["summary"]["passed_tests"] == 13
            assert data["summary"]["failed_tests"] == 2

    def test_generate_failure_report(self, generator, sample_test_results):
        """测试生成失败报告"""
        report_path = generator.generate_failure_report(sample_test_results)
        assert Path(report_path).exists()

        with open(report_path, 'r', encoding='utf-8') as f:
            content = f.read()
            assert "失败测试详情报告" in content
            assert "test_foo" in content
            assert "test_bar" in content
            assert "Assertion failed" in content
            assert "x=5, y=10" in content  # 反例

        # 验证JSON报告
        json_path = Path(report_path).parent / "failures.json"
        assert json_path.exists()

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            assert data["total_failures"] == 2
            assert len(data["failures"]) == 2

    def test_generate_failure_report_no_failures(self, generator):
        """测试没有失败时的失败报告"""
        results = [
            TestResult(
                suite_name="all_pass",
                total_tests=5,
                passed_tests=5,
                failed_tests=0,
                skipped_tests=0,
                execution_time=1.0
            )
        ]

        report_path = generator.generate_failure_report(results)
        assert Path(report_path).exists()

        with open(report_path, 'r', encoding='utf-8') as f:
            content = f.read()
            assert "所有测试通过" in content

    def test_generate_performance_report(self, generator, sample_test_results):
        """测试生成性能报告"""
        report_path = generator.generate_performance_report(
            sample_test_results,
            slow_threshold=2.0
        )
        assert Path(report_path).exists()

        with open(report_path, 'r', encoding='utf-8') as f:
            content = f.read()
            assert "性能报告" in content
            assert "慢速测试阈值" in content
            assert "integration_tests" in content  # 最慢的测试

        # 验证JSON报告
        json_path = Path(report_path).parent / "performance.json"
        assert json_path.exists()

        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            assert data["slow_threshold"] == 2.0
            assert data["slow_tests_count"] == 1  # integration_tests > 2.0

    def test_generate_junit_report(self, generator, sample_test_results):
        """测试生成JUnit XML报告"""
        report_path = generator.generate_junit_report(sample_test_results)
        assert Path(report_path).exists()

        # 解析XML并验证结构
        tree = ET.parse(report_path)
        root = tree.getroot()

        assert root.tag == 'testsuites'
        assert root.get('tests') == '15'
        assert root.get('failures') == '2'
        assert root.get('skipped') == '0'

        # 验证测试套件
        testsuites = root.findall('testsuite')
        assert len(testsuites) == 2

        # 验证第一个测试套件
        suite1 = testsuites[0]
        assert suite1.get('name') == 'unit_tests'
        assert suite1.get('tests') == '10'
        assert suite1.get('failures') == '2'

        # 验证失败元素
        failures = suite1.findall('.//failure')
        assert len(failures) == 2

    def test_generate_html_report(
        self,
        generator,
        sample_test_results,
        sample_coverage_data
    ):
        """测试生成HTML报告"""
        validation_result = ValidationResult(
            success=False,
            test_results=sample_test_results,
            coverage_data=sample_coverage_data,
            execution_time=3.8,
            timestamp=datetime.now()
        )

        report_path = generator.generate_html_report(validation_result)
        assert Path(report_path).exists()

        with open(report_path, 'r', encoding='utf-8') as f:
            content = f.read()
            assert "<!DOCTYPE html>" in content
            assert "Nexus 系统验证报告" in content
            assert "测试汇总" in content
            assert "代码覆盖率" in content
            assert "85.0%" in content  # 行覆盖率

    def test_html_report_without_coverage(self, generator, sample_test_results):
        """测试生成不含覆盖率的HTML报告"""
        validation_result = ValidationResult(
            success=True,
            test_results=sample_test_results,
            coverage_data=None,
            execution_time=3.8,
            timestamp=datetime.now()
        )

        report_path = generator.generate_html_report(validation_result)
        assert Path(report_path).exists()

        with open(report_path, 'r', encoding='utf-8') as f:
            content = f.read()
            assert "<!DOCTYPE html>" in content
            # 不应该有覆盖率部分
            assert content.count("代码覆盖率") <= 1  # 可能在标题中


# 策略：生成测试结果
test_result_strategy = st.builds(
    TestResult,
    suite_name=st.text(min_size=1, max_size=50, alphabet=st.characters(
        whitelist_categories=('Lu', 'Ll', 'Nd'),
        whitelist_characters='_-'
    )),
    total_tests=st.integers(min_value=0, max_value=100),
    passed_tests=st.integers(min_value=0, max_value=100),
    failed_tests=st.integers(min_value=0, max_value=100),
    skipped_tests=st.integers(min_value=0, max_value=100),
    execution_time=st.floats(min_value=0.0, max_value=100.0, allow_nan=False, allow_infinity=False),
    failures=st.lists(
        st.builds(
            TestFailure,
            test_name=st.text(min_size=1, max_size=50),
            error_message=st.text(min_size=1, max_size=200),
            stack_trace=st.text(max_size=500),
            counterexample=st.one_of(st.none(), st.text(max_size=100))
        ),
        max_size=10
    )
)


@settings(max_examples=100, deadline=None)
@given(
    test_results=st.lists(test_result_strategy, min_size=1, max_size=10)
)
def test_property_report_generation_completeness(test_results):
    """
    属性测试：报告生成完整性

    Feature: system-validation, Property 5: 报告生成完整性

    对于任何测试结果列表，所有报告格式都能成功生成

    **验证需求: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6**
    """
    # 创建临时目录
    temp_dir = tempfile.mkdtemp()
    try:
        # 创建配置
        config = ValidationConfig(
            build_dir="build",
            source_dir=".",
            report_dir=temp_dir
        )

        generator = ReportGenerator(config)

        # 生成所有报告格式
        # 1. 汇总报告
        summary_path = generator.generate_summary_report(test_results)
        assert Path(summary_path).exists()
        assert Path(summary_path).stat().st_size > 0

        # 验证JSON汇总也存在
        summary_json = Path(summary_path).parent / "summary.json"
        assert summary_json.exists()

        # 2. 失败报告
        failure_path = generator.generate_failure_report(test_results)
        assert Path(failure_path).exists()
        assert Path(failure_path).stat().st_size > 0

        # 验证JSON失败报告也存在
        failure_json = Path(failure_path).parent / "failures.json"
        assert failure_json.exists()

        # 3. 性能报告
        perf_path = generator.generate_performance_report(test_results)
        assert Path(perf_path).exists()
        assert Path(perf_path).stat().st_size > 0

        # 验证JSON性能报告也存在
        perf_json = Path(perf_path).parent / "performance.json"
        assert perf_json.exists()

        # 4. JUnit XML报告
        junit_path = generator.generate_junit_report(test_results)
        assert Path(junit_path).exists()
        assert Path(junit_path).stat().st_size > 0

        # 验证XML格式正确
        tree = ET.parse(junit_path)
        root = tree.getroot()
        assert root.tag == 'testsuites'

        # 5. HTML报告
        validation_result = ValidationResult(
            success=True,
            test_results=test_results,
            coverage_data=None,
            execution_time=sum(r.execution_time for r in test_results),
            timestamp=datetime.now()
        )
        html_path = generator.generate_html_report(validation_result)
        assert Path(html_path).exists()
        assert Path(html_path).stat().st_size > 0

        # 验证HTML格式
        with open(html_path, 'r', encoding='utf-8') as f:
            html_content = f.read()
            assert "<!DOCTYPE html>" in html_content
            assert "</html>" in html_content

        # 验证所有报告都包含数据
        # 汇总报告应该包含测试统计
        with open(summary_path, 'r', encoding='utf-8') as f:
            summary_content = f.read()
            assert "测试汇总报告" in summary_content

        # JUnit报告应该包含所有测试套件
        testsuites = root.findall('testsuite')
        assert len(testsuites) == len(test_results)

    except Exception as e:
        pytest.fail(f"报告生成失败: {str(e)}")
    finally:
        # 清理临时目录
        shutil.rmtree(temp_dir, ignore_errors=True)


@settings(max_examples=100, deadline=None)
@given(
    test_results=st.lists(test_result_strategy, min_size=1, max_size=10)
)
def test_property_summary_statistics_consistency(test_results):
    """
    属性测试：汇总统计一致性

    验证汇总报告中的统计数据与原始测试结果一致
    """
    temp_dir = tempfile.mkdtemp()
    try:
        config = ValidationConfig(
            build_dir="build",
            source_dir=".",
            report_dir=temp_dir
        )

        generator = ReportGenerator(config)

        # 生成汇总报告
        summary_path = generator.generate_summary_report(test_results)

        # 读取JSON汇总
        json_path = Path(summary_path).parent / "summary.json"
        with open(json_path, 'r', encoding='utf-8') as f:
            summary_data = json.load(f)

        # 计算预期统计
        expected_total = sum(r.total_tests for r in test_results)
        expected_passed = sum(r.passed_tests for r in test_results)
        expected_failed = sum(r.failed_tests for r in test_results)
        expected_skipped = sum(r.skipped_tests for r in test_results)

        # 验证统计一致性
        assert summary_data["summary"]["total_tests"] == expected_total
        assert summary_data["summary"]["passed_tests"] == expected_passed
        assert summary_data["summary"]["failed_tests"] == expected_failed
        assert summary_data["summary"]["skipped_tests"] == expected_skipped

        # 验证测试套件数量
        assert len(summary_data["test_suites"]) == len(test_results)
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@settings(max_examples=100, deadline=None)
@given(
    test_results=st.lists(test_result_strategy, min_size=1, max_size=10)
)
def test_property_junit_xml_validity(test_results):
    """
    属性测试：JUnit XML有效性

    验证生成的JUnit XML格式正确且可解析
    """
    temp_dir = tempfile.mkdtemp()
    try:
        config = ValidationConfig(
            build_dir="build",
            source_dir=".",
            report_dir=temp_dir
        )

        generator = ReportGenerator(config)

        # 生成JUnit报告
        junit_path = generator.generate_junit_report(test_results)

        # 解析XML
        tree = ET.parse(junit_path)
        root = tree.getroot()

        # 验证根元素
        assert root.tag == 'testsuites'

        # 验证必需属性存在
        assert 'tests' in root.attrib
        assert 'failures' in root.attrib
        assert 'skipped' in root.attrib
        assert 'time' in root.attrib

        # 验证测试套件数量
        testsuites = root.findall('testsuite')
        assert len(testsuites) == len(test_results)

        # 验证每个测试套件
        for testsuite in testsuites:
            assert 'name' in testsuite.attrib
            assert 'tests' in testsuite.attrib
            assert 'failures' in testsuite.attrib
            assert 'time' in testsuite.attrib

    except ET.ParseError as e:
        pytest.fail(f"JUnit XML解析失败: {str(e)}")
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)
