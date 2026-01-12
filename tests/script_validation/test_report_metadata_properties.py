"""
报告元数据完整性属性测试

属性10：验证报告元数据完整性
*对于任何*生成的验证报告，应该包含完整的执行时间戳、环境详细信息、
版本信息和可操作的问题解决建议

验证：需求 8.4, 8.5
"""

import pytest
from hypothesis import given, strategies as st, settings, assume
from pathlib import Path
from datetime import datetime, timedelta
import json
import re

from script_validation.models import (
    ValidationReport, ValidationResult, ValidationStatus,
    Platform, Script, ScriptType, ScriptCategory,
    CompatibilityMatrix, ValidationSummary, EnvironmentInfo,
    ScriptMetadata, Parameter
)
from script_validation.reporters.html_reporter import HTMLReporter
from script_validation.reporters.json_reporter import JSONReporter
from script_validation.reporters.summary_reporter import SummaryReporter


# ============================================================================
# 测试策略（Generators）
# ============================================================================

# 平台策略
platforms = st.sampled_from([Platform.WINDOWS, Platform.WSL, Platform.LINUX])

# 验证状态策略
validation_statuses = st.sampled_from([
    ValidationStatus.PASSED,
    ValidationStatus.FAILED,
    ValidationStatus.SKIPPED,
    ValidationStatus.ERROR
])

# 脚本类型策略
script_types = st.sampled_from([
    ScriptType.BATCH,
    ScriptType.POWERSHELL,
    ScriptType.SHELL,
    ScriptType.PYTHON
])

# 脚本分类策略
script_categories = st.sampled_from([
    ScriptCategory.BUILD,
    ScriptCategory.TEST,
    ScriptCategory.FORMAT,
    ScriptCategory.CLEAN,
    ScriptCategory.DOCS,
    ScriptCategory.SETUP,
    ScriptCategory.CI,
    ScriptCategory.UTILITY
])

# 版本字符串策略
version_strings = st.from_regex(r'[0-9]+\.[0-9]+\.[0-9]+', fullmatch=True)

# 安全文本策略（避免特殊字符导致的问题）
safe_text = st.text(
    alphabet=st.characters(
        whitelist_categories=('L', 'N', 'P', 'S'),
        blacklist_characters='<>&"\'\\\r\n\t'
    ),
    min_size=1,
    max_size=100
)

# 时间戳策略
timestamps = st.datetimes(
    min_value=datetime(2020, 1, 1),
    max_value=datetime(2030, 12, 31)
)

# 执行时间策略
execution_times = st.floats(min_value=0.001, max_value=3600.0, allow_nan=False, allow_infinity=False)

# 内存使用策略
memory_usages = st.integers(min_value=0, max_value=1024 * 1024 * 1024)  # 0 to 1GB

# 建议列表策略
recommendations = st.lists(safe_text, min_size=0, max_size=10)


# ============================================================================
# 辅助函数
# ============================================================================

@st.composite
def script_metadata_strategy(draw):
    """生成脚本元数据"""
    return ScriptMetadata(
        description=draw(safe_text),
        usage=draw(safe_text),
        parameters=[
            Parameter(
                name=draw(st.text(alphabet='abcdefghijklmnopqrstuvwxyz_', min_size=1, max_size=20)),
                description=draw(safe_text),
                required=draw(st.booleans()),
                default_value=draw(st.one_of(st.none(), safe_text))
            )
            for _ in range(draw(st.integers(min_value=0, max_value=3)))
        ],
        examples=draw(st.lists(safe_text, min_size=0, max_size=3)),
        author=draw(safe_text),
        version=draw(version_strings)
    )


@st.composite
def script_strategy(draw):
    """生成脚本对象"""
    script_type = draw(script_types)
    ext_map = {
        ScriptType.BATCH: '.bat',
        ScriptType.POWERSHELL: '.ps1',
        ScriptType.SHELL: '.sh',
        ScriptType.PYTHON: '.py'
    }
    name = draw(st.text(alphabet='abcdefghijklmnopqrstuvwxyz_', min_size=1, max_size=20))
    ext = ext_map[script_type]

    return Script(
        path=Path(f"scripts/{name}{ext}"),
        name=f"{name}{ext}",
        type=script_type,
        platform=draw(platforms),
        metadata=draw(script_metadata_strategy()),
        dependencies=draw(st.lists(
            st.text(alphabet='abcdefghijklmnopqrstuvwxyz_', min_size=1, max_size=20),
            min_size=0, max_size=5
        )),
        category=draw(script_categories)
    )


@st.composite
def validation_result_strategy(draw, script=None):
    """生成验证结果"""
    if script is None:
        script = draw(script_strategy())

    status = draw(validation_statuses)
    error = None
    if status in (ValidationStatus.FAILED, ValidationStatus.ERROR):
        error = draw(safe_text)

    return ValidationResult(
        script=script,
        platform=draw(platforms),
        validator=draw(st.sampled_from([
            'FunctionalValidator',
            'CompatibilityValidator',
            'PerformanceValidator',
            'DocumentationValidator'
        ])),
        status=status,
        execution_time=draw(execution_times),
        memory_usage=draw(memory_usages),
        output=draw(safe_text),
        error=error,
        details={}
    )


@st.composite
def environment_info_strategy(draw):
    """生成环境信息"""
    return EnvironmentInfo(
        platform=draw(platforms),
        os_version=draw(safe_text),
        python_version=draw(version_strings),
        shell_version=draw(safe_text),
        available_commands=draw(st.lists(
            st.text(alphabet='abcdefghijklmnopqrstuvwxyz_', min_size=1, max_size=20),
            min_size=0, max_size=10
        ))
    )


@st.composite
def validation_summary_strategy(draw):
    """生成验证摘要"""
    passed = draw(st.integers(min_value=0, max_value=50))
    failed = draw(st.integers(min_value=0, max_value=20))
    skipped = draw(st.integers(min_value=0, max_value=10))
    errors = draw(st.integers(min_value=0, max_value=10))
    total = passed + failed + skipped + errors

    return ValidationSummary(
        total_scripts=total,
        passed=passed,
        failed=failed,
        skipped=skipped,
        errors=errors,
        execution_time=draw(execution_times)
    )


@st.composite
def validation_report_strategy(draw):
    """生成完整的验证报告"""
    scripts = draw(st.lists(script_strategy(), min_size=1, max_size=10))
    platforms_list = [Platform.WINDOWS, Platform.WSL, Platform.LINUX]

    results = []
    for script in scripts:
        result = draw(validation_result_strategy(script=script))
        results.append(result)

    # 创建兼容性矩阵
    matrix = CompatibilityMatrix(
        scripts=scripts,
        platforms=platforms_list,
        results={}
    )
    for result in results:
        matrix.set_result(result.script.name, result.platform, result)

    # 计算摘要
    passed = sum(1 for r in results if r.status == ValidationStatus.PASSED)
    failed = sum(1 for r in results if r.status == ValidationStatus.FAILED)
    skipped = sum(1 for r in results if r.status == ValidationStatus.SKIPPED)
    errors = sum(1 for r in results if r.status == ValidationStatus.ERROR)

    summary = ValidationSummary(
        total_scripts=len(results),
        passed=passed,
        failed=failed,
        skipped=skipped,
        errors=errors,
        execution_time=draw(execution_times)
    )

    return ValidationReport(
        timestamp=draw(timestamps),
        environment=draw(environment_info_strategy()),
        summary=summary,
        results=results,
        compatibility_matrix=matrix,
        recommendations=draw(recommendations)
    )


# ============================================================================
# 属性测试：报告元数据完整性
# Feature: script-delivery-validation, Property 10: 验证报告元数据完整性
# ============================================================================

class TestReportMetadataCompleteness:
    """
    属性10：验证报告元数据完整性

    *对于任何*生成的验证报告，应该包含完整的执行时间戳、环境详细信息、
    版本信息和可操作的问题解决建议

    **验证：需求 8.4, 8.5**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_html_report_contains_timestamp(self, report_data: ValidationReport):
        """
        属性10.1：HTML报告包含执行时间戳

        *对于任何*验证报告，生成的HTML报告应该包含执行时间戳

        **验证：需求 8.4**
        """
        # Arrange
        reporter = HTMLReporter()

        # Act
        html_content = reporter.generate_report(report_data)

        # Assert
        timestamp_str = report_data.timestamp.strftime("%Y-%m-%d %H:%M:%S")
        assert timestamp_str in html_content, "HTML报告应该包含执行时间戳"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_html_report_contains_environment_details(self, report_data: ValidationReport):
        """
        属性10.2：HTML报告包含环境详细信息

        *对于任何*验证报告，生成的HTML报告应该包含环境详细信息

        **验证：需求 8.4**
        """
        # Arrange
        reporter = HTMLReporter()

        # Act
        html_content = reporter.generate_report(report_data)

        # Assert
        env = report_data.environment
        assert env.platform.value in html_content, "HTML报告应该包含平台信息"
        assert env.python_version in html_content, "HTML报告应该包含Python版本"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_contains_complete_metadata(self, report_data: ValidationReport):
        """
        属性10.3：JSON报告包含完整元数据

        *对于任何*验证报告，生成的JSON报告应该包含完整的元数据

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert - 检查元数据字段
        assert "metadata" in parsed, "JSON报告应该包含metadata字段"
        metadata = parsed["metadata"]
        assert "timestamp" in metadata, "元数据应该包含timestamp"
        assert "report_type" in metadata, "元数据应该包含report_type"
        assert "project" in metadata, "元数据应该包含project"
        assert "total_scripts" in metadata, "元数据应该包含total_scripts"
        assert "platforms_tested" in metadata, "元数据应该包含platforms_tested"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_contains_environment_info(self, report_data: ValidationReport):
        """
        属性10.4：JSON报告包含环境信息

        *对于任何*验证报告，生成的JSON报告应该包含环境信息

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert - 检查环境信息字段
        assert "environment" in parsed, "JSON报告应该包含environment字段"
        env = parsed["environment"]
        assert "platform" in env, "环境信息应该包含platform"
        assert "os_version" in env, "环境信息应该包含os_version"
        assert "python_version" in env, "环境信息应该包含python_version"
        assert "shell_version" in env, "环境信息应该包含shell_version"
        assert "available_commands" in env, "环境信息应该包含available_commands"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_contains_version_info(self, report_data: ValidationReport):
        """
        属性10.5：JSON报告包含版本信息

        *对于任何*验证报告，生成的JSON报告应该包含版本信息

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert - 检查版本信息
        assert "version" in parsed, "JSON报告应该包含version字段"
        assert "report_format" in parsed, "JSON报告应该包含report_format字段"
        # 版本格式应该是语义化版本
        version = parsed["version"]
        assert re.match(r'^\d+\.\d+\.\d+$', version), "版本应该是语义化版本格式"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_timestamp_is_iso_format(self, report_data: ValidationReport):
        """
        属性10.6：JSON报告时间戳是ISO格式

        *对于任何*验证报告，生成的JSON报告的时间戳应该是ISO格式

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert - 检查时间戳格式
        assert "generated_at" in parsed, "JSON报告应该包含generated_at字段"
        timestamp_str = parsed["generated_at"]
        # 尝试解析ISO格式时间戳
        try:
            datetime.fromisoformat(timestamp_str)
        except ValueError:
            pytest.fail(f"时间戳 '{timestamp_str}' 不是有效的ISO格式")

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_summary_report_contains_timestamp(self, report_data: ValidationReport):
        """
        属性10.7：摘要报告包含时间戳

        *对于任何*验证报告，生成的摘要报告应该包含时间戳

        **验证：需求 8.4**
        """
        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        summary_content = reporter.generate_report(report_data)

        # Assert
        timestamp_str = report_data.timestamp.strftime("%Y-%m-%d %H:%M:%S")
        assert timestamp_str in summary_content, "摘要报告应该包含执行时间戳"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_summary_report_contains_environment_info(self, report_data: ValidationReport):
        """
        属性10.8：摘要报告包含环境信息

        *对于任何*验证报告，生成的摘要报告应该包含环境信息

        **验证：需求 8.4**
        """
        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        summary_content = reporter.generate_report(report_data)

        # Assert
        env = report_data.environment
        assert env.platform.value in summary_content, "摘要报告应该包含平台信息"
        assert env.python_version in summary_content, "摘要报告应该包含Python版本"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_contains_recommendations(self, report_data: ValidationReport):
        """
        属性10.9：JSON报告包含建议

        *对于任何*验证报告，生成的JSON报告应该包含建议列表

        **验证：需求 8.5**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert
        assert "recommendations" in parsed, "JSON报告应该包含recommendations字段"
        assert isinstance(parsed["recommendations"], list), "recommendations应该是列表"
        assert parsed["recommendations"] == report_data.recommendations, \
            "recommendations应该与输入数据一致"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_html_report_contains_recommendations_section(self, report_data: ValidationReport):
        """
        属性10.10：HTML报告包含建议部分

        *对于任何*验证报告，生成的HTML报告应该包含建议部分

        **验证：需求 8.5**
        """
        # Arrange
        reporter = HTMLReporter()

        # Act
        html_content = reporter.generate_report(report_data)

        # Assert
        assert "建议" in html_content or "recommendations" in html_content.lower(), \
            "HTML报告应该包含建议部分"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_summary_report_contains_recommendations_section(self, report_data: ValidationReport):
        """
        属性10.11：摘要报告包含建议部分

        *对于任何*验证报告，生成的摘要报告应该包含建议部分

        **验证：需求 8.5**
        """
        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        summary_content = reporter.generate_report(report_data)

        # Assert
        assert "建议" in summary_content or "改进" in summary_content, \
            "摘要报告应该包含建议部分"


class TestReportActionableRecommendations:
    """
    测试报告的可操作建议

    **验证：需求 8.5**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_summary_reporter_generates_actionable_items(self, report_data: ValidationReport):
        """
        属性10.12：摘要报告生成可操作项

        *对于任何*包含失败或错误的验证报告，应该生成可操作的问题列表

        **验证：需求 8.5**
        """
        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        actionable_items = reporter.generate_actionable_items(report_data)

        # Assert
        failed_or_error_count = sum(
            1 for r in report_data.results
            if r.status in (ValidationStatus.FAILED, ValidationStatus.ERROR)
        )

        # 每个失败或错误的结果应该有一个可操作项
        assert len(actionable_items) == failed_or_error_count, \
            "可操作项数量应该等于失败和错误的数量"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_actionable_items_contain_suggestions(self, report_data: ValidationReport):
        """
        属性10.13：可操作项包含建议

        *对于任何*可操作项，应该包含具体的修复建议

        **验证：需求 8.5**
        """
        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        actionable_items = reporter.generate_actionable_items(report_data)

        # Assert
        for item in actionable_items:
            assert "建议" in item, f"可操作项应该包含建议: {item}"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_quick_summary_contains_status(self, report_data: ValidationReport):
        """
        属性10.14：快速摘要包含状态

        *对于任何*验证报告，快速摘要应该包含通过/失败状态

        **验证：需求 8.5**
        """
        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        quick_summary = reporter.generate_quick_summary(report_data)

        # Assert
        assert "PASSED" in quick_summary or "FAILED" in quick_summary, \
            "快速摘要应该包含状态"
        assert "通过" in quick_summary, "快速摘要应该包含通过数量"
        assert "失败" in quick_summary, "快速摘要应该包含失败数量"


class TestReportSerialization:
    """
    测试报告序列化

    **验证：需求 8.4**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_is_valid_json(self, report_data: ValidationReport):
        """
        属性10.15：JSON报告是有效的JSON

        *对于任何*验证报告，生成的JSON报告应该是有效的JSON

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)

        # Assert
        try:
            parsed = json.loads(json_content)
            assert isinstance(parsed, dict), "JSON报告应该是一个对象"
        except json.JSONDecodeError as e:
            pytest.fail(f"JSON报告不是有效的JSON: {e}")

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_contains_all_results(self, report_data: ValidationReport):
        """
        属性10.16：JSON报告包含所有结果

        *对于任何*验证报告，生成的JSON报告应该包含所有验证结果

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert
        assert "results" in parsed, "JSON报告应该包含results字段"
        assert len(parsed["results"]) == len(report_data.results), \
            "JSON报告应该包含所有验证结果"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=100)
    def test_json_report_contains_compatibility_matrix(self, report_data: ValidationReport):
        """
        属性10.17：JSON报告包含兼容性矩阵

        *对于任何*验证报告，生成的JSON报告应该包含兼容性矩阵

        **验证：需求 8.4**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert
        assert "compatibility_matrix" in parsed, "JSON报告应该包含compatibility_matrix字段"
        matrix = parsed["compatibility_matrix"]
        assert "scripts" in matrix, "兼容性矩阵应该包含scripts"
        assert "platforms" in matrix, "兼容性矩阵应该包含platforms"
        assert "matrix" in matrix, "兼容性矩阵应该包含matrix数据"


class TestReportFileSaving:
    """
    测试报告文件保存

    **验证：需求 8.4**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=50)
    def test_html_report_can_be_saved(self, report_data: ValidationReport):
        """
        属性10.18：HTML报告可以保存到文件

        *对于任何*验证报告，生成的HTML报告应该能够保存到文件

        **验证：需求 8.4**
        """
        import tempfile
        import os

        # Arrange
        reporter = HTMLReporter()

        # Act
        html_content = reporter.generate_report(report_data)

        with tempfile.TemporaryDirectory() as tmp_dir:
            output_path = Path(tmp_dir) / "report.html"
            result = reporter.save_report(html_content, str(output_path))

            # Assert
            assert result is True, "保存应该成功"
            assert output_path.exists(), "文件应该存在"
            saved_content = output_path.read_text(encoding='utf-8')
            assert saved_content == html_content, "保存的内容应该与生成的内容一致"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=50)
    def test_json_report_can_be_saved(self, report_data: ValidationReport):
        """
        属性10.19：JSON报告可以保存到文件

        *对于任何*验证报告，生成的JSON报告应该能够保存到文件

        **验证：需求 8.4**
        """
        import tempfile

        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)

        with tempfile.TemporaryDirectory() as tmp_dir:
            output_path = Path(tmp_dir) / "report.json"
            result = reporter.save_report(json_content, str(output_path))

            # Assert
            assert result is True, "保存应该成功"
            assert output_path.exists(), "文件应该存在"
            saved_content = output_path.read_text(encoding='utf-8')
            assert saved_content == json_content, "保存的内容应该与生成的内容一致"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=50)
    def test_summary_report_can_be_saved(self, report_data: ValidationReport):
        """
        属性10.20：摘要报告可以保存到文件

        *对于任何*验证报告，生成的摘要报告应该能够保存到文件

        **验证：需求 8.4**
        """
        import tempfile

        # Arrange
        reporter = SummaryReporter(use_colors=False)

        # Act
        summary_content = reporter.generate_report(report_data)

        with tempfile.TemporaryDirectory() as tmp_dir:
            output_path = Path(tmp_dir) / "report.txt"
            result = reporter.save_report(summary_content, str(output_path))

            # Assert
            assert result is True, "保存应该成功"
            assert output_path.exists(), "文件应该存在"


class TestReportFormatConsistency:
    """
    测试报告格式一致性

    **验证：需求 8.4**
    """

    def test_html_reporter_format_is_html(self):
        """
        属性10.21：HTML报告器格式是html

        **验证：需求 8.4**
        """
        reporter = HTMLReporter()
        assert reporter.get_report_format() == "html"

    def test_json_reporter_format_is_json(self):
        """
        属性10.22：JSON报告器格式是json

        **验证：需求 8.4**
        """
        reporter = JSONReporter()
        assert reporter.get_report_format() == "json"

    def test_summary_reporter_format_is_summary(self):
        """
        属性10.23：摘要报告器格式是summary

        **验证：需求 8.4**
        """
        reporter = SummaryReporter()
        assert reporter.get_report_format() == "summary"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
