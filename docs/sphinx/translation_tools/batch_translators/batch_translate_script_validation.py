#!/usr/bin/env python3
"""
批量翻译工具 - 脚本验证系统
"""

import re
from pathlib import Path
import polib

SCRIPT_VALIDATION_TRANSLATIONS = {
    # 系统名称
    "Script Validation System": "脚本验证系统",
    "Validation System": "验证系统",
    "Validation Framework": "验证框架",

    # 特性描述
    "**Cross-Platform Validation**: Supports Windows, WSL, and Linux platforms": "**跨平台验证**：支持 Windows、WSL 和 Linux 平台",
    "**Multiple Validators**: Functional, compatibility, performance, and documentation validation": "**多种验证器**：功能、兼容性、性能和文档验证",
    "**Multiple Report Formats**: HTML, JSON, Summary, and JUnit XML": "**多种报告格式**：HTML、JSON、摘要和 JUnit XML",
    "**CI/CD Integration**: Supports GitHub Actions, GitLab CI, Jenkins, Azure DevOps": "**CI/CD 集成**：支持 GitHub Actions、GitLab CI、Jenkins、Azure DevOps",
    "**Flexible Configuration**: Command-line arguments and configuration files": "**灵活配置**：命令行参数和配置文件",

    # 目录结构
    "Directory Structure": "目录结构",
    "File Structure": "文件结构",
    "Project Structure": "项目结构",

    # 功能描述
    "Handle platform-specific script execution and environment setup": "处理平台特定的脚本执行和环境设置",
    "Perform different types of validation:": "执行不同类型的验证：",
    "Functional: Verify script functionality": "功能：验证脚本功能",
    "Compatibility: Check cross-platform compatibility": "兼容性：检查跨平台兼容性",
    "Performance: Measure execution performance": "性能：测量执行性能",
    "Documentation: Validate documentation completeness": "文档：验证文档完整性",
    "Generate validation reports in various formats": "生成各种格式的验证报告",
    "Orchestrate the validation workflow": "编排验证工作流",

    # 使用方式
    "Command-Line Usage": "命令行用法",
    "Programming Interface": "编程接口",
    "Command-Line Arguments": "命令行参数",
    "Configuration Options": "配置选项",

    # 参数说明
    "Project root directory path": "项目根目录路径",
    "Current directory": "当前目录",
    "Validation mode: full/quick/platform-specific": "验证模式：完整/快速/平台特定",
    "Target platforms: windows/wsl/linux": "目标平台：windows/wsl/linux",
    "Report format: html/json/summary/junit/all": "报告格式：html/json/summary/junit/all",
    "Report output directory": "报告输出目录",
    "Validators: functional/compatibility/performance/documentation": "验证器：功能/兼容性/性能/文档",
    "Disable parallel execution": "禁用并行执行",

    # 跨平台测试
    "Cross-Platform Testing Guide": "跨平台测试指南",
    "The script validation system supports three platforms:": "脚本验证系统支持三个平台：",
    "**Windows**: Native Windows environment": "**Windows**：原生 Windows 环境",
    "**WSL**: Windows Subsystem for Linux": "**WSL**：Windows Linux 子系统",
    "**Linux**: Native Linux environment": "**Linux**：原生 Linux 环境",

    # 平台测试
    "Testing on Windows": "在 Windows 上测试",
    "Testing on WSL": "在 WSL 上测试",
    "Testing on Linux": "在 Linux 上测试",
    "Testing on macOS": "在 macOS 上测试",

    # 跨平台注意事项
    "Path separators: Use ``Path`` objects for cross-platform compatibility": "路径分隔符：使用 ``Path`` 对象实现跨平台兼容性",
    "Line endings: Ensure scripts handle both CRLF and LF": "行结束符：确保脚本处理 CRLF 和 LF",
    "Shell differences: Test both CMD and PowerShell execution": "Shell 差异：测试 CMD 和 PowerShell 执行",
    "File permissions: WSL may have different permissions than Windows": "文件权限：WSL 可能与 Windows 有不同的权限",
    "Path translation: Windows paths need conversion in WSL": "路径转换：Windows 路径在 WSL 中需要转换",

    # 要求
    "Python 3.8+ in WSL": "WSL 中的 Python 3.8+",
    "Access to Windows filesystem": "访问 Windows 文件系统",
    "Python 3.8+ installed": "已安装 Python 3.8+",
    "Required packages installed": "已安装必需的包",

    # 验证类型
    "Functional validation": "功能验证",
    "Compatibility validation": "兼容性验证",
    "Performance validation": "性能验证",
    "Documentation validation": "文档验证",
    "Security validation": "安全验证",

    # 报告格式
    "HTML report": "HTML 报告",
    "JSON report": "JSON 报告",
    "Summary report": "摘要报告",
    "JUnit XML report": "JUnit XML 报告",
    "Detailed report": "详细报告",

    # CI/CD 平台
    "GitHub Actions": "GitHub Actions",
    "GitLab CI": "GitLab CI",
    "Jenkins": "Jenkins",
    "Azure DevOps": "Azure DevOps",
    "Travis CI": "Travis CI",
    "CircleCI": "CircleCI",

    # 执行模式
    "Full validation": "完整验证",
    "Quick validation": "快速验证",
    "Platform-specific validation": "平台特定验证",
    "Parallel execution": "并行执行",
    "Sequential execution": "顺序执行",

    # 结果
    "Validation passed": "验证通过",
    "Validation failed": "验证失败",
    "Validation skipped": "验证跳过",
    "Validation in progress": "验证进行中",

    # 错误和警告
    "Validation error": "验证错误",
    "Validation warning": "验证警告",
    "Configuration error": "配置错误",
    "Execution error": "执行错误",

    # 统计信息
    "Total tests": "总测试数",
    "Passed tests": "通过的测试",
    "Failed tests": "失败的测试",
    "Skipped tests": "跳过的测试",
    "Test coverage": "测试覆盖率",
    "Execution time": "执行时间",

    # 配置
    "Configuration file": "配置文件",
    "Default configuration": "默认配置",
    "Custom configuration": "自定义配置",
    "Load configuration": "加载配置",
    "Save configuration": "保存配置",

    # 输出
    "Output directory": "输出目录",
    "Report directory": "报告目录",
    "Log directory": "日志目录",
    "Temporary directory": "临时目录",

    # 过滤和选择
    "Filter by platform": "按平台过滤",
    "Filter by type": "按类型过滤",
    "Select validators": "选择验证器",
    "Select platforms": "选择平台",

    # 详细程度
    "Verbose output": "详细输出",
    "Quiet mode": "安静模式",
    "Debug mode": "调试模式",
    "Normal mode": "正常模式",

    # 帮助和文档
    "Show help": "显示帮助",
    "Show version": "显示版本",
    "Show examples": "显示示例",
    "Show configuration": "显示配置",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in SCRIPT_VALIDATION_TRANSLATIONS:
        return SCRIPT_VALIDATION_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in SCRIPT_VALIDATION_TRANSLATIONS.items():
        if key.lower() == msgid.lower():
            return value

    return ""

def process_po_file(po_path):
    """处理 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        return 0

    count = 0
    for entry in po:
        if entry.msgid and not entry.msgstr and not entry.obsolete:
            translation = translate_entry(entry.msgid)
            if translation:
                entry.msgstr = translation
                count += 1

    if count > 0:
        po.save(po_path)

    return count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')
    po_files = sorted(locale_dir.rglob('*.po'))

    print(f"处理 {len(po_files)} 个文件...")

    total = 0
    for po_file in po_files:
        count = process_po_file(po_file)
        if count > 0:
            print(f"✓ {po_file.relative_to(locale_dir)}: {count} 条")
            total += count

    print(f"\n总计翻译: {total} 条")
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
