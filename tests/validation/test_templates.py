"""
\\file            test_templates.py
\\brief           测试报告模板的渲染功能
\\author          Nexus Team
\\version         1.0.0
\\date            2026-01-18

\\copyright       Copyright (c) 2026 Nexus Team

\\details         验证所有Jinja2模板可以正确加载和渲染
"""

import pytest
from pathlib import Path
from datetime import datetime
from jinja2 import Environment, FileSystemLoader, select_autoescape

from scripts.validation.models import (
    ValidationResult,
    TestResult,
    TestFailure,
    CoverageData,
    CodeLocation
)


@pytest.fixture
def jinja_env():
    """
    \\brief           创建Jinja2环境
    """
    template_dir = Path(__file__).parent.parent.parent / "scripts" / "validation" / "templates"
    return Environment(
        loader=FileSystemLoader(str(template_dir)),
        autoescape=select_autoescape(['html', 'xml'])
    )


@pytest.fixture
def sample_test_results():
    """
    \\brief           创建示例测试结果
    """
    return [
        TestResult(
            suite_name="ConfigTests",
            total_tests=10,
            passed_tests=8,
            failed_tests=2,
            skipped_tests=0,
            execution_time=1.234,
            failures=[
                TestFailure(
                    test_name="test_config_load",
                    error_message="Expected value 42, got 0",
                    stack_trace="at config.c:123",
                    counterexample=None
                ),
                TestFailure(
                    test_name="test_config_save",
                    error_message="File not found",
                    stack_trace="at config.c:456",
                    counterexample="Input: {'key': 'value'}"
                )
            ]
        ),
        TestResult(
            suite_name="HALTests",
            total_tests=15,
            passed_tests=15,
            failed_tests=0,
            skipped_tests=0,
            execution_time=2.567
        )
    ]


@pytest.fixture
def sample_coverage_data():
    """
    \\brief           创建示例覆盖率数据
    """
    return CoverageData(
        line_coverage=0.85,
        branch_coverage=0.78,
        function_coverage=0.92,
        uncovered_lines=[
            CodeLocation(file_path="src/config.c", line_number=123, function_name="load_config"),
            CodeLocation(file_path="src/hal.c", line_number=456, function_name="init_hal")
        ],
        uncovered_branches=[
            CodeLocation(file_path="src/config.c", line_number=789, function_name="validate_config")
        ]
    )


def test_summary_template_loads(jinja_env):
    """
    \\brief           测试汇总报告模板可以加载
    """
    template = jinja_env.get_template('summary.html.j2')
    assert template is not None


def test_summary_template_renders(jinja_env, sample_test_results, sample_coverage_data):
    """
    \\brief           测试汇总报告模板可以渲染
    """
    template = jinja_env.get_template('summary.html.j2')

    # 准备模板数据
    total_tests = sum(r.total_tests for r in sample_test_results)
    passed_tests = sum(r.passed_tests for r in sample_test_results)
    failed_tests = sum(r.failed_tests for r in sample_test_results)
    skipped_tests = sum(r.skipped_tests for r in sample_test_results)
    execution_time = sum(r.execution_time for r in sample_test_results)
    pass_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0.0

    template_data = {
        'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'success': failed_tests == 0,
        'total_tests': total_tests,
        'passed_tests': passed_tests,
        'failed_tests': failed_tests,
        'skipped_tests': skipped_tests,
        'pass_rate': pass_rate,
        'execution_time': execution_time,
        'test_results': [
            {
                'suite_name': r.suite_name,
                'total_tests': r.total_tests,
                'passed_tests': r.passed_tests,
                'failed_tests': r.failed_tests,
                'skipped_tests': r.skipped_tests,
                'execution_time': r.execution_time,
                'pass_rate': (r.passed_tests / r.total_tests * 100) if r.total_tests > 0 else 0.0
            }
            for r in sample_test_results
        ],
        'coverage': {
            'line_coverage': sample_coverage_data.line_coverage * 100,
            'branch_coverage': sample_coverage_data.branch_coverage * 100,
            'function_coverage': sample_coverage_data.function_coverage * 100,
            'uncovered_lines_count': len(sample_coverage_data.uncovered_lines),
            'uncovered_branches_count': len(sample_coverage_data.uncovered_branches)
        }
    }

    html = template.render(**template_data)

    # 验证关键内容存在
    assert '测试汇总报告' in html
    assert 'ConfigTests' in html
    assert 'HALTests' in html
    assert str(total_tests) in html
    assert '代码覆盖率概览' in html


def test_coverage_template_loads(jinja_env):
    """
    \\brief           测试覆盖率报告模板可以加载
    """
    template = jinja_env.get_template('coverage.html.j2')
    assert template is not None


def test_coverage_template_renders(jinja_env, sample_coverage_data):
    """
    \\brief           测试覆盖率报告模板可以渲染
    """
    template = jinja_env.get_template('coverage.html.j2')

    template_data = {
        'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'threshold': 0.80,
        'coverage': {
            'line_coverage': sample_coverage_data.line_coverage * 100,
            'branch_coverage': sample_coverage_data.branch_coverage * 100,
            'function_coverage': sample_coverage_data.function_coverage * 100,
            'uncovered_lines': sample_coverage_data.uncovered_lines,
            'uncovered_branches': sample_coverage_data.uncovered_branches
        },
        'file_coverage': [
            {
                'path': 'src/config.c',
                'line_coverage': 85.0,
                'branch_coverage': 78.0,
                'function_coverage': 92.0
            },
            {
                'path': 'src/hal.c',
                'line_coverage': 90.0,
                'branch_coverage': 85.0,
                'function_coverage': 95.0
            }
        ]
    }

    html = template.render(**template_data)

    # 验证关键内容存在
    assert '代码覆盖率报告' in html
    assert '覆盖率概览' in html
    assert '文件级覆盖率' in html
    assert 'src/config.c' in html
    assert 'src/hal.c' in html
    assert '未覆盖的代码行' in html


def test_junit_template_loads(jinja_env):
    """
    \\brief           测试JUnit XML模板可以加载
    """
    template = jinja_env.get_template('junit.xml.j2')
    assert template is not None


def test_junit_template_renders(jinja_env, sample_test_results):
    """
    \\brief           测试JUnit XML模板可以渲染
    """
    template = jinja_env.get_template('junit.xml.j2')

    total_tests = sum(r.total_tests for r in sample_test_results)
    total_failures = sum(r.failed_tests for r in sample_test_results)
    total_skipped = sum(r.skipped_tests for r in sample_test_results)
    total_time = sum(r.execution_time for r in sample_test_results)

    template_data = {
        'timestamp': datetime.now().isoformat(),
        'total_tests': total_tests,
        'total_failures': total_failures,
        'total_skipped': total_skipped,
        'total_time': total_time,
        'test_results': sample_test_results
    }

    xml = template.render(**template_data)

    # 验证XML结构
    assert '<?xml version="1.0" encoding="UTF-8"?>' in xml
    assert '<testsuites' in xml
    assert '</testsuites>' in xml
    assert '<testsuite' in xml
    assert '</testsuite>' in xml
    assert '<testcase' in xml
    assert 'ConfigTests' in xml
    assert 'HALTests' in xml
    assert '<failure' in xml
    assert 'test_config_load' in xml


def test_junit_template_valid_xml(jinja_env, sample_test_results):
    """
    \\brief           测试JUnit XML模板生成有效的XML
    """
    import xml.etree.ElementTree as ET

    template = jinja_env.get_template('junit.xml.j2')

    total_tests = sum(r.total_tests for r in sample_test_results)
    total_failures = sum(r.failed_tests for r in sample_test_results)
    total_skipped = sum(r.skipped_tests for r in sample_test_results)
    total_time = sum(r.execution_time for r in sample_test_results)

    template_data = {
        'timestamp': datetime.now().isoformat(),
        'total_tests': total_tests,
        'total_failures': total_failures,
        'total_skipped': total_skipped,
        'total_time': total_time,
        'test_results': sample_test_results
    }

    xml = template.render(**template_data)

    # 尝试解析XML
    try:
        root = ET.fromstring(xml)
        assert root.tag == 'testsuites'
        assert root.get('tests') == str(total_tests)
        assert root.get('failures') == str(total_failures)
    except ET.ParseError as e:
        pytest.fail(f"生成的XML无效: {e}")


def test_all_templates_exist(jinja_env):
    """
    \\brief           测试所有必需的模板都存在
    """
    required_templates = [
        'summary.html.j2',
        'coverage.html.j2',
        'junit.xml.j2',
        'report.html.j2'
    ]

    for template_name in required_templates:
        template = jinja_env.get_template(template_name)
        assert template is not None, f"模板 {template_name} 不存在"

