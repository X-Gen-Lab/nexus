"""
CI集成模块

提供CI/CD系统集成功能，包括环境检测、退出代码管理和CI特定输出格式。
支持Jenkins、GitHub Actions、GitLab CI、Azure DevOps等主流CI平台。
"""

import os
import sys
import logging
from enum import IntEnum
from dataclasses import dataclass
from typing import Dict, List, Optional, Any
from pathlib import Path

from .models import ValidationReport, ValidationStatus, ValidationSummary


# 设置日志
logger = logging.getLogger(__name__)


class ExitCode(IntEnum):
    """CI退出代码

    标准化的退出代码，用于CI/CD系统判断构建状态。

    Attributes:
        SUCCESS: 验证成功，所有脚本通过
        VALIDATION_FAILED: 验证失败，存在失败的脚本
        ERROR: 执行错误，系统或配置问题
    """
    SUCCESS = 0
    VALIDATION_FAILED = 1
    ERROR = 2


@dataclass
class CIEnvironment:
    """CI环境信息

    存储检测到的CI环境详细信息。

    Attributes:
        is_ci: 是否在CI环境中运行
        ci_platform: CI平台名称
        build_id: 构建ID
        build_number: 构建编号
        branch: 分支名称
        commit_sha: 提交SHA
        pull_request: PR编号（如果适用）
        repository: 仓库名称
        workflow: 工作流名称
        job: 作业名称
        runner_os: 运行器操作系统
        additional_info: 其他CI特定信息
    """
    is_ci: bool = False
    ci_platform: str = "unknown"
    build_id: str = ""
    build_number: str = ""
    branch: str = ""
    commit_sha: str = ""
    pull_request: str = ""
    repository: str = ""
    workflow: str = ""
    job: str = ""
    runner_os: str = ""
    additional_info: Dict[str, str] = None

    def __post_init__(self):
        if self.additional_info is None:
            self.additional_info = {}


class CIDetector:
    """CI环境检测器

    检测当前运行环境是否为CI/CD系统，并识别具体的CI平台。
    """

    # CI环境变量映射
    CI_ENVIRONMENTS = {
        "github_actions": {
            "detect_vars": ["GITHUB_ACTIONS"],
            "platform_name": "GitHub Actions",
            "env_mapping": {
                "build_id": "GITHUB_RUN_ID",
                "build_number": "GITHUB_RUN_NUMBER",
                "branch": "GITHUB_REF_NAME",
                "commit_sha": "GITHUB_SHA",
                "repository": "GITHUB_REPOSITORY",
                "workflow": "GITHUB_WORKFLOW",
                "job": "GITHUB_JOB",
                "runner_os": "RUNNER_OS",
                "pull_request": "GITHUB_EVENT_NAME"  # 需要额外处理
            }
        },
        "gitlab_ci": {
            "detect_vars": ["GITLAB_CI"],
            "platform_name": "GitLab CI",
            "env_mapping": {
                "build_id": "CI_PIPELINE_ID",
                "build_number": "CI_PIPELINE_IID",
                "branch": "CI_COMMIT_REF_NAME",
                "commit_sha": "CI_COMMIT_SHA",
                "repository": "CI_PROJECT_PATH",
                "job": "CI_JOB_NAME"
            }
        },
        "jenkins": {
            "detect_vars": ["JENKINS_URL", "JENKINS_HOME"],
            "platform_name": "Jenkins",
            "env_mapping": {
                "build_id": "BUILD_ID",
                "build_number": "BUILD_NUMBER",
                "branch": "GIT_BRANCH",
                "commit_sha": "GIT_COMMIT",
                "job": "JOB_NAME"
            }
        },
        "azure_devops": {
            "detect_vars": ["AZURE_PIPELINES", "TF_BUILD"],
            "platform_name": "Azure DevOps",
            "env_mapping": {
                "build_id": "BUILD_BUILDID",
                "build_number": "BUILD_BUILDNUMBER",
                "branch": "BUILD_SOURCEBRANCHNAME",
                "commit_sha": "BUILD_SOURCEVERSION",
                "repository": "BUILD_REPOSITORY_NAME"
            }
        },
        "travis": {
            "detect_vars": ["TRAVIS"],
            "platform_name": "Travis CI",
            "env_mapping": {
                "build_id": "TRAVIS_BUILD_ID",
                "build_number": "TRAVIS_BUILD_NUMBER",
                "branch": "TRAVIS_BRANCH",
                "commit_sha": "TRAVIS_COMMIT",
                "repository": "TRAVIS_REPO_SLUG",
                "pull_request": "TRAVIS_PULL_REQUEST"
            }
        },
        "circleci": {
            "detect_vars": ["CIRCLECI"],
            "platform_name": "CircleCI",
            "env_mapping": {
                "build_number": "CIRCLE_BUILD_NUM",
                "branch": "CIRCLE_BRANCH",
                "commit_sha": "CIRCLE_SHA1",
                "repository": "CIRCLE_PROJECT_REPONAME",
                "pull_request": "CIRCLE_PULL_REQUEST"
            }
        },
        "teamcity": {
            "detect_vars": ["TEAMCITY_VERSION"],
            "platform_name": "TeamCity",
            "env_mapping": {
                "build_number": "BUILD_NUMBER"
            }
        },
        "buildkite": {
            "detect_vars": ["BUILDKITE"],
            "platform_name": "Buildkite",
            "env_mapping": {
                "build_id": "BUILDKITE_BUILD_ID",
                "build_number": "BUILDKITE_BUILD_NUMBER",
                "branch": "BUILDKITE_BRANCH",
                "commit_sha": "BUILDKITE_COMMIT",
                "repository": "BUILDKITE_REPO",
                "pull_request": "BUILDKITE_PULL_REQUEST"
            }
        },
        "bitbucket": {
            "detect_vars": ["BITBUCKET_BUILD_NUMBER"],
            "platform_name": "Bitbucket Pipelines",
            "env_mapping": {
                "build_number": "BITBUCKET_BUILD_NUMBER",
                "branch": "BITBUCKET_BRANCH",
                "commit_sha": "BITBUCKET_COMMIT",
                "repository": "BITBUCKET_REPO_SLUG",
                "pull_request": "BITBUCKET_PR_ID"
            }
        },
        "generic_ci": {
            "detect_vars": ["CI"],
            "platform_name": "Generic CI",
            "env_mapping": {}
        }
    }

    @classmethod
    def detect(cls) -> CIEnvironment:
        """检测CI环境

        Returns:
            CIEnvironment: CI环境信息
        """
        # 按优先级检测各CI平台
        for ci_key, ci_config in cls.CI_ENVIRONMENTS.items():
            if ci_key == "generic_ci":
                continue  # 最后检测通用CI

            if cls._is_ci_platform(ci_config["detect_vars"]):
                return cls._build_ci_environment(ci_config)

        # 检测通用CI环境
        if cls._is_ci_platform(["CI"]):
            return cls._build_ci_environment(cls.CI_ENVIRONMENTS["generic_ci"])

        # 非CI环境
        return CIEnvironment(is_ci=False)

    @classmethod
    def _is_ci_platform(cls, detect_vars: List[str]) -> bool:
        """检查是否为特定CI平台

        Args:
            detect_vars: 用于检测的环境变量列表

        Returns:
            是否检测到该CI平台
        """
        for var in detect_vars:
            value = os.environ.get(var, "").lower()
            if value and value not in ("false", "0", "no"):
                return True
        return False

    @classmethod
    def _build_ci_environment(cls, ci_config: Dict[str, Any]) -> CIEnvironment:
        """构建CI环境信息

        Args:
            ci_config: CI配置字典

        Returns:
            CIEnvironment: CI环境信息
        """
        env_mapping = ci_config.get("env_mapping", {})

        return CIEnvironment(
            is_ci=True,
            ci_platform=ci_config["platform_name"],
            build_id=os.environ.get(env_mapping.get("build_id", ""), ""),
            build_number=os.environ.get(env_mapping.get("build_number", ""), ""),
            branch=os.environ.get(env_mapping.get("branch", ""), ""),
            commit_sha=os.environ.get(env_mapping.get("commit_sha", ""), ""),
            pull_request=os.environ.get(env_mapping.get("pull_request", ""), ""),
            repository=os.environ.get(env_mapping.get("repository", ""), ""),
            workflow=os.environ.get(env_mapping.get("workflow", ""), ""),
            job=os.environ.get(env_mapping.get("job", ""), ""),
            runner_os=os.environ.get(env_mapping.get("runner_os", ""), "")
        )


class CIOutputFormatter:
    """CI输出格式化器

    根据不同的CI平台格式化输出消息，支持平台特定的注解格式。
    """

    def __init__(self, ci_env: CIEnvironment):
        """初始化输出格式化器

        Args:
            ci_env: CI环境信息
        """
        self.ci_env = ci_env

    def format_error(self, message: str, file: str = None, line: int = None) -> str:
        """格式化错误消息

        Args:
            message: 错误消息
            file: 文件路径（可选）
            line: 行号（可选）

        Returns:
            格式化的错误消息
        """
        if self.ci_env.ci_platform == "GitHub Actions":
            return self._github_annotation("error", message, file, line)
        elif self.ci_env.ci_platform == "GitLab CI":
            return f"ERROR: {message}"
        elif self.ci_env.ci_platform == "Azure DevOps":
            return f"##vso[task.logissue type=error]{message}"
        else:
            return f"ERROR: {message}"

    def format_warning(self, message: str, file: str = None, line: int = None) -> str:
        """格式化警告消息

        Args:
            message: 警告消息
            file: 文件路径（可选）
            line: 行号（可选）

        Returns:
            格式化的警告消息
        """
        if self.ci_env.ci_platform == "GitHub Actions":
            return self._github_annotation("warning", message, file, line)
        elif self.ci_env.ci_platform == "Azure DevOps":
            return f"##vso[task.logissue type=warning]{message}"
        else:
            return f"WARNING: {message}"

    def format_notice(self, message: str) -> str:
        """格式化通知消息

        Args:
            message: 通知消息

        Returns:
            格式化的通知消息
        """
        if self.ci_env.ci_platform == "GitHub Actions":
            return f"::notice::{message}"
        else:
            return f"INFO: {message}"

    def format_group_start(self, title: str) -> str:
        """格式化分组开始

        Args:
            title: 分组标题

        Returns:
            格式化的分组开始标记
        """
        if self.ci_env.ci_platform == "GitHub Actions":
            return f"::group::{title}"
        elif self.ci_env.ci_platform == "Azure DevOps":
            return f"##[group]{title}"
        elif self.ci_env.ci_platform == "GitLab CI":
            return f"\\e[0Ksection_start:{int(__import__('time').time())}:{title.replace(' ', '_')}\\r\\e[0K{title}"
        else:
            return f"=== {title} ==="

    def format_group_end(self, title: str = "") -> str:
        """格式化分组结束

        Args:
            title: 分组标题（某些平台需要）

        Returns:
            格式化的分组结束标记
        """
        if self.ci_env.ci_platform == "GitHub Actions":
            return "::endgroup::"
        elif self.ci_env.ci_platform == "Azure DevOps":
            return "##[endgroup]"
        elif self.ci_env.ci_platform == "GitLab CI":
            return f"\\e[0Ksection_end:{int(__import__('time').time())}:{title.replace(' ', '_')}\\r\\e[0K"
        else:
            return ""

    def format_set_output(self, name: str, value: str) -> str:
        """格式化设置输出变量

        Args:
            name: 变量名
            value: 变量值

        Returns:
            格式化的设置输出命令
        """
        if self.ci_env.ci_platform == "GitHub Actions":
            # GitHub Actions 使用环境文件设置输出
            return f"{name}={value}"
        elif self.ci_env.ci_platform == "Azure DevOps":
            return f"##vso[task.setvariable variable={name}]{value}"
        else:
            return f"{name}={value}"

    def _github_annotation(
        self,
        level: str,
        message: str,
        file: str = None,
        line: int = None
    ) -> str:
        """生成GitHub Actions注解

        Args:
            level: 级别（error, warning, notice）
            message: 消息内容
            file: 文件路径
            line: 行号

        Returns:
            GitHub Actions注解格式的字符串
        """
        parts = [f"::{level}"]

        params = []
        if file:
            params.append(f"file={file}")
        if line:
            params.append(f"line={line}")

        if params:
            parts.append(" " + ",".join(params))

        parts.append(f"::{message}")

        return "".join(parts)


class CIIntegration:
    """CI集成管理器

    提供完整的CI/CD集成功能，包括环境检测、输出格式化和退出代码管理。
    """

    def __init__(self):
        """初始化CI集成管理器"""
        self.ci_env = CIDetector.detect()
        self.formatter = CIOutputFormatter(self.ci_env)

        if self.ci_env.is_ci:
            logger.info(f"Detected CI environment: {self.ci_env.ci_platform}")

    @property
    def is_ci(self) -> bool:
        """是否在CI环境中运行"""
        return self.ci_env.is_ci

    @property
    def platform(self) -> str:
        """CI平台名称"""
        return self.ci_env.ci_platform

    def get_exit_code(self, report: ValidationReport) -> int:
        """根据验证报告获取退出代码

        Args:
            report: 验证报告

        Returns:
            退出代码
        """
        if report.summary.errors > 0:
            return ExitCode.ERROR
        elif report.summary.failed > 0:
            return ExitCode.VALIDATION_FAILED
        else:
            return ExitCode.SUCCESS

    def print_ci_summary(self, report: ValidationReport) -> None:
        """打印CI格式的摘要

        Args:
            report: 验证报告
        """
        summary = report.summary

        # 打印分组开始
        print(self.formatter.format_group_start("Validation Summary"))

        # 打印统计信息
        print(f"Total Scripts: {summary.total_scripts}")
        print(f"Passed: {summary.passed}")
        print(f"Failed: {summary.failed}")
        print(f"Skipped: {summary.skipped}")
        print(f"Errors: {summary.errors}")
        print(f"Execution Time: {summary.execution_time:.2f}s")

        # 计算通过率
        if summary.total_scripts > 0:
            pass_rate = (summary.passed / summary.total_scripts) * 100
            print(f"Pass Rate: {pass_rate:.1f}%")

        # 打印分组结束
        print(self.formatter.format_group_end("Validation Summary"))

        # 打印失败详情
        if summary.failed > 0 or summary.errors > 0:
            self._print_failure_details(report)

        # 打印最终状态
        exit_code = self.get_exit_code(report)
        if exit_code == ExitCode.SUCCESS:
            print(self.formatter.format_notice("Validation passed"))
        elif exit_code == ExitCode.VALIDATION_FAILED:
            print(self.formatter.format_error("Validation failed"))
        else:
            print(self.formatter.format_error("Validation encountered errors"))

    def _print_failure_details(self, report: ValidationReport) -> None:
        """打印失败详情

        Args:
            report: 验证报告
        """
        print(self.formatter.format_group_start("Failure Details"))

        for result in report.results:
            if result.status in [ValidationStatus.FAILED, ValidationStatus.ERROR]:
                script_path = str(result.script.path)
                message = f"{result.script.name} ({result.platform.value}): {result.error or 'Validation failed'}"

                if result.status == ValidationStatus.ERROR:
                    print(self.formatter.format_error(message, file=script_path))
                else:
                    print(self.formatter.format_warning(message, file=script_path))

        print(self.formatter.format_group_end("Failure Details"))

    def write_github_output(self, report: ValidationReport, output_file: str = None) -> bool:
        """写入GitHub Actions输出

        Args:
            report: 验证报告
            output_file: 输出文件路径（默认使用GITHUB_OUTPUT环境变量）

        Returns:
            是否成功写入
        """
        if self.ci_env.ci_platform != "GitHub Actions":
            return False

        output_file = output_file or os.environ.get("GITHUB_OUTPUT")
        if not output_file:
            return False

        try:
            summary = report.summary
            exit_code = self.get_exit_code(report)
            pass_rate = (summary.passed / summary.total_scripts * 100) if summary.total_scripts > 0 else 0

            with open(output_file, "a", encoding="utf-8") as f:
                f.write(f"validation_status={'success' if exit_code == 0 else 'failure'}\n")
                f.write(f"total_scripts={summary.total_scripts}\n")
                f.write(f"passed={summary.passed}\n")
                f.write(f"failed={summary.failed}\n")
                f.write(f"errors={summary.errors}\n")
                f.write(f"pass_rate={pass_rate:.1f}\n")
                f.write(f"execution_time={summary.execution_time:.2f}\n")

            return True
        except Exception as e:
            logger.error(f"Failed to write GitHub output: {e}")
            return False

    def get_ci_environment_info(self) -> Dict[str, str]:
        """获取CI环境信息字典

        Returns:
            CI环境信息字典
        """
        return {
            "is_ci": str(self.ci_env.is_ci),
            "ci_platform": self.ci_env.ci_platform,
            "build_id": self.ci_env.build_id,
            "build_number": self.ci_env.build_number,
            "branch": self.ci_env.branch,
            "commit_sha": self.ci_env.commit_sha,
            "repository": self.ci_env.repository,
            "workflow": self.ci_env.workflow,
            "job": self.ci_env.job
        }


def is_ci_environment() -> bool:
    """检测是否在CI环境中运行

    便捷函数，用于快速检测CI环境。

    Returns:
        是否在CI环境中
    """
    return CIDetector.detect().is_ci


def get_ci_platform() -> str:
    """获取CI平台名称

    便捷函数，用于快速获取CI平台名称。

    Returns:
        CI平台名称，非CI环境返回"unknown"
    """
    env = CIDetector.detect()
    return env.ci_platform if env.is_ci else "unknown"


def get_exit_code_from_summary(summary: ValidationSummary) -> int:
    """根据验证摘要获取退出代码

    便捷函数，用于快速获取退出代码。

    Args:
        summary: 验证摘要

    Returns:
        退出代码
    """
    if summary.errors > 0:
        return ExitCode.ERROR
    elif summary.failed > 0:
        return ExitCode.VALIDATION_FAILED
    else:
        return ExitCode.SUCCESS
