#!/usr/bin/env python3
"""
Fix and complete Chinese translations for Kconfig documentation
修复并完成 Kconfig 文档的中文翻译
"""

import re
from pathlib import Path

def fix_kconfig_user_guide():
    """Fix user_guide/kconfig.po translations."""
    po_file = Path("locale/zh_CN/LC_MESSAGES/user_guide/kconfig.po")

    with open(po_file, 'r', encoding='utf-8') as f:
        content = f.read()

    # Fix specific translations
    replacements = [
        # Line 33-36: Overview description
        (r'#: ../../user_guide/kconfig\.rst:7 6bb9f1d6f2a34862b494aef5c108d938\nmsgid ""\n"The Nexus platform uses a Kconfig-based configuration system for managing"\n" build-time configuration options\. This system provides a hierarchical, "\n"type-safe, and maintainable way to configure platforms, chips, "\n"peripherals, and RTOS backends\."\nmsgstr ""',
         '#: ../../user_guide/kconfig.rst:7 6bb9f1d6f2a34862b494aef5c108d938\nmsgid ""\n"The Nexus platform uses a Kconfig-based configuration system for managing"\n" build-time configuration options. This system provides a hierarchical, "\n"type-safe, and maintainable way to configure platforms, chips, "\n"peripherals, and RTOS backends."\nmsgstr ""\n"Nexus 平台使用基于 Kconfig 的配置系统来管理构建时配置选项。"\n"该系统提供了一种层次化、类型安全且易于维护的方式来配置平台、芯片、外设和 RTOS 后端。"'),

        # Type Safety
        (r'msgid ""\n"\*\*Type Safety\*\*: Boolean, integer, string, and hexadecimal types with "\n"validation"\nmsgstr ""',
         'msgid ""\n"**Type Safety**: Boolean, integer, string, and hexadecimal types with "\n"validation"\nmsgstr "**类型安全**：布尔、整数、字符串和十六进制类型，带有验证"'),

        # Validation Tools
        (r'msgid ""\n"\*\*Validation Tools\*\*: Syntax checking, dependency validation, range "\n"constraints"\nmsgstr ""',
         'msgid ""\n"**Validation Tools**: Syntax checking, dependency validation, range "\n"constraints"\nmsgstr "**验证工具**：语法检查、依赖验证、范围约束"'),

        # Migration Support - fix existing bad translation
        (r'msgstr "\*\*迁移支持\*\*: 配置 migration 版本之间"',
         'msgstr "**迁移支持**：版本之间的配置迁移"'),

        # Automatic Generation - fix existing bad translation
        (r'msgstr "\*\*自动生成\*\*: C 头文件 从配置生成"',
         'msgstr "**自动生成**：从配置生成 C 头文件"'),

        # Basic Configuration
        (r'msgstr "Basic 配置"',
         'msgstr "基本配置"'),
    ]

    for pattern, replacement in replacements:
        content = re.sub(pattern, replacement, content, flags=re.MULTILINE)

    with open(po_file, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"✓ Fixed {po_file}")

def add_missing_translations():
    """Add missing translations by reading and updating .po files."""

    # User guide translations
    user_guide_po = Path("locale/zh_CN/LC_MESSAGES/user_guide/kconfig.po")

    with open(user_guide_po, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find and fix empty msgstr entries
    i = 0
    modified = False
    while i < len(lines):
        line = lines[i]

        # Look for msgstr ""
        if line.strip() == 'msgstr ""' and i > 0:
            # Check previous line for msgid
            prev_line = lines[i-1].strip()

            # Add translations for specific msgids
            if 'Edit ``.config`` or use menuconfig:' in prev_line:
                lines[i] = 'msgstr "编辑 ``.config`` 或使用 menuconfig："\n'
                modified = True
            elif 'Run the configuration generator:' in prev_line:
                lines[i] = 'msgstr "运行配置生成器："\n'
                modified = True
            elif 'This generates:' in prev_line:
                lines[i] = 'msgstr "这将生成："\n'
                modified = True
            elif 'Validate your configuration:' in prev_line:
                lines[i] = 'msgstr "验证您的配置："\n'
                modified = True

        i += 1

    if modified:
        with open(user_guide_po, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        print(f"✓ Added missing translations to {user_guide_po}")
    else:
        print(f"  No additional translations needed for {user_guide_po}")

def main():
    print("Fixing and completing Chinese translations...\n")

    fix_kconfig_user_guide()
    add_missing_translations()

    print("\n✓ Translation fixes complete!")
    print("\nNext steps:")
    print("  1. Run: python build_docs.py --lang zh_CN")
    print("  2. Verify: python build_docs.py --serve")

if __name__ == "__main__":
    main()
