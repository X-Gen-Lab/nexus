"""
脚本交付验证系统 - 命令行入口点

提供命令行界面用于执行脚本验证、生成报告和CI集成。

使用方式:
    python -m script_validation [options]
    python -m script_validation --help

示例:
    # 完整验证
    python -m script_validation --mode full

    # 快速验证
    python -m script_validation --mode quick

    # 指定平台验证
    python -m script_validation --platforms windows wsl

    # 生成特定格式报告
    python -m script_validation --report-format html json

    # CI模式
    python -m script_validation --ci

    # 生成JUnit XML报告（用于CI集成）
    python -m script_validation --ci --report-format junit
"""

import argparse
import sys
import os
import logging
from pathlib import Path
from typing import List, Optional

from .models import ValidationConfig, Platform
from .controllers import ValidationController
from .ci_integration import (
    CIIntegration,
    CIDetector,
    ExitCode,
    is_ci_environment,
    get_ci_platform,
)


# 退出代码常量
EXIT_SUCCESS = ExitCode.SUCCESS
EXIT_VALIDATION_FAILED = ExitCode.VALIDATION_FAILED
EXIT_ERROR = ExitCode.ERROR


def setup_logging(verbose: bool = False, ci_mode: bool = False) -> None:
    """配置日志系统

    Args:
        verbose: 是否启用详细输出
        ci_mode: 是否为CI模式
    """
    level = logging.DEBUG if verbose else logging.INFO

    # CI模式下使用简化的日志格式
    if ci_mode:
        log_format = "%(levelname)s: %(message)s"
    else:
        log_format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

    logging.basicConfig(
        level=level,
        format=log_format,
        handlers=[logging.StreamHandler(sys.stdout)]
    )


def create_argument_parser() -> argparse.ArgumentParser:
    """创建命令行参数解析器

    Returns:
        argparse.ArgumentParser: 配置好的参数解析器
    """
    parser = argparse.ArgumentParser(
        prog="script_validation",
        description="脚本交付验证系统 - 跨平台脚本验证框架",
        epilog="更多信息请参阅项目文档。",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # 基本选项
    parser.add_argument(
        "--root-path", "-r",
        dest="root_path",
        type=str,
        default=None,
        help="项目根目录路径 (默认: 当前目录)"
    )

    # 验证模式
    parser.add_argument(
        "--mode", "-m",
        dest="mode",
        type=str,
        choices=["full", "quick", "platform-specific"],
        default="full",
        help="验证模式: full(完整), quick(快速), platform-specific(平台特定) (默认: full)"
    )

    # 平台选择
    parser.add_argument(
        "--platforms", "-p",
        dest="platforms",
        nargs="+",
        type=str,
        choices=["windows", "wsl", "linux"],
        default=None,
        help="目标验证平台 (默认: 所有可用平台)"
    )

    # 报告格式
    parser.add_argument(
        "--report-format", "-f",
        dest="report_format",
        nargs="+",
        type=str,
        choices=["html", "json", "summary", "junit", "all"],
        default=["all"],
        help="报告输出格式 (默认: all)"
    )

    # 输出目录
    parser.add_argument(
        "--output-dir", "-o",
        dest="output_dir",
        type=str,
        default=None,
        help="报告输出目录 (默认: ./validation_reports)"
    )

    # 验证器选择
    parser.add_argument(
        "--validators", "-v",
        dest="validators",
        nargs="+",
        type=str,
        choices=["functional", "compatibility", "performance", "documentation"],
        default=None,
        help="启用的验证器 (默认: 所有验证器)"
    )

    # 脚本模式
    parser.add_argument(
        "--patterns",
        dest="patterns",
        nargs="+",
        type=str,
        default=None,
        help="脚本文件匹配模式 (默认: *.bat *.ps1 *.sh *.py)"
    )

    # 排除模式
    parser.add_argument(
        "--exclude",
        dest="exclude",
        nargs="+",
        type=str,
        default=None,
        help="排除的文件模式"
    )

    # 超时设置
    parser.add_argument(
        "--timeout",
        dest="timeout",
        type=int,
        default=300,
        help="脚本执行超时时间(秒) (默认: 300)"
    )

    # 内存限制
    parser.add_argument(
        "--max-memory",
        dest="max_memory",
        type=int,
        default=1024,
        help="最大内存使用限制(MB) (默认: 1024)"
    )

    # 并行执行
    parser.add_argument(
        "--no-parallel",
        dest="no_parallel",
        action="store_true",
        default=False,
        help="禁用并行执行"
    )

    # 配置文件
    parser.add_argument(
        "--config", "-c",
        dest="config",
        type=str,
        default=None,
        help="配置文件路径 (支持 YAML/JSON)"
    )

    # CI模式
    parser.add_argument(
        "--ci",
        dest="ci",
        action="store_true",
        default=False,
        help="CI模式 (简化输出，适当退出代码)"
    )

    # 详细输出
    parser.add_argument(
        "--verbose",
        dest="verbose",
        action="store_true",
        default=False,
        help="启用详细输出"
    )

    # 版本信息
    parser.add_argument(
        "--version",
        action="version",
        version="%(prog)s 1.0.0"
    )

    # 仅列出脚本
    parser.add_argument(
        "--list-scripts",
        dest="list_scripts",
        action="store_true",
        default=False,
        help="仅列出发现的脚本，不执行验证"
    )

    # 仅检查平台
    parser.add_argument(
        "--check-platforms",
        dest="check_platforms",
        action="store_true",
        default=False,
        help="仅检查平台可用性，不执行验证"
    )

    return parser


def detect_ci_environment() -> bool:
    """检测是否在CI环境中运行

    使用CI集成模块进行全面的CI环境检测。

    Returns:
        bool: 是否在CI环境中
    """
    return is_ci_environment()


def get_detected_ci_platform() -> str:
    """获取检测到的CI平台名称

    Returns:
        str: CI平台名称
    """
    return get_ci_platform()


def list_scripts(config: ValidationConfig) -> int:
    """列出发现的脚本

    Args:
        config: 验证配置

    Returns:
        int: 退出代码
    """
    from .managers import ScriptManager

    try:
        script_manager = ScriptManager()
        scripts = script_manager.discover_scripts(
            config.root_path,
            config.script_patterns
        )

        print(f"\n发现 {len(scripts)} 个脚本:\n")
        print("-" * 60)

        for script in scripts:
            print(f"  {script.name}")
            print(f"    路径: {script.path}")
            print(f"    类型: {script.type.value}")
            print(f"    分类: {script.category.value}")
            print()

        return EXIT_SUCCESS

    except Exception as e:
        logging.error(f"列出脚本失败: {e}")
        return EXIT_ERROR


def check_platforms(config: ValidationConfig) -> int:
    """检查平台可用性

    Args:
        config: 验证配置

    Returns:
        int: 退出代码
    """
    from .managers import PlatformManager

    try:
        platform_manager = PlatformManager()

        print("\n平台可用性检查:\n")
        print("-" * 40)

        all_available = True
        for platform in config.target_platforms:
            available = platform_manager.is_platform_available(platform)
            status = "✓ 可用" if available else "✗ 不可用"
            print(f"  {platform.value}: {status}")

            if not available:
                all_available = False

        print()

        # 显示当前环境信息
        env_info = platform_manager.get_current_environment_info()
        print("当前环境信息:")
        print(f"  平台: {env_info.platform.value}")
        print(f"  操作系统版本: {env_info.os_version}")
        print(f"  Python版本: {env_info.python_version}")
        print(f"  Shell版本: {env_info.shell_version}")
        print()

        return EXIT_SUCCESS if all_available else EXIT_VALIDATION_FAILED

    except Exception as e:
        logging.error(f"检查平台失败: {e}")
        return EXIT_ERROR


def run_validation(config: ValidationConfig) -> int:
    """运行验证流程

    Args:
        config: 验证配置

    Returns:
        int: 退出代码
    """
    logger = logging.getLogger(__name__)

    # 初始化CI集成
    ci_integration = CIIntegration()

    try:
        # 验证配置
        config_errors = config.validate()
        if config_errors:
            for error in config_errors:
                logger.error(f"配置错误: {error}")
            return EXIT_ERROR

        # 创建验证控制器
        controller = ValidationController(config)

        # 运行验证
        logger.info("开始验证流程...")
        report = controller.run_validation()

        # 生成报告
        output_dir = config.output_dir or (config.root_path / "validation_reports")
        generated_reports = controller.generate_reports(report, output_dir)

        # 如果配置了JUnit格式，额外生成JUnit报告
        if hasattr(config, 'generate_junit_report') and config.generate_junit_report:
            from .reporters import JUnitReporter
            junit_reporter = JUnitReporter()
            junit_content = junit_reporter.generate_report(report)
            junit_path = output_dir / f"validation_report_{report.timestamp.strftime('%Y%m%d_%H%M%S')}.xml"
            if junit_reporter.save_report(junit_content, str(junit_path)):
                generated_reports['junit'] = str(junit_path)
                logger.info(f"Generated JUnit report: {junit_path}")

        # 输出摘要
        if config.ci_mode and ci_integration.is_ci:
            # CI模式下使用CI特定的输出格式
            ci_integration.print_ci_summary(report)
            # 写入GitHub Actions输出（如果适用）
            ci_integration.write_github_output(report)
        else:
            print_summary(report, generated_reports, config.ci_mode)

        # 返回适当的退出代码
        return ci_integration.get_exit_code(report)

    except Exception as e:
        logger.error(f"验证失败: {e}")
        if config.verbose:
            import traceback
            traceback.print_exc()
        return EXIT_ERROR


def print_summary(report, generated_reports: dict, ci_mode: bool = False) -> None:
    """打印验证摘要

    Args:
        report: 验证报告
        generated_reports: 生成的报告文件路径
        ci_mode: 是否为CI模式
    """
    summary = report.summary

    print("\n" + "=" * 60)
    print("验证摘要")
    print("=" * 60)

    print(f"\n总脚本数: {summary.total_scripts}")
    print(f"  ✓ 通过: {summary.passed}")
    print(f"  ✗ 失败: {summary.failed}")
    print(f"  ○ 跳过: {summary.skipped}")
    print(f"  ! 错误: {summary.errors}")
    print(f"\n总执行时间: {summary.execution_time:.2f}秒")

    # 计算通过率
    if summary.total_scripts > 0:
        pass_rate = (summary.passed / summary.total_scripts) * 100
        print(f"通过率: {pass_rate:.1f}%")

    # 显示生成的报告
    if generated_reports:
        print("\n生成的报告:")
        for format_name, path in generated_reports.items():
            print(f"  {format_name}: {path}")

    # 显示建议
    if report.recommendations and not ci_mode:
        print("\n建议:")
        for rec in report.recommendations:
            print(f"  • {rec}")

    print("\n" + "=" * 60)

    # CI模式下的简化输出
    if ci_mode:
        if summary.failed > 0 or summary.errors > 0:
            print("::error::验证失败")
        else:
            print("::notice::验证通过")


def main(args: Optional[List[str]] = None) -> int:
    """主入口函数

    Args:
        args: 命令行参数列表，None表示使用sys.argv

    Returns:
        int: 退出代码
    """
    # 解析命令行参数
    parser = create_argument_parser()
    parsed_args = parser.parse_args(args)

    # 检测CI环境
    ci_detected = detect_ci_environment()
    if ci_detected and not parsed_args.ci:
        parsed_args.ci = True

    # 配置日志
    setup_logging(parsed_args.verbose, parsed_args.ci)
    logger = logging.getLogger(__name__)

    # 如果在CI环境中，记录检测到的平台
    if ci_detected:
        ci_platform = get_detected_ci_platform()
        logger.info(f"Detected CI environment: {ci_platform}")

    # 创建配置
    try:
        config = ValidationConfig.from_args(parsed_args)

        # 检查是否需要生成JUnit报告
        if hasattr(parsed_args, 'report_format') and parsed_args.report_format:
            formats = parsed_args.report_format if isinstance(parsed_args.report_format, list) else [parsed_args.report_format]
            config.generate_junit_report = 'junit' in formats or 'all' in formats
        else:
            config.generate_junit_report = False

    except Exception as e:
        logger.error(f"配置创建失败: {e}")
        return EXIT_ERROR

    # 处理特殊命令
    if parsed_args.list_scripts:
        return list_scripts(config)

    if parsed_args.check_platforms:
        return check_platforms(config)

    # 运行验证
    return run_validation(config)


if __name__ == "__main__":
    sys.exit(main())
