#!/usr/bin/env python3
"""
\file            validate_project.py
\brief           验证项目所有 Kconfig 文件的示例脚本
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         此脚本演示如何使用验证器批量检查项目中的所有 Kconfig 文件，
                 生成详细的验证报告，并提供修复建议。
"""

import os
import sys
from pathlib import Path
from typing import Dict, List

/* 添加父目录到 Python 路径 */
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from kconfig_tools import KconfigValidator, NamingRules, ValidationIssue


def find_kconfig_files(directory: str) -> List[str]:
    """
    \brief           递归查找目录下所有 Kconfig 文件
    \param[in]       directory: 搜索目录路径
    \return          Kconfig 文件路径列表
    """
    kconfig_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file == "Kconfig" or file.endswith(".kconfig"):
                kconfig_files.append(os.path.join(root, file))
    return kconfig_files


def print_validation_summary(all_issues: Dict[str, List[ValidationIssue]]):
    """
    \brief           打印验证结果汇总
    \param[in]       all_issues: 所有文件的验证问题字典
    """
    total_files = len(all_issues)
    files_with_issues = sum(1 for issues in all_issues.values() if issues)
    total_errors = sum(
        sum(1 for issue in issues if issue.severity == "error")
        for issues in all_issues.values()
    )
    total_warnings = sum(
        sum(1 for issue in issues if issue.severity == "warning")
        for issues in all_issues.values()
    )
    total_info = sum(
        sum(1 for issue in issues if issue.severity == "info")
        for issues in all_issues.values()
    )

    print("\n" + "=" * 80)
    print("验证结果汇总")
    print("=" * 80)
    print(f"总文件数: {total_files}")
    print(f"有问题的文件: {files_with_issues}")
    print(f"无问题的文件: {total_files - files_with_issues}")
    print(f"\n问题统计:")
    print(f"  错误 (ERROR):   {total_errors}")
    print(f"  警告 (WARNING): {total_warnings}")
    print(f"  信息 (INFO):    {total_info}")
    print("=" * 80)


def print_detailed_issues(all_issues: Dict[str, List[ValidationIssue]],
                         show_all: bool = False):
    """
    \brief           打印详细的验证问题
    \param[in]       all_issues: 所有文件的验证问题字典
    \param[in]       show_all: 是否显示所有文件（包括无问题的）
    """
    for file_path, issues in sorted(all_issues.items()):
        if not issues and not show_all:
            continue

        print(f"\n文件: {file_path}")
        print("-" * 80)

        if not issues:
            print("✓ 无问题")
            continue

        /* 按严重程度分组 */
        errors = [i for i in issues if i.severity == "error"]
        warnings = [i for i in issues if i.severity == "warning"]
        infos = [i for i in issues if i.severity == "info"]

        /* 打印错误 */
        if errors:
            print(f"\n错误 ({len(errors)} 个):")
            for issue in errors:
                print(f"  [行 {issue.line}] {issue.message}")
                if issue.suggestion:
                    print(f"    建议: {issue.suggestion}")

        /* 打印警告 */
        if warnings:
            print(f"\n警告 ({len(warnings)} 个):")
            for issue in warnings:
                print(f"  [行 {issue.line}] {issue.message}")
                if issue.suggestion:
                    print(f"    建议: {issue.suggestion}")

        /* 打印信息 */
        if infos:
            print(f"\n信息 ({len(infos)} 个):")
            for issue in infos:
                print(f"  [行 {issue.line}] {issue.message}")
                if issue.suggestion:
                    print(f"    建议: {issue.suggestion}")


def save_report(all_issues: Dict[str, List[ValidationIssue]],
                output_file: str):
    """
    \brief           保存验证报告到文件
    \param[in]       all_issues: 所有文件的验证问题字典
    \param[in]       output_file: 输出文件路径
    """
    validator = KconfigValidator(NamingRules())
    report = validator.generate_report(all_issues)

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(report)

    print(f"\n详细报告已保存到: {output_file}")


def validate_project(directory: str, report_file: str = None,
                    show_all: bool = False):
    """
    \brief           验证项目中的所有 Kconfig 文件
    \param[in]       directory: 项目目录路径
    \param[in]       report_file: 报告输出文件路径（可选）
    \param[in]       show_all: 是否显示所有文件（包括无问题的）
    \return          退出码（0 表示无错误，1 表示有错误）
    """
    print(f"开始验证项目: {directory}")
    print("正在搜索 Kconfig 文件...\n")

    /* 查找所有 Kconfig 文件 */
    kconfig_files = find_kconfig_files(directory)

    if not kconfig_files:
        print("未找到任何 Kconfig 文件！")
        return 1

    print(f"找到 {len(kconfig_files)} 个 Kconfig 文件")
    print("开始验证...\n")

    /* 创建验证器 */
    validator = KconfigValidator(NamingRules())

    /* 验证所有文件 */
    all_issues = {}
    for file_path in kconfig_files:
        try:
            issues = validator.validate_file(file_path)
            all_issues[file_path] = issues

            /* 显示进度 */
            status = "✓" if not issues else "✗"
            print(f"{status} {file_path}")

        except Exception as e:
            print(f"✗ {file_path} - 验证失败: {str(e)}")
            all_issues[file_path] = []

    /* 打印汇总 */
    print_validation_summary(all_issues)

    /* 打印详细问题 */
    print_detailed_issues(all_issues, show_all)

    /* 保存报告 */
    if report_file:
        save_report(all_issues, report_file)

    /* 返回退出码 */
    has_errors = any(
        any(issue.severity == "error" for issue in issues)
        for issues in all_issues.values()
    )

    if has_errors:
        print("\n验证失败: 发现错误，请修复后重新验证。")
        return 1
    else:
        print("\n验证成功: 所有文件符合命名规范！")
        return 0


def main():
    """
    \brief           主函数
    """
    /* 解析命令行参数 */
    if len(sys.argv) < 2:
        print("用法: python validate_project.py <项目目录> [选项]")
        print("\n选项:")
        print("  --report <文件>  保存详细报告到指定文件")
        print("  --show-all       显示所有文件（包括无问题的）")
        print("\n示例:")
        print("  python validate_project.py platforms/")
        print("  python validate_project.py platforms/ --report report.txt")
        print("  python validate_project.py platforms/ --show-all")
        print("  python validate_project.py platforms/ --report report.txt --show-all")
        sys.exit(1)

    directory = sys.argv[1]
    report_file = None
    show_all = False

    /* 解析选项 */
    i = 2
    while i < len(sys.argv):
        if sys.argv[i] == "--report" and i + 1 < len(sys.argv):
            report_file = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == "--show-all":
            show_all = True
            i += 1
        else:
            print(f"未知选项: {sys.argv[i]}")
            sys.exit(1)

    /* 检查目录是否存在 */
    if not os.path.isdir(directory):
        print(f"错误: 目录不存在: {directory}")
        sys.exit(1)

    /* 执行验证 */
    exit_code = validate_project(directory, report_file, show_all)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
