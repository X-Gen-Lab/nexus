#!/usr/bin/env python3
r"""
\file            validate.py
\brief           Nexus系统验证主脚本
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         提供命令行接口用于运行系统验证，支持配置文件加载、
                 命令行参数解析和详细的帮助信息。
"""

import sys
import argparse
import os
from pathlib import Path
from typing import Optional

# 修复导入问题：支持直接运行和模块导入
if __name__ == '__main__' and __package__ is None:
    # 直接运行时，添加父目录到 Python 路径
    script_dir = Path(__file__).resolve().parent
    parent_dir = script_dir.parent
    if str(parent_dir) not in sys.path:
        sys.path.insert(0, str(parent_dir))
    # 设置包名以支持相对导入
    __package__ = 'validation'

from .config import ConfigManager, ConfigurationError
from .models import ValidationConfig
from .validation_controller import ValidationController, ValidationControllerError
from .utils import safe_print


def create_argument_parser() -> argparse.ArgumentParser:
    r"""
    \brief           创建命令行参数解析器
    \return          配置好的参数解析器
    """
    parser = argparse.ArgumentParser(
        prog='validate.py',
        description='Nexus 系统验证框架 - 自动化测试执行、覆盖率分析和报告生成',
        epilog='示例:\n'
               '  python validate.py --coverage --threshold 0.80\n'
               '  python validate.py --config validation_config.json\n'
               '  python validate.py --fail-fast --verbose\n',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # 基本选项
    parser.add_argument(
        '--build-dir',
        type=str,
        default='build',
        metavar='DIR',
        help='构建目录路径（默认: build）'
    )

    parser.add_argument(
        '--source-dir',
        type=str,
        default='.',
        metavar='DIR',
        help='源代码目录路径（默认: .）'
    )

    # 覆盖率选项
    coverage_group = parser.add_argument_group('覆盖率选项')

    coverage_group.add_argument(
        '--coverage',
        action='store_true',
        help='启用代码覆盖率分析'
    )

    coverage_group.add_argument(
        '--threshold',
        type=float,
        default=0.80,
        metavar='FLOAT',
        help='覆盖率阈值，范围 0.0-1.0（默认: 0.80）'
    )

    # 测试执行选项
    test_group = parser.add_argument_group('测试执行选项')

    test_group.add_argument(
        '--timeout',
        type=int,
        default=300,
        metavar='SECONDS',
        help='测试超时时间（秒）（默认: 300）'
    )

    test_group.add_argument(
        '--parallel',
        type=int,
        default=0,
        metavar='N',
        help='并行作业数，0表示自动检测（默认: 0）'
    )

    test_group.add_argument(
        '--fail-fast',
        action='store_true',
        help='首次测试失败时立即停止'
    )

    # 报告选项
    report_group = parser.add_argument_group('报告选项')

    report_group.add_argument(
        '--report-dir',
        type=str,
        default='build/validation_reports',
        metavar='DIR',
        help='报告输出目录（默认: build/validation_reports）'
    )

    # 配置文件选项
    config_group = parser.add_argument_group('配置文件选项')

    config_group.add_argument(
        '--config',
        type=str,
        metavar='FILE',
        help='从JSON配置文件加载配置'
    )

    config_group.add_argument(
        '--save-config',
        type=str,
        metavar='FILE',
        help='保存当前配置到JSON文件'
    )

    # 输出选项
    output_group = parser.add_argument_group('输出选项')

    output_group.add_argument(
        '--verbose',
        '-v',
        action='store_true',
        help='启用详细输出模式'
    )

    output_group.add_argument(
        '--quiet',
        '-q',
        action='store_true',
        help='启用安静模式（最小输出）'
    )

    # 其他选项
    parser.add_argument(
        '--version',
        action='version',
        version='%(prog)s 1.0.0'
    )

    return parser


def load_configuration(args: argparse.Namespace) -> ValidationConfig:
    r"""
    \brief           加载验证配置
    \param[in]       args: 命令行参数
    \return          验证配置对象
    \details         从配置文件或命令行参数加载配置，命令行参数优先级更高
    """
    config_manager = ConfigManager()

    # 如果指定了配置文件，从文件加载
    if args.config:
        try:
            config = config_manager.load_from_file(args.config)
            safe_print(f"✓ 从配置文件加载: {args.config}")
        except ConfigurationError as e:
            safe_print(f"✗ 配置文件加载失败: {str(e)}")
            sys.exit(1)
    else:
        # 使用默认配置
        config = config_manager.load_default()

    # 命令行参数覆盖配置文件
    if args.build_dir != 'build':
        config.build_dir = args.build_dir

    if args.source_dir != '.':
        config.source_dir = args.source_dir

    if args.coverage:
        config.coverage_enabled = True

    if args.threshold != 0.80:
        config.coverage_threshold = args.threshold

    if args.timeout != 300:
        config.test_timeout = args.timeout

    if args.parallel != 0:
        config.parallel_jobs = args.parallel

    if args.fail_fast:
        config.fail_fast = True

    if args.report_dir != 'build/validation_reports':
        config.report_dir = args.report_dir

    if args.verbose:
        config.verbose = True

    if args.quiet:
        config.verbose = False

    # 合并环境变量
    config = config_manager.merge_with_env(config)

    # 如果需要保存配置
    if args.save_config:
        try:
            config_manager.save_to_file(config, args.save_config)
            safe_print(f"✓ 配置已保存到: {args.save_config}")
        except ConfigurationError as e:
            safe_print(f"✗ 配置保存失败: {str(e)}")

    return config


def print_configuration(config: ValidationConfig) -> None:
    r"""
    \brief           打印配置信息
    \param[in]       config: 验证配置对象
    """
    print("\n" + "=" * 80)
    print("验证配置")
    print("=" * 80)
    print(f"构建目录:       {config.build_dir}")
    print(f"源代码目录:     {config.source_dir}")
    print(f"覆盖率分析:     {'启用' if config.coverage_enabled else '禁用'}")
    if config.coverage_enabled:
        print(f"覆盖率阈值:     {config.coverage_threshold * 100:.2f}%")
    print(f"测试超时:       {config.test_timeout} 秒")
    print(f"并行作业数:     {config.parallel_jobs if config.parallel_jobs > 0 else '自动'}")
    print(f"Fail-fast模式:  {'启用' if config.fail_fast else '禁用'}")
    print(f"报告目录:       {config.report_dir}")
    print(f"详细输出:       {'启用' if config.verbose else '禁用'}")
    print("=" * 80 + "\n")


def main() -> int:
    r"""
    \brief           主函数
    \return          退出码（0表示成功，非0表示失败）
    """
    # 解析命令行参数
    parser = create_argument_parser()
    args = parser.parse_args()

    # 加载配置
    try:
        config = load_configuration(args)
    except Exception as e:
        safe_print(f"✗ 配置加载失败: {str(e)}")
        return 1

    # 打印配置信息
    if config.verbose:
        print_configuration(config)

    # 创建验证控制器
    try:
        controller = ValidationController(config)
    except Exception as e:
        safe_print(f"✗ 初始化验证控制器失败: {str(e)}")
        return 1

    # 运行验证
    try:
        validation_result = controller.run_all_tests()

        # 返回适当的退出码
        if validation_result.success:
            safe_print("\n✓ 验证成功")
            return 0
        else:
            safe_print("\n✗ 验证失败")
            return 1

    except ValidationControllerError as e:
        safe_print(f"\n✗ 验证失败: {str(e)}")
        return 1

    except KeyboardInterrupt:
        safe_print("\n\n✗ 验证被用户中断")
        return 130  # 标准的Ctrl+C退出码

    except Exception as e:
        safe_print(f"\n✗ 验证过程中发生未预期的错误: {str(e)}")
        import traceback
        if config.verbose:
            traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())

