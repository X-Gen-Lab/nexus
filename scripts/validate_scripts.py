#!/usr/bin/env python3
"""
端到端脚本验证入口脚本

提供完整的脚本验证工作流程，支持多平台兼容性验证。
这是脚本交付验证系统的主要入口点，用于验证项目中所有脚本的功能性、
兼容性和可靠性。

使用方式:
    python scripts/validate_scripts.py [options]
    python scripts/validate_scripts.py --help

示例:
    # 完整验证
    python scripts/validate_scripts.py

    # 快速验证
    python scripts/validate_scripts.py --quick

    # 指定平台验证
    python scripts/validate_scripts.py --platform windows

    # 生成所有报告格式
    python scripts/validate_scripts.py --report-format all

    # CI模式
    python scripts/validate_scripts.py --ci

    # 仅列出脚本
    python scripts/validate_scripts.py --list-scripts

    # 检查平台可用性
    python scripts/validate_scripts.py --check-platforms

需求：1.1-1.5, 6.1-6.5
"""

import argparse
import sys
import os
import logging
from pathlib import Path
from typing import List, Optional, Dict, Any
from datetime import datetime

# 确保项目根目录在Python路径中
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent
sys.path.insert(0, str(PROJECT_ROOT))

from script_validation import (
    ValidationWorkflow,
    ValidationBuilder,
    Platform,
    ValidationStatus,
    create_workflow,
    run_validation,
    discover_scripts,
    check_platform_availability,
    get_system_info,
)
from script_validation.models import ValidationConfig
from script_validation.ci_integration import (
    CIIntegration,
    ExitCode,
    is_ci_environment,
    get_ci_platform,
)


# 版本信息
__version__ = "1.0.0"

# 退出代码
EXIT_SUCCESS = ExitCode.SUCCESS
EXIT_VALIDATION_FAILED = ExitCode.VALIDATION_FAILED
EXIT_ERROR = ExitCode.ERROR


def setup_logging(verbose: bool = False, ci_mode: bool = False) -> logging.Logger:
    """配置日志系统

    Args:
        verbose: 是否启用详细输出
        ci_mode: 是否为CI模式

    Returns:
        logging.Logger: 配置好的日志记录器
    """
    level = logging.DEBUG if verbose else logging.INFO

    # CI模式下使用简化的日志格式
    if ci_mode:
        log_format = "%(levelname)s: %(message)s"
    else:
        log_format = "%(asctime)s - %(levelname)s - %(message)s"

    logging.basicConfig(
        level=level,
        format=log_format,
        handlers=[logging.StreamHandler(sys.stdout)]
    )

    return logging.getLogger(__name__)


def create_argument_parser() -> argparse.ArgumentParser:
    """创建命令行参数解析器

    Returns:
        argparse.ArgumentParser: 配置好的参数解析器
    """
    parser = argparse.ArgumentParser(
        prog="validate_scripts",
        description="Nexus项目脚本交付验证系统 - 端到端验证入口",
        epilog="""
示例:
  %(prog)s                          # 完整验证
  %(prog)s --quick                  # 快速验证
  %(prog)s --platform windows       # 仅验证Windows平台
  %(prog)s --ci                     # CI模式
  %(prog)s --list-scripts           # 列出所有脚本
  %(prog)s --check-platforms        # 检查平台可用性
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # 验证模式
    mode_group = parser.add_argument_group('验证模式')
    mode_group.add_argument(
        "--quick", "-q",
        action="store_true",
        default=False,
        help="快速验证模式（仅功能验证）"
    )
    mode_group.add_argument(
        "--full", "-f",
        action="store_true",
        default=False,
        help="完整验证模式（所有验证器）"
    )

    # 平台选择
    platform_group = parser.add_argument_group('平台选择')
    platform_group.add_argument(
        "--platform", "-p",
        dest="platform",
        type=str,
        choices=["windows", "wsl", "linux", "all"],
        default="all",
        help="目标验证平台 (默认: all)"
    )
    platform_group.add_argument(
        "--platforms",
        dest="platforms",
        nargs="+",
        type=str,
        choices=["windows", "wsl", "linux"],
        default=None,
        help="多个目标验证平台"
    )

    # 报告选项
    report_group = parser.add_argument_group('报告选项')
    report_group.add_argument(
        "--report-format", "-r",
        dest="report_format",
        nargs="+",
        type=str,
        choices=["html", "json", "summary", "junit", "all"],
        default=["all"],
        help="报告输出格式 (默认: all)"
    )
    report_group.add_argument(
        "--output-dir", "-o",
        dest="output_dir",
        type=str,
        default=None,
        help="报告输出目录 (默认: ./validation_reports)"
    )

    # 验证器选择
    validator_group = parser.add_argument_group('验证器选择')
    validator_group.add_argument(
        "--validators", "-v",
        dest="validators",
        nargs="+",
        type=str,
        choices=["functional", "compatibility", "performance", "documentation"],
        default=None,
        help="启用的验证器 (默认: 所有验证器)"
    )

    # 执行选项
    exec_group = parser.add_argument_group('执行选项')
    exec_group.add_argument(
        "--timeout",
        dest="timeout",
        type=int,
        default=300,
        help="脚本执行超时时间(秒) (默认: 300)"
    )
    exec_group.add_argument(
        "--max-memory",
        dest="max_memory",
        type=int,
        default=1024,
        help="最大内存使用限制(MB) (默认: 1024)"
    )
    exec_group.add_argument(
        "--no-parallel",
        dest="no_parallel",
        action="store_true",
        default=False,
        help="禁用并行执行"
    )

    # CI选项
    ci_group = parser.add_argument_group('CI集成')
    ci_group.add_argument(
        "--ci",
        dest="ci",
        action="store_true",
        default=False,
        help="CI模式 (简化输出，适当退出代码)"
    )

    # 信息选项
    info_group = parser.add_argument_group('信息选项')
    info_group.add_argument(
        "--list-scripts",
        dest="list_scripts",
        action="store_true",
        default=False,
        help="仅列出发现的脚本，不执行验证"
    )
    info_group.add_argument(
        "--check-platforms",
        dest="check_platforms",
        action="store_true",
        default=False,
        help="仅检查平台可用性，不执行验证"
    )
    info_group.add_argument(
        "--system-info",
        dest="system_info",
        action="store_true",
        default=False,
        help="显示系统信息"
    )

    # 通用选项
    general_group = parser.add_argument_group('通用选项')
    general_group.add_argument(
        "--verbose",
        dest="verbose",
        action="store_true",
        default=False,
        help="启用详细输出"
    )
    general_group.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}"
    )

    return parser


def get_target_platforms(args) -> List[Platform]:
    """解析目标平台参数

    Args:
        args: 解析后的命令行参数

    Returns:
        List[Platform]: 目标平台列表
    """
    platforms = []

    # 优先使用 --platforms 参数
    if args.platforms:
        for p in args.platforms:
            try:
                platforms.append(Platform(p.lower()))
            except ValueError:
                pass
    # 否则使用 --platform 参数
    elif args.platform and args.platform != "all":
        try:
            platforms.append(Platform(args.platform.lower()))
        except ValueError:
            pass

    # 如果没有指定平台，使用所有平台
    if not platforms:
        platforms = [Platform.WINDOWS, Platform.WSL, Platform.LINUX]

    return platforms


def get_report_formats(args) -> Dict[str, bool]:
    """解析报告格式参数

    Args:
        args: 解析后的命令行参数

    Returns:
        Dict[str, bool]: 报告格式启用状态
    """
    formats = {
        'html': False,
        'json': False,
        'summary': False,
        'junit': False
    }

    report_formats = args.report_format if args.report_format else ['all']

    if 'all' in report_formats:
        return {k: True for k in formats}

    for fmt in report_formats:
        if fmt in formats:
            formats[fmt] = True

    return formats


def list_scripts_command(logger: logging.Logger) -> int:
    """列出发现的脚本

    Args:
        logger: 日志记录器

    Returns:
        int: 退出代码
    """
    try:
        scripts = discover_scripts(PROJECT_ROOT)

        print(f"\n{'='*60}")
        print(f"发现 {len(scripts)} 个脚本")
        print(f"{'='*60}\n")

        # 按分类分组
        by_category = {}
        for script in scripts:
            category = script.category.value
            if category not in by_category:
                by_category[category] = []
            by_category[category].append(script)

        for category, category_scripts in sorted(by_category.items()):
            print(f"[{category.upper()}] ({len(category_scripts)} 个脚本)")
            print("-" * 40)
            for script in category_scripts:
                print(f"  {script.name}")
                print(f"    路径: {script.path}")
                print(f"    类型: {script.type.value}")
            print()

        return EXIT_SUCCESS

    except Exception as e:
        logger.error(f"列出脚本失败: {e}")
        return EXIT_ERROR


def check_platforms_command(logger: logging.Logger) -> int:
    """检查平台可用性

    Args:
        logger: 日志记录器

    Returns:
        int: 退出代码
    """
    try:
        availability = check_platform_availability()

        print(f"\n{'='*60}")
        print("平台可用性检查")
        print(f"{'='*60}\n")

        all_available = True
        for platform, available in availability.items():
            status = "✓ 可用" if available else "✗ 不可用"
            print(f"  {platform.value:10s}: {status}")
            if not available:
                all_available = False

        print()

        # 显示当前环境信息
        info = get_system_info()
        env = info.get('environment', {})

        print("当前环境信息:")
        print(f"  平台: {env.get('platform', 'unknown')}")
        print(f"  操作系统版本: {env.get('os_version', 'unknown')}")
        print(f"  Python版本: {env.get('python_version', 'unknown')}")
        print(f"  Shell版本: {env.get('shell_version', 'unknown')}")
        print()

        return EXIT_SUCCESS if all_available else EXIT_VALIDATION_FAILED

    except Exception as e:
        logger.error(f"检查平台失败: {e}")
        return EXIT_ERROR


def show_system_info_command(logger: logging.Logger) -> int:
    """显示系统信息

    Args:
        logger: 日志记录器

    Returns:
        int: 退出代码
    """
    try:
        info = get_system_info()

        print(f"\n{'='*60}")
        print("系统信息")
        print(f"{'='*60}\n")

        # 环境信息
        env = info.get('environment', {})
        print("环境信息:")
        print(f"  平台: {env.get('platform', 'unknown')}")
        print(f"  操作系统版本: {env.get('os_version', 'unknown')}")
        print(f"  Python版本: {env.get('python_version', 'unknown')}")
        print(f"  Shell版本: {env.get('shell_version', 'unknown')}")
        print()

        # 平台可用性
        print("平台可用性:")
        platform_avail = info.get('platform_availability', {})
        for platform, available in platform_avail.items():
            status = "✓" if available else "✗"
            print(f"  {platform:10s}: {status}")
        print()

        # CI信息
        ci_info = info.get('ci', {})
        print("CI环境:")
        print(f"  是否CI环境: {'是' if ci_info.get('is_ci') else '否'}")
        if ci_info.get('is_ci'):
            print(f"  CI平台: {ci_info.get('platform', 'unknown')}")
        print()

        return EXIT_SUCCESS

    except Exception as e:
        logger.error(f"获取系统信息失败: {e}")
        return EXIT_ERROR


def run_validation_command(args, logger: logging.Logger) -> int:
    """运行验证流程

    Args:
        args: 解析后的命令行参数
        logger: 日志记录器

    Returns:
        int: 退出代码
    """
    ci_integration = CIIntegration()

    try:
        # 获取目标平台
        platforms = get_target_platforms(args)

        # 获取报告格式
        report_formats = get_report_formats(args)

        # 获取验证器
        validators = args.validators if args.validators else None

        # 确定验证模式
        if args.quick:
            validators = ['functional']
            mode = 'quick'
        else:
            mode = 'full'

        # 构建工作流程
        builder = (
            ValidationBuilder()
            .root_path(PROJECT_ROOT)
            .platforms(*platforms)
            .timeout(args.timeout)
            .max_memory(args.max_memory)
            .ci_mode(args.ci or is_ci_environment())
            .verbose(args.verbose)
            .parallel(not args.no_parallel)
        )

        # 设置验证器
        if validators:
            builder.validators(*validators)

        # 设置报告格式
        enabled_formats = [fmt for fmt, enabled in report_formats.items() if enabled]
        if enabled_formats:
            builder.report_formats(*enabled_formats)

        # 构建并运行
        workflow = builder.build()

        logger.info("开始验证流程...")
        logger.info(f"目标平台: {[p.value for p in platforms]}")
        logger.info(f"验证模式: {mode}")

        # 运行验证
        report = workflow.run()

        # 确定输出目录
        output_dir = Path(args.output_dir) if args.output_dir else (PROJECT_ROOT / "validation_reports")

        # 生成报告
        generated_reports = workflow.controller.generate_reports(report, output_dir)

        # 打印摘要
        if args.ci or is_ci_environment():
            ci_integration.print_ci_summary(report)
            ci_integration.write_github_output(report)
        else:
            print_summary(report, generated_reports)

        # 返回退出代码
        return ci_integration.get_exit_code(report)

    except Exception as e:
        logger.error(f"验证失败: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        return EXIT_ERROR


def print_summary(report, generated_reports: Dict[str, str]) -> None:
    """打印验证摘要

    Args:
        report: 验证报告
        generated_reports: 生成的报告文件路径
    """
    summary = report.summary

    print(f"\n{'='*60}")
    print("验证摘要")
    print(f"{'='*60}")

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
    if report.recommendations:
        print("\n建议:")
        for rec in report.recommendations[:5]:  # 最多显示5条建议
            print(f"  • {rec}")
        if len(report.recommendations) > 5:
            print(f"  ... 还有 {len(report.recommendations) - 5} 条建议")

    print(f"\n{'='*60}")

    # 显示最终状态
    if summary.failed > 0 or summary.errors > 0:
        print("状态: ✗ 验证失败")
    else:
        print("状态: ✓ 验证通过")

    print(f"{'='*60}\n")


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
    ci_detected = is_ci_environment()
    if ci_detected and not parsed_args.ci:
        parsed_args.ci = True

    # 配置日志
    logger = setup_logging(parsed_args.verbose, parsed_args.ci)

    # 如果在CI环境中，记录检测到的平台
    if ci_detected:
        ci_platform = get_ci_platform()
        logger.info(f"检测到CI环境: {ci_platform}")

    # 处理信息命令
    if parsed_args.list_scripts:
        return list_scripts_command(logger)

    if parsed_args.check_platforms:
        return check_platforms_command(logger)

    if parsed_args.system_info:
        return show_system_info_command(logger)

    # 运行验证
    return run_validation_command(parsed_args, logger)


if __name__ == "__main__":
    sys.exit(main())
