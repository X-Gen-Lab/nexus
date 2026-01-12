"""
CLI功能单元测试

测试命令行参数解析和CI集成功能。
需求：6.1, 6.5
"""

import pytest
import sys
import os
from pathlib import Path
from unittest.mock import patch, MagicMock
from argparse import Namespace

# 添加项目根目录到路径
sys.path.insert(0, str(Path(__file__).parent.parent))

from script_validation.__main__ import (
    create_argument_parser,
    setup_logging,
    detect_ci_environment,
    get_detected_ci_platform,
    main,
    EXIT_SUCCESS,
    EXIT_VALIDATION_FAILED,
    EXIT_ERROR,
)
from script_validation.ci_integration import (
    CIDetector,
    CIEnvironment,
    CIIntegration,
    CIOutputFormatter,
    ExitCode,
    is_ci_environment,
    get_ci_platform,
    get_exit_code_from_summary,
)
from script_validation.models import (
    ValidationConfig,
    ValidationSummary,
    Platform,
)


class TestArgumentParser:
    """测试命令行参数解析"""

    def test_create_parser_returns_parser(self):
        """测试创建参数解析器"""
        parser = create_argument_parser()
        assert parser is not None
        assert parser.prog == "script_validation"

    def test_default_mode_is_full(self):
        """测试默认验证模式为full"""
        parser = create_argument_parser()
        args = parser.parse_args([])
        assert args.mode == "full"

    def test_mode_choices(self):
        """测试验证模式选项"""
        parser = create_argument_parser()

        # 测试有效模式
        for mode in ["full", "quick", "platform-specific"]:
            args = parser.parse_args(["--mode", mode])
            assert args.mode == mode

    def test_platform_choices(self):
        """测试平台选项"""
        parser = create_argument_parser()

        args = parser.parse_args(["--platforms", "windows", "wsl"])
        assert args.platforms == ["windows", "wsl"]

        args = parser.parse_args(["-p", "linux"])
        assert args.platforms == ["linux"]

    def test_report_format_choices(self):
        """测试报告格式选项"""
        parser = create_argument_parser()

        args = parser.parse_args(["--report-format", "html", "json"])
        assert args.report_format == ["html", "json"]

        args = parser.parse_args(["-f", "junit"])
        assert args.report_format == ["junit"]

    def test_timeout_argument(self):
        """测试超时参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--timeout", "600"])
        assert args.timeout == 600

        # 默认值
        args = parser.parse_args([])
        assert args.timeout == 300

    def test_max_memory_argument(self):
        """测试内存限制参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--max-memory", "2048"])
        assert args.max_memory == 2048

        # 默认值
        args = parser.parse_args([])
        assert args.max_memory == 1024

    def test_ci_flag(self):
        """测试CI模式标志"""
        parser = create_argument_parser()

        args = parser.parse_args(["--ci"])
        assert args.ci is True

        args = parser.parse_args([])
        assert args.ci is False

    def test_verbose_flag(self):
        """测试详细输出标志"""
        parser = create_argument_parser()

        args = parser.parse_args(["--verbose"])
        assert args.verbose is True

        args = parser.parse_args([])
        assert args.verbose is False

    def test_no_parallel_flag(self):
        """测试禁用并行执行标志"""
        parser = create_argument_parser()

        args = parser.parse_args(["--no-parallel"])
        assert args.no_parallel is True

        args = parser.parse_args([])
        assert args.no_parallel is False

    def test_list_scripts_flag(self):
        """测试列出脚本标志"""
        parser = create_argument_parser()

        args = parser.parse_args(["--list-scripts"])
        assert args.list_scripts is True

    def test_check_platforms_flag(self):
        """测试检查平台标志"""
        parser = create_argument_parser()

        args = parser.parse_args(["--check-platforms"])
        assert args.check_platforms is True

    def test_root_path_argument(self):
        """测试根路径参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--root-path", "/some/path"])
        assert args.root_path == "/some/path"

        args = parser.parse_args(["-r", "/another/path"])
        assert args.root_path == "/another/path"

    def test_output_dir_argument(self):
        """测试输出目录参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--output-dir", "/output"])
        assert args.output_dir == "/output"

        args = parser.parse_args(["-o", "/reports"])
        assert args.output_dir == "/reports"

    def test_validators_argument(self):
        """测试验证器选择参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--validators", "functional", "performance"])
        assert args.validators == ["functional", "performance"]

    def test_patterns_argument(self):
        """测试脚本模式参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--patterns", "*.sh", "*.py"])
        assert args.patterns == ["*.sh", "*.py"]

    def test_exclude_argument(self):
        """测试排除模式参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--exclude", "test_*", "*.bak"])
        assert args.exclude == ["test_*", "*.bak"]

    def test_config_argument(self):
        """测试配置文件参数"""
        parser = create_argument_parser()

        args = parser.parse_args(["--config", "config.yaml"])
        assert args.config == "config.yaml"

        args = parser.parse_args(["-c", "settings.json"])
        assert args.config == "settings.json"


class TestCIDetector:
    """测试CI环境检测"""

    def test_detect_non_ci_environment(self):
        """测试非CI环境检测"""
        with patch.dict(os.environ, {}, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is False

    def test_detect_github_actions(self):
        """测试GitHub Actions环境检测"""
        env_vars = {
            "GITHUB_ACTIONS": "true",
            "GITHUB_RUN_ID": "12345",
            "GITHUB_RUN_NUMBER": "42",
            "GITHUB_REF_NAME": "main",
            "GITHUB_SHA": "abc123",
            "GITHUB_REPOSITORY": "owner/repo",
            "GITHUB_WORKFLOW": "CI",
            "GITHUB_JOB": "build",
            "RUNNER_OS": "Linux"
        }
        with patch.dict(os.environ, env_vars, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is True
            assert env.ci_platform == "GitHub Actions"
            assert env.build_id == "12345"
            assert env.build_number == "42"
            assert env.branch == "main"
            assert env.commit_sha == "abc123"
            assert env.repository == "owner/repo"

    def test_detect_gitlab_ci(self):
        """测试GitLab CI环境检测"""
        env_vars = {
            "GITLAB_CI": "true",
            "CI_PIPELINE_ID": "67890",
            "CI_COMMIT_REF_NAME": "develop",
            "CI_COMMIT_SHA": "def456"
        }
        with patch.dict(os.environ, env_vars, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is True
            assert env.ci_platform == "GitLab CI"
            assert env.build_id == "67890"
            assert env.branch == "develop"

    def test_detect_jenkins(self):
        """测试Jenkins环境检测"""
        env_vars = {
            "JENKINS_URL": "http://jenkins.example.com",
            "BUILD_ID": "100",
            "BUILD_NUMBER": "100",
            "GIT_BRANCH": "feature/test"
        }
        with patch.dict(os.environ, env_vars, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is True
            assert env.ci_platform == "Jenkins"
            assert env.build_number == "100"

    def test_detect_azure_devops(self):
        """测试Azure DevOps环境检测"""
        env_vars = {
            "TF_BUILD": "True",
            "BUILD_BUILDID": "999",
            "BUILD_SOURCEBRANCHNAME": "release"
        }
        with patch.dict(os.environ, env_vars, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is True
            assert env.ci_platform == "Azure DevOps"

    def test_detect_generic_ci(self):
        """测试通用CI环境检测"""
        env_vars = {"CI": "true"}
        with patch.dict(os.environ, env_vars, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is True
            assert env.ci_platform == "Generic CI"

    def test_ci_false_value_not_detected(self):
        """测试CI环境变量为false时不检测为CI"""
        env_vars = {"CI": "false"}
        with patch.dict(os.environ, env_vars, clear=True):
            env = CIDetector.detect()
            assert env.is_ci is False


class TestExitCodes:
    """测试退出代码"""

    def test_exit_code_values(self):
        """测试退出代码值"""
        assert ExitCode.SUCCESS == 0
        assert ExitCode.VALIDATION_FAILED == 1
        assert ExitCode.ERROR == 2

    def test_exit_code_constants(self):
        """测试退出代码常量"""
        assert EXIT_SUCCESS == 0
        assert EXIT_VALIDATION_FAILED == 1
        assert EXIT_ERROR == 2

    def test_get_exit_code_from_summary_success(self):
        """测试成功时的退出代码"""
        summary = ValidationSummary(
            total_scripts=10,
            passed=10,
            failed=0,
            skipped=0,
            errors=0,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(summary) == ExitCode.SUCCESS

    def test_get_exit_code_from_summary_failed(self):
        """测试失败时的退出代码"""
        summary = ValidationSummary(
            total_scripts=10,
            passed=8,
            failed=2,
            skipped=0,
            errors=0,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(summary) == ExitCode.VALIDATION_FAILED

    def test_get_exit_code_from_summary_error(self):
        """测试错误时的退出代码"""
        summary = ValidationSummary(
            total_scripts=10,
            passed=8,
            failed=0,
            skipped=0,
            errors=2,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(summary) == ExitCode.ERROR

    def test_error_takes_precedence_over_failed(self):
        """测试错误优先于失败"""
        summary = ValidationSummary(
            total_scripts=10,
            passed=5,
            failed=3,
            skipped=0,
            errors=2,
            execution_time=5.0
        )
        assert get_exit_code_from_summary(summary) == ExitCode.ERROR


class TestCIIntegration:
    """测试CI集成功能"""

    def test_ci_integration_initialization(self):
        """测试CI集成初始化"""
        with patch.dict(os.environ, {}, clear=True):
            ci = CIIntegration()
            assert ci.is_ci is False

    def test_ci_integration_with_ci_env(self):
        """测试CI环境下的CI集成"""
        env_vars = {"GITHUB_ACTIONS": "true"}
        with patch.dict(os.environ, env_vars, clear=True):
            ci = CIIntegration()
            assert ci.is_ci is True
            assert ci.platform == "GitHub Actions"

    def test_get_ci_environment_info(self):
        """测试获取CI环境信息"""
        env_vars = {
            "GITHUB_ACTIONS": "true",
            "GITHUB_REPOSITORY": "test/repo"
        }
        with patch.dict(os.environ, env_vars, clear=True):
            ci = CIIntegration()
            info = ci.get_ci_environment_info()
            assert info["is_ci"] == "True"
            assert info["ci_platform"] == "GitHub Actions"
            assert info["repository"] == "test/repo"


class TestCIOutputFormatter:
    """测试CI输出格式化"""

    def test_github_actions_error_format(self):
        """测试GitHub Actions错误格式"""
        env = CIEnvironment(is_ci=True, ci_platform="GitHub Actions")
        formatter = CIOutputFormatter(env)

        result = formatter.format_error("Test error")
        assert "::error" in result
        assert "Test error" in result

    def test_github_actions_warning_format(self):
        """测试GitHub Actions警告格式"""
        env = CIEnvironment(is_ci=True, ci_platform="GitHub Actions")
        formatter = CIOutputFormatter(env)

        result = formatter.format_warning("Test warning")
        assert "::warning" in result
        assert "Test warning" in result

    def test_github_actions_notice_format(self):
        """测试GitHub Actions通知格式"""
        env = CIEnvironment(is_ci=True, ci_platform="GitHub Actions")
        formatter = CIOutputFormatter(env)

        result = formatter.format_notice("Test notice")
        assert "::notice::" in result
        assert "Test notice" in result

    def test_github_actions_group_format(self):
        """测试GitHub Actions分组格式"""
        env = CIEnvironment(is_ci=True, ci_platform="GitHub Actions")
        formatter = CIOutputFormatter(env)

        start = formatter.format_group_start("Test Group")
        assert "::group::" in start

        end = formatter.format_group_end()
        assert "::endgroup::" in end

    def test_azure_devops_error_format(self):
        """测试Azure DevOps错误格式"""
        env = CIEnvironment(is_ci=True, ci_platform="Azure DevOps")
        formatter = CIOutputFormatter(env)

        result = formatter.format_error("Test error")
        assert "##vso[task.logissue type=error]" in result

    def test_generic_error_format(self):
        """测试通用错误格式"""
        env = CIEnvironment(is_ci=True, ci_platform="Generic CI")
        formatter = CIOutputFormatter(env)

        result = formatter.format_error("Test error")
        assert "ERROR:" in result


class TestConvenienceFunctions:
    """测试便捷函数"""

    def test_is_ci_environment_false(self):
        """测试非CI环境"""
        with patch.dict(os.environ, {}, clear=True):
            assert is_ci_environment() is False

    def test_is_ci_environment_true(self):
        """测试CI环境"""
        with patch.dict(os.environ, {"CI": "true"}, clear=True):
            assert is_ci_environment() is True

    def test_get_ci_platform_unknown(self):
        """测试非CI环境返回unknown"""
        with patch.dict(os.environ, {}, clear=True):
            assert get_ci_platform() == "unknown"

    def test_get_ci_platform_github(self):
        """测试GitHub Actions平台"""
        with patch.dict(os.environ, {"GITHUB_ACTIONS": "true"}, clear=True):
            assert get_ci_platform() == "GitHub Actions"

    def test_detect_ci_environment_function(self):
        """测试detect_ci_environment函数"""
        with patch.dict(os.environ, {}, clear=True):
            assert detect_ci_environment() is False

        with patch.dict(os.environ, {"CI": "true"}, clear=True):
            assert detect_ci_environment() is True

    def test_get_detected_ci_platform_function(self):
        """测试get_detected_ci_platform函数"""
        with patch.dict(os.environ, {"GITLAB_CI": "true"}, clear=True):
            assert get_detected_ci_platform() == "GitLab CI"


class TestValidationConfigFromArgs:
    """测试从命令行参数创建配置"""

    def test_config_from_args_defaults(self):
        """测试默认配置"""
        parser = create_argument_parser()
        args = parser.parse_args([])
        config = ValidationConfig.from_args(args)

        assert config.validation_mode == "full"
        assert config.timeout_seconds == 300
        assert config.max_memory_mb == 1024
        assert config.parallel_execution is True
        assert config.ci_mode is False

    def test_config_from_args_custom_values(self):
        """测试自定义配置"""
        parser = create_argument_parser()
        args = parser.parse_args([
            "--mode", "quick",
            "--timeout", "600",
            "--max-memory", "2048",
            "--no-parallel",
            "--ci",
            "--verbose"
        ])
        config = ValidationConfig.from_args(args)

        assert config.validation_mode == "quick"
        assert config.timeout_seconds == 600
        assert config.max_memory_mb == 2048
        assert config.parallel_execution is False
        assert config.ci_mode is True
        assert config.verbose is True

    def test_config_from_args_platforms(self):
        """测试平台配置"""
        parser = create_argument_parser()
        args = parser.parse_args(["--platforms", "windows", "linux"])
        config = ValidationConfig.from_args(args)

        assert Platform.WINDOWS in config.target_platforms
        assert Platform.LINUX in config.target_platforms
        assert Platform.WSL not in config.target_platforms

    def test_config_from_args_validators(self):
        """测试验证器配置"""
        parser = create_argument_parser()
        args = parser.parse_args(["--validators", "functional", "performance"])
        config = ValidationConfig.from_args(args)

        assert "functional" in config.enabled_validators
        assert "performance" in config.enabled_validators
        assert "compatibility" not in config.enabled_validators

    def test_config_from_args_report_formats(self):
        """测试报告格式配置"""
        parser = create_argument_parser()
        args = parser.parse_args(["--report-format", "html", "junit"])
        config = ValidationConfig.from_args(args)

        assert config.generate_html_report is True
        assert config.generate_junit_report is True
        assert config.generate_json_report is False


class TestSetupLogging:
    """测试日志配置"""

    def test_setup_logging_normal_mode(self):
        """测试普通模式日志配置"""
        # 不应抛出异常
        setup_logging(verbose=False, ci_mode=False)

    def test_setup_logging_verbose_mode(self):
        """测试详细模式日志配置"""
        setup_logging(verbose=True, ci_mode=False)

    def test_setup_logging_ci_mode(self):
        """测试CI模式日志配置"""
        setup_logging(verbose=False, ci_mode=True)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
