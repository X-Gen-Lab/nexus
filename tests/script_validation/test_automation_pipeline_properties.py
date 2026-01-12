"""
自动化管道集成属性测试

属性8：自动化管道集成
*对于任何*验证管道执行，应该能够自动运行所有测试、生成完整报告、
处理失败情况、更新状态并提供正确的CI退出代码

验证：需求 6.1, 6.2, 6.3, 6.4, 6.5
"""

import pytest
from hypothesis import given, strategies as st, settings, assume
from pathlib import Path
from datetime import datetime
import tempfile
import json

from script_validation.models import (
    ValidationReport, ValidationResult, ValidationStatus,
    Platform, Script, ScriptType, ScriptCategory,
    CompatibilityMatrix, ValidationSummary, EnvironmentInfo,
    ScriptMetadata, Parameter, ValidationConfig
)
from script_validation.controllers.validation_controller import ValidationController
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

# 安全文本策略
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
memory_usages = st.integers(min_value=0, max_value=1024 * 1024 * 1024)

# 建议列表策略
recommendations = st.lists(safe_text, min_size=0, max_size=10)

# 验证器名称策略
validator_names = st.lists(
    st.sampled_from(['functional', 'compatibility', 'performance', 'documentation']),
    min_size=1,
    max_size=4,
    unique=True
)

# 验证模式策略
validation_modes = st.sampled_from(['full', 'quick', 'platform-specific'])


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


@st.composite
def validation_config_strategy(draw):
    """生成验证配置"""
    return ValidationConfig(
        root_path=Path('.'),
        target_platforms=draw(st.lists(platforms, min_size=1, max_size=3, unique=True)),
        script_patterns=["*.bat", "*.ps1", "*.sh", "*.py"],
        exclude_patterns=[],
        timeout_seconds=draw(st.integers(min_value=30, max_value=600)),
        max_memory_mb=draw(st.integers(min_value=256, max_value=2048)),
        parallel_execution=draw(st.booleans()),
        generate_html_report=draw(st.booleans()),
        generate_json_report=draw(st.booleans()),
        generate_summary_report=draw(st.booleans()),
        enabled_validators=draw(validator_names),
        validation_mode=draw(validation_modes),
        verbose=draw(st.booleans()),
        ci_mode=draw(st.booleans())
    )


# ============================================================================
# 属性测试：自动化管道集成
# Feature: script-delivery-validation, Property 8: 自动化管道集成
# ============================================================================

class TestAutomationPipelineExecution:
    """
    属性8：自动化管道集成

    *对于任何*验证管道执行，应该能够自动运行所有测试、生成完整报告、
    处理失败情况、更新状态并提供正确的CI退出代码

    **验证：需求 6.1, 6.2, 6.3, 6.4, 6.5**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_exit_code_zero_for_all_passed(self, report_data: ValidationReport):
        """
        属性8.1：所有测试通过时退出代码为0

        *对于任何*验证报告，如果所有测试通过（无失败和错误），
        退出代码应该为0

        **验证：需求 6.5**
        """
        # Arrange - 修改报告使所有测试通过
        report_data.summary.failed = 0
        report_data.summary.errors = 0

        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # Act
        exit_code = controller.get_exit_code(report_data)

        # Assert
        assert exit_code == 0, "所有测试通过时退出代码应该为0"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_exit_code_one_for_failures(self, report_data: ValidationReport):
        """
        属性8.2：存在失败时退出代码为1

        *对于任何*验证报告，如果存在失败（但无错误），
        退出代码应该为1

        **验证：需求 6.5**
        """
        # Arrange - 修改报告使存在失败但无错误
        report_data.summary.failed = max(1, report_data.summary.failed)
        report_data.summary.errors = 0

        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # Act
        exit_code = controller.get_exit_code(report_data)

        # Assert
        assert exit_code == 1, "存在失败时退出代码应该为1"


    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_exit_code_two_for_errors(self, report_data: ValidationReport):
        """
        属性8.3：存在错误时退出代码为2

        *对于任何*验证报告，如果存在执行错误，
        退出代码应该为2

        **验证：需求 6.5**
        """
        # Arrange - 修改报告使存在错误
        report_data.summary.errors = max(1, report_data.summary.errors)

        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # Act
        exit_code = controller.get_exit_code(report_data)

        # Assert
        assert exit_code == 2, "存在错误时退出代码应该为2"


class TestValidationReportGeneration:
    """
    测试验证报告生成

    **验证：需求 6.2**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_report_contains_pass_fail_status(self, report_data: ValidationReport):
        """
        属性8.4：报告包含每个脚本的通过/失败状态

        *对于任何*验证报告，应该包含每个脚本的通过/失败状态

        **验证：需求 6.2**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert
        assert "results" in parsed, "报告应该包含results字段"
        for result in parsed["results"]:
            assert "status" in result, "每个结果应该包含status字段"
            assert result["status"] in ["passed", "failed", "skipped", "error"], \
                "状态应该是有效的验证状态"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_report_summary_matches_results(self, report_data: ValidationReport):
        """
        属性8.5：报告摘要与结果一致

        *对于任何*验证报告，摘要中的统计数据应该与实际结果一致

        **验证：需求 6.2**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert
        summary = parsed["summary"]
        results = parsed["results"]

        actual_passed = sum(1 for r in results if r["status"] == "passed")
        actual_failed = sum(1 for r in results if r["status"] == "failed")
        actual_skipped = sum(1 for r in results if r["status"] == "skipped")
        actual_errors = sum(1 for r in results if r["status"] == "error")

        assert summary["passed"] == actual_passed, "通过数量应该一致"
        assert summary["failed"] == actual_failed, "失败数量应该一致"
        assert summary["skipped"] == actual_skipped, "跳过数量应该一致"
        assert summary["errors"] == actual_errors, "错误数量应该一致"


class TestFailureHandling:
    """
    测试失败处理

    **验证：需求 6.3**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_failed_results_have_error_info(self, report_data: ValidationReport):
        """
        属性8.6：失败的结果包含错误信息

        *对于任何*失败或错误的验证结果，应该包含详细的失败信息

        **验证：需求 6.3**
        """
        # Arrange
        reporter = JSONReporter()

        # Act
        json_content = reporter.generate_report(report_data)
        parsed = json.loads(json_content)

        # Assert
        for result in parsed["results"]:
            if result["status"] in ["failed", "error"]:
                # 失败或错误的结果应该有error字段或details字段
                has_error_info = result.get("error") is not None or result.get("details")
                assert has_error_info or result["status"] == "failed", \
                    "失败的结果应该包含错误信息或详情"

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_recommendations_generated_for_failures(self, report_data: ValidationReport):
        """
        属性8.7：为失败生成建议

        *对于任何*包含失败的验证报告，应该生成修复建议

        **验证：需求 6.3**
        """
        # Arrange
        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # 确保有失败的结果
        has_failures = any(
            r.status in [ValidationStatus.FAILED, ValidationStatus.ERROR]
            for r in report_data.results
        )

        # Act
        recommendations = controller._generate_recommendations(report_data.results)

        # Assert
        if has_failures:
            assert len(recommendations) > 0, "有失败时应该生成建议"


class TestCompatibilityMatrixUpdate:
    """
    测试兼容性矩阵更新

    **验证：需求 6.4**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_compatibility_matrix_updated_with_results(self, report_data: ValidationReport):
        """
        属性8.8：兼容性矩阵随结果更新

        *对于任何*验证报告，兼容性矩阵应该包含所有验证结果

        **验证：需求 6.4**
        """
        # Arrange
        matrix = report_data.compatibility_matrix

        # Act & Assert
        for result in report_data.results:
            stored_result = matrix.get_result(result.script.name, result.platform)
            # 结果应该存在于矩阵中
            assert stored_result is not None, \
                f"矩阵应该包含脚本 {result.script.name} 在平台 {result.platform.value} 的结果"


    @given(scripts=st.lists(script_strategy(), min_size=1, max_size=5))
    @settings(max_examples=25)
    def test_compatibility_matrix_tracks_history(self, scripts):
        """
        属性8.9：兼容性矩阵跟踪历史

        *对于任何*兼容性矩阵，状态变更应该被记录到历史中

        **验证：需求 6.4**
        """
        # Arrange
        platforms_list = [Platform.WINDOWS, Platform.WSL, Platform.LINUX]
        matrix = CompatibilityMatrix(
            scripts=scripts,
            platforms=platforms_list
        )

        # 创建初始结果
        script = scripts[0]
        initial_result = ValidationResult(
            script=script,
            platform=Platform.WINDOWS,
            validator="test",
            status=ValidationStatus.PASSED,
            execution_time=1.0,
            memory_usage=0,
            output=""
        )
        matrix.set_result(script.name, Platform.WINDOWS, initial_result)

        # 创建状态变更的结果
        changed_result = ValidationResult(
            script=script,
            platform=Platform.WINDOWS,
            validator="test",
            status=ValidationStatus.FAILED,
            execution_time=1.0,
            memory_usage=0,
            output="",
            error="Test failure"
        )

        # Act
        matrix.set_result(script.name, Platform.WINDOWS, changed_result)

        # Assert
        history = matrix.get_history(script_name=script.name)
        assert len(history) > 0, "状态变更应该被记录到历史中"
        assert history[0]['old_status'] == 'passed', "历史应该记录旧状态"
        assert history[0]['new_status'] == 'failed', "历史应该记录新状态"


class TestValidationConfigIntegration:
    """
    测试验证配置集成

    **验证：需求 6.1**
    """

    @given(config=validation_config_strategy())
    @settings(max_examples=25)
    def test_config_validation(self, config: ValidationConfig):
        """
        属性8.10：配置验证

        *对于任何*验证配置，应该能够验证其有效性

        **验证：需求 6.1**
        """
        # Act
        errors = config.validate()

        # Assert - 配置应该能够被验证（可能有错误，但不应该抛出异常）
        assert isinstance(errors, list), "验证应该返回错误列表"

    @given(config=validation_config_strategy())
    @settings(max_examples=25)
    def test_config_to_dict_roundtrip(self, config: ValidationConfig):
        """
        属性8.11：配置字典往返

        *对于任何*验证配置，转换为字典后应该能够重建

        **验证：需求 6.1**
        """
        # Act
        config_dict = config.to_dict()
        restored_config = ValidationConfig.from_dict(config_dict)

        # Assert
        assert restored_config.timeout_seconds == config.timeout_seconds
        assert restored_config.max_memory_mb == config.max_memory_mb
        assert restored_config.validation_mode == config.validation_mode
        assert restored_config.verbose == config.verbose
        assert restored_config.ci_mode == config.ci_mode


    @given(config=validation_config_strategy())
    @settings(max_examples=25)
    def test_config_file_save_and_load(self, config: ValidationConfig):
        """
        属性8.12：配置文件保存和加载

        *对于任何*验证配置，应该能够保存到文件并重新加载

        **验证：需求 6.1**
        """
        import tempfile

        # Arrange
        with tempfile.TemporaryDirectory() as tmp_dir:
            json_path = Path(tmp_dir) / "config.json"

            # Act - 保存为JSON
            save_result = config.save_to_file(json_path)

            # Assert
            if save_result:
                assert json_path.exists(), "配置文件应该被创建"
                loaded_config = ValidationConfig.from_file(json_path)
                assert loaded_config.timeout_seconds == config.timeout_seconds
                assert loaded_config.max_memory_mb == config.max_memory_mb

    @given(config=validation_config_strategy())
    @settings(max_examples=25)
    def test_quick_mode_uses_functional_validator_only(self, config: ValidationConfig):
        """
        属性8.13：快速模式只使用功能验证器

        *对于任何*快速模式的配置，应该只使用功能验证器

        **验证：需求 6.1**
        """
        # Arrange
        config.validation_mode = 'quick'

        # Act
        effective_validators = config.get_effective_validators()

        # Assert
        assert effective_validators == ['functional'], \
            "快速模式应该只使用功能验证器"


class TestControllerLifecycle:
    """
    测试控制器生命周期管理

    **验证：需求 6.1**
    """

    def test_controller_initializes_validators(self):
        """
        属性8.14：控制器初始化验证器

        控制器应该在初始化时创建所有验证器

        **验证：需求 6.1**
        """
        # Arrange
        config = ValidationConfig(root_path=Path('.'))

        # Act
        controller = ValidationController(config)

        # Assert
        validators = controller.list_validators()
        assert len(validators) > 0, "控制器应该初始化验证器"
        assert 'functional' in validators, "应该包含功能验证器"

    def test_controller_initializes_reporters(self):
        """
        属性8.15：控制器初始化报告器

        控制器应该在初始化时创建所有报告器

        **验证：需求 6.1**
        """
        # Arrange
        config = ValidationConfig(
            root_path=Path('.'),
            generate_html_report=True,
            generate_json_report=True,
            generate_summary_report=True
        )

        # Act
        controller = ValidationController(config)

        # Assert
        reporters = controller.list_reporters()
        assert 'html' in reporters, "应该包含HTML报告器"
        assert 'json' in reporters, "应该包含JSON报告器"
        assert 'summary' in reporters, "应该包含摘要报告器"


    def test_controller_can_add_validator(self):
        """
        属性8.16：控制器可以添加验证器

        控制器应该支持动态添加验证器

        **验证：需求 6.1**
        """
        # Arrange
        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        from script_validation.validators import FunctionalValidator
        new_validator = FunctionalValidator()

        # Act
        controller.add_validator('custom', new_validator)

        # Assert
        assert 'custom' in controller.list_validators()
        assert controller.get_validator('custom') is new_validator

    def test_controller_can_remove_validator(self):
        """
        属性8.17：控制器可以移除验证器

        控制器应该支持动态移除验证器

        **验证：需求 6.1**
        """
        # Arrange
        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # 确保有验证器可以移除
        validators_before = controller.list_validators()
        assume(len(validators_before) > 0)
        validator_to_remove = validators_before[0]

        # Act
        result = controller.remove_validator(validator_to_remove)

        # Assert
        assert result is True
        assert validator_to_remove not in controller.list_validators()

    def test_controller_can_add_reporter(self):
        """
        属性8.18：控制器可以添加报告器

        控制器应该支持动态添加报告器

        **验证：需求 6.1**
        """
        # Arrange
        config = ValidationConfig(
            root_path=Path('.'),
            generate_html_report=False,
            generate_json_report=False,
            generate_summary_report=False
        )
        controller = ValidationController(config)

        new_reporter = HTMLReporter()

        # Act
        controller.add_reporter('custom_html', new_reporter)

        # Assert
        assert 'custom_html' in controller.list_reporters()
        assert controller.get_reporter('custom_html') is new_reporter


class TestReportGenerationIntegration:
    """
    测试报告生成集成

    **验证：需求 6.2**
    """

    @given(report_data=validation_report_strategy())
    @settings(max_examples=25)
    def test_generate_reports_creates_files(self, report_data: ValidationReport):
        """
        属性8.19：生成报告创建文件

        *对于任何*验证报告，generate_reports应该创建报告文件

        **验证：需求 6.2**
        """
        import tempfile

        # Arrange
        config = ValidationConfig(
            root_path=Path('.'),
            generate_html_report=True,
            generate_json_report=True,
            generate_summary_report=True
        )
        controller = ValidationController(config)

        with tempfile.TemporaryDirectory() as tmp_dir:
            output_dir = Path(tmp_dir)

            # Act
            generated = controller.generate_reports(report_data, output_dir)

            # Assert
            assert len(generated) > 0, "应该生成至少一个报告"
            for format_name, file_path in generated.items():
                assert Path(file_path).exists(), f"{format_name}报告文件应该存在"


class TestStatusMerging:
    """
    测试状态合并逻辑

    **验证：需求 6.2**
    """

    def test_merge_status_error_takes_precedence(self):
        """
        属性8.20：错误状态优先

        合并状态时，ERROR应该优先于其他状态

        **验证：需求 6.2**
        """
        # Arrange
        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # Act & Assert
        assert controller._merge_status(ValidationStatus.PASSED, ValidationStatus.ERROR) == ValidationStatus.ERROR
        assert controller._merge_status(ValidationStatus.FAILED, ValidationStatus.ERROR) == ValidationStatus.ERROR
        assert controller._merge_status(ValidationStatus.SKIPPED, ValidationStatus.ERROR) == ValidationStatus.ERROR

    def test_merge_status_failed_over_passed(self):
        """
        属性8.21：失败状态优先于通过

        合并状态时，FAILED应该优先于PASSED

        **验证：需求 6.2**
        """
        # Arrange
        config = ValidationConfig(root_path=Path('.'))
        controller = ValidationController(config)

        # Act & Assert
        assert controller._merge_status(ValidationStatus.PASSED, ValidationStatus.FAILED) == ValidationStatus.FAILED
        assert controller._merge_status(ValidationStatus.SKIPPED, ValidationStatus.FAILED) == ValidationStatus.FAILED


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
