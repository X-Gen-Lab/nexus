#!/usr/bin/env python3
"""
批量翻译工具 - 第2阶段：文档贡献和开发指南
处理文档贡献、开发流程相关的完整句子
"""

import re
from pathlib import Path
import polib

# 文档贡献相关翻译
DOCUMENTATION_TRANSLATIONS = {
    # 感谢和介绍
    "Thank you for your interest in improving the Nexus documentation! This guide will help you contribute effectively.": "感谢您对改进 Nexus 文档的关注！本指南将帮助您有效地做出贡献。",
    "Before contributing to documentation, ensure you have:": "在为文档做出贡献之前，请确保您具备：",
    "Python 3.8 or later": "Python 3.8 或更高版本",
    "Sphinx and required extensions": "Sphinx 和所需的扩展",
    "Git for version control": "用于版本控制的 Git",

    # 环境设置
    "Setting Up the Environment": "设置环境",
    "Clone the repository:": "克隆仓库：",
    "Install documentation dependencies:": "安装文档依赖：",
    "Build the documentation locally:": "在本地构建文档：",
    "View the documentation:": "查看文档：",
    "Open ``_build/html/index.html`` in your web browser.": "在您的网络浏览器中打开 ``_build/html/index.html``。",

    # 文档结构
    "Documentation Structure": "文档结构",
    "The documentation is organized as follows:": "文档组织如下：",
    "docs/sphinx/": "docs/sphinx/",
    "├── getting_started/     # Installation and quickstart guides": "├── getting_started/     # 安装和快速入门指南",
    "├── user_guide/          # User guides for modules and features": "├── user_guide/          # 模块和功能的用户指南",
    "├── platform_guides/     # Platform-specific documentation": "├── platform_guides/     # 平台特定文档",
    "├── api/                 # API reference (auto-generated)": "├── api/                 # API 参考（自动生成）",
    "├── tutorials/           # Step-by-step tutorials": "├── tutorials/           # 分步教程",
    "├── development/         # Development guides": "├── development/         # 开发指南",
    "├── reference/           # Reference materials": "├── reference/           # 参考资料",
    "└── _templates/          # Documentation templates": "└── _templates/          # 文档模板",

    # 贡献类型
    "Types of Contributions": "贡献类型",
    "We welcome the following types of documentation contributions:": "我们欢迎以下类型的文档贡献：",
    "Fixing Errors": "修复错误",
    "Typos and grammar mistakes": "拼写和语法错误",
    "Broken links": "损坏的链接",
    "Incorrect code examples": "不正确的代码示例",
    "Outdated information": "过时的信息",

    # 改进内容
    "Improving Content": "改进内容",
    "Clarifying confusing sections": "澄清令人困惑的部分",
    "Adding missing information": "添加缺失的信息",
    "Improving code examples": "改进代码示例",
    "Adding diagrams and illustrations": "添加图表和插图",

    # 新内容
    "Adding New Content": "添加新内容",
    "New tutorials": "新教程",
    "Platform guides": "平台指南",
    "API documentation": "API 文档",
    "Best practices": "最佳实践",

    # 翻译
    "Translations": "翻译",
    "Translating documentation to other languages": "将文档翻译成其他语言",
    "Reviewing and improving existing translations": "审查和改进现有翻译",

    # 写作指南
    "Writing Guidelines": "写作指南",
    "Follow these guidelines when writing documentation:": "编写文档时请遵循以下指南：",
    "Use clear and concise language": "使用清晰简洁的语言",
    "Write in present tense": "使用现在时态",
    "Use active voice": "使用主动语态",
    "Be consistent with terminology": "保持术语一致性",
    "Include code examples where appropriate": "在适当的地方包含代码示例",
    "Add cross-references to related topics": "添加相关主题的交叉引用",

    # 代码示例
    "Code Examples": "代码示例",
    "Code examples should:": "代码示例应该：",
    "Be complete and runnable": "完整且可运行",
    "Include necessary includes and setup": "包含必要的包含和设置",
    "Follow the project coding standards": "遵循项目编码标准",
    "Include comments explaining key points": "包含解释关键点的注释",
    "Be tested to ensure they work": "经过测试以确保它们有效",

    # RST 格式
    "RST Formatting": "RST 格式",
    "Use reStructuredText (RST) for documentation": "使用 reStructuredText (RST) 编写文档",
    "Common RST elements:": "常见的 RST 元素：",
    "Headings": "标题",
    "Code blocks": "代码块",
    "Lists": "列表",
    "Links": "链接",
    "Notes and warnings": "注释和警告",

    # 提交更改
    "Submitting Changes": "提交更改",
    "Create a new branch:": "创建新分支：",
    "Make your changes": "进行更改",
    "Test your changes locally": "在本地测试您的更改",
    "Commit your changes:": "提交您的更改：",
    "Push to your fork:": "推送到您的分支：",
    "Create a pull request": "创建拉取请求",

    # 审查流程
    "Review Process": "审查流程",
    "All documentation changes go through a review process:": "所有文档更改都要经过审查流程：",
    "Automated checks run on your pull request": "自动检查在您的拉取请求上运行",
    "A maintainer reviews your changes": "维护者审查您的更改",
    "You may be asked to make revisions": "您可能会被要求进行修订",
    "Once approved, your changes are merged": "一旦获得批准，您的更改将被合并",

    # 文档测试
    "Documentation Testing": "文档测试",
    "Before submitting, ensure:": "提交之前，请确保：",
    "Documentation builds without errors": "文档构建没有错误",
    "All links work correctly": "所有链接都正常工作",
    "Code examples compile and run": "代码示例可以编译和运行",
    "Formatting is correct": "格式正确",
    "Spelling and grammar are correct": "拼写和语法正确",

    # 获取帮助
    "Getting Help": "获取帮助",
    "If you need help with documentation:": "如果您需要文档方面的帮助：",
    "Check existing documentation": "查看现有文档",
    "Ask in the project discussions": "在项目讨论中提问",
    "Open an issue for clarification": "打开问题以获得澄清",
    "Contact the documentation team": "联系文档团队",
}

def translate_po_file(po_path):
    """翻译单个 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        print(f"错误: 无法加载 {po_path}: {e}")
        return 0

    translated_count = 0

    for entry in po:
        if entry.msgid and not entry.msgstr and not entry.obsolete:
            # 精确匹配
            if entry.msgid in DOCUMENTATION_TRANSLATIONS:
                entry.msgstr = DOCUMENTATION_TRANSLATIONS[entry.msgid]
                translated_count += 1
            # 去除首尾空白后匹配
            elif entry.msgid.strip() in DOCUMENTATION_TRANSLATIONS:
                entry.msgstr = DOCUMENTATION_TRANSLATIONS[entry.msgid.strip()]
                translated_count += 1

    if translated_count > 0:
        po.save(po_path)

    return translated_count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 重点处理开发和贡献相关文件
    target_files = [
        'development/documentation_contributing.po',
        'development/contributing.po',
        'development/kconfig_guide.po',
        'development/scripts.po',
        'development/validation_framework.po',
        'development/coverage_analysis.po',
        'development/script_validation.po',
        'development/ci_cd_integration.po',
        'development/testing.po',
        'development/coding_standards.po',
    ]

    print(f"处理 {len(target_files)} 个开发指南文件...")

    total_translated = 0

    for file_path in target_files:
        full_path = locale_dir / file_path
        if full_path.exists():
            count = translate_po_file(full_path)
            if count > 0:
                print(f"  {file_path}: {count} 条")
            total_translated += count

    print(f"\n总计翻译: {total_translated} 条")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
