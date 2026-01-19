#!/usr/bin/env python3
r"""
\file            check_coverage.py
\brief           覆盖率阈值检查脚本
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         解析lcov覆盖率数据并检查是否达到指定阈值，
                 用于CI环境中强制执行覆盖率要求。
"""

import sys
import argparse
import re
from pathlib import Path
from typing import Dict, Optional, Tuple


class CoverageData:
    """
    \brief           覆盖率数据类
    """
    def __init__(self):
        self.lines_found = 0
        self.lines_hit = 0
        self.functions_found = 0
        self.functions_hit = 0
        self.branches_found = 0
        self.branches_hit = 0

    @property
    def line_coverage(self) -> float:
        r"""
        \brief           计算行覆盖率
        \return          行覆盖率百分比（0.0-1.0）
        """
        if self.lines_found == 0:
            return 0.0
        return self.lines_hit / self.lines_found

    @property
    def function_coverage(self) -> float:
        r"""
        \brief           计算函数覆盖率
        \return          函数覆盖率百分比（0.0-1.0）
        """
        if self.functions_found == 0:
            return 0.0
        return self.functions_hit / self.functions_found

    @property
    def branch_coverage(self) -> float:
        r"""
        \brief           计算分支覆盖率
        \return          分支覆盖率百分比（0.0-1.0）
        """
        if self.branches_found == 0:
            return 0.0
        return self.branches_hit / self.branches_found


def parse_lcov_info(coverage_file: Path) -> Optional[CoverageData]:
    r"""
    \brief           解析lcov info文件
    \param[in]       coverage_file: 覆盖率文件路径
    \return          覆盖率数据对象，解析失败返回None
    """
    if not coverage_file.exists():
        print(f"✗ 覆盖率文件不存在: {coverage_file}", file=sys.stderr)
        return None

    coverage = CoverageData()

    try:
        with open(coverage_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()

                # 解析行覆盖率
                if line.startswith('LF:'):
                    coverage.lines_found += int(line.split(':')[1])
                elif line.startswith('LH:'):
                    coverage.lines_hit += int(line.split(':')[1])

                # 解析函数覆盖率
                elif line.startswith('FNF:'):
                    coverage.functions_found += int(line.split(':')[1])
                elif line.startswith('FNH:'):
                    coverage.functions_hit += int(line.split(':')[1])

                # 解析分支覆盖率
                elif line.startswith('BRF:'):
                    coverage.branches_found += int(line.split(':')[1])
                elif line.startswith('BRH:'):
                    coverage.branches_hit += int(line.split(':')[1])

        return coverage

    except Exception as e:
        print(f"✗ 解析覆盖率文件失败: {str(e)}", file=sys.stderr)
        return None


def print_coverage_summary(coverage: CoverageData) -> None:
    r"""
    \brief           打印覆盖率摘要
    \param[in]       coverage: 覆盖率数据对象
    """
    print("\n" + "=" * 80)
    print("覆盖率摘要")
    print("=" * 80)

    print(f"\n行覆盖率:")
    print(f"  已覆盖: {coverage.lines_hit} / {coverage.lines_found}")
    print(f"  百分比: {coverage.line_coverage * 100:.2f}%")

    if coverage.functions_found > 0:
        print(f"\n函数覆盖率:")
        print(f"  已覆盖: {coverage.functions_hit} / {coverage.functions_found}")
        print(f"  百分比: {coverage.function_coverage * 100:.2f}%")

    if coverage.branches_found > 0:
        print(f"\n分支覆盖率:")
        print(f"  已覆盖: {coverage.branches_hit} / {coverage.branches_found}")
        print(f"  百分比: {coverage.branch_coverage * 100:.2f}%")

    print("\n" + "=" * 80 + "\n")


def check_threshold(coverage: CoverageData, threshold: float,
                   check_type: str = 'line') -> Tuple[bool, str]:
    r"""
    \brief           检查覆盖率是否达到阈值
    \param[in]       coverage: 覆盖率数据对象
    \param[in]       threshold: 阈值（0.0-1.0）
    \param[in]       check_type: 检查类型（'line', 'function', 'branch', 'all'）
    \return          (是否通过, 失败原因)
    """
    if check_type == 'line' or check_type == 'all':
        if coverage.line_coverage < threshold:
            return False, (f"行覆盖率 {coverage.line_coverage * 100:.2f}% "
                          f"低于阈值 {threshold * 100:.2f}%")

    if check_type == 'function' or check_type == 'all':
        if coverage.functions_found > 0 and coverage.function_coverage < threshold:
            return False, (f"函数覆盖率 {coverage.function_coverage * 100:.2f}% "
                          f"低于阈值 {threshold * 100:.2f}%")

    if check_type == 'branch' or check_type == 'all':
        if coverage.branches_found > 0 and coverage.branch_coverage < threshold:
            return False, (f"分支覆盖率 {coverage.branch_coverage * 100:.2f}% "
                          f"低于阈值 {threshold * 100:.2f}%")

    return True, ""


def main() -> int:
    r"""
    \brief           主函数
    \return          退出码（0表示通过，1表示失败）
    """
    parser = argparse.ArgumentParser(
        description='检查代码覆盖率是否达到指定阈值',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='示例:\n'
               '  python check_coverage.py --coverage-file coverage.info --threshold 0.80\n'
               '  python check_coverage.py --coverage-file coverage.info --threshold 0.90 --type all\n'
    )

    parser.add_argument(
        '--coverage-file',
        type=str,
        required=True,
        metavar='FILE',
        help='lcov覆盖率信息文件路径'
    )

    parser.add_argument(
        '--threshold',
        type=float,
        default=0.80,
        metavar='FLOAT',
        help='覆盖率阈值，范围 0.0-1.0（默认: 0.80）'
    )

    parser.add_argument(
        '--type',
        type=str,
        choices=['line', 'function', 'branch', 'all'],
        default='line',
        help='检查类型：line（行）、function（函数）、branch（分支）、all（全部）（默认: line）'
    )

    parser.add_argument(
        '--verbose',
        '-v',
        action='store_true',
        help='启用详细输出'
    )

    args = parser.parse_args()

    # 验证阈值范围
    if not 0.0 <= args.threshold <= 1.0:
        print(f"✗ 阈值必须在 0.0 到 1.0 之间，当前值: {args.threshold}",
              file=sys.stderr)
        return 1

    # 解析覆盖率文件
    coverage_file = Path(args.coverage_file)
    coverage = parse_lcov_info(coverage_file)

    if coverage is None:
        return 1

    # 打印覆盖率摘要
    if args.verbose:
        print_coverage_summary(coverage)

    # 检查阈值
    passed, reason = check_threshold(coverage, args.threshold, args.type)

    if passed:
        print(f"✓ 覆盖率检查通过（阈值: {args.threshold * 100:.2f}%）")
        if not args.verbose:
            print(f"  行覆盖率: {coverage.line_coverage * 100:.2f}%")
            if coverage.functions_found > 0:
                print(f"  函数覆盖率: {coverage.function_coverage * 100:.2f}%")
            if coverage.branches_found > 0:
                print(f"  分支覆盖率: {coverage.branch_coverage * 100:.2f}%")
        return 0
    else:
        print(f"✗ 覆盖率检查失败: {reason}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
