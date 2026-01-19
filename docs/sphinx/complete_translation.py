#!/usr/bin/env python3
"""
Complete Chinese translation for Kconfig documentation
完整的 Kconfig 文档中文翻译
"""

import re
from pathlib import Path

# Comprehensive translation dictionary
TRANSLATIONS = {
    # Long descriptions
    "The Nexus platform uses a Kconfig-based configuration system for managing build-time configuration options. This system provides a hierarchical, type-safe, and maintainable way to configure platforms, chips, peripherals, and RTOS backends.":
    "Nexus 平台使用基于 Kconfig 的配置系统来管理构建时配置选项。该系统提供了一种层次化、类型安全且易于维护的方式来配置平台、芯片、外设和 RTOS 后端。",

    "**Type Safety**: Boolean, integer, string, and hexadecimal types with validation":
    "**类型安全**：布尔、整数、字符串和十六进制类型，带有验证",

    "**Validation Tools**: Syntax checking, dependency validation, range constraints":
    "**验证工具**：语法检查、依赖验证、范围约束",

    "**Migration Support**: Configuration migration between versions":
    "**迁移支持**：版本之间的配置迁移",

    "The configuration system follows a four-level hierarchy:":
    "配置系统遵循四级层次结构：",

    # Sections
    "File Organization": "文件组织",
    "Basic Configuration": "基本配置",
    "Configuration Tools": "配置工具",
    "Platform Configuration": "平台配置",
    "Peripheral Configuration": "外设配置",
    "OSAL Configuration": "OSAL 配置",
    "HAL Configuration": "HAL 配置",
    "Using Configuration in Code": "在代码中使用配置",
    "Build System Integration": "构建系统集成",
    "Best Practices": "最佳实践",
    "Troubleshooting": "故障排除",

    # Instructions
    "Edit ``.config`` or use menuconfig:": "编辑 ``.config`` 或使用 menuconfig：",

    # Tool names
    "Generate Configuration": "生成配置",
    "Validate Configuration": "验证配置",
    "Migrate Configuration": "迁移配置",
    "Compare Configurations": "比较配置",

    # Common phrases
    "Run the configuration generator:": "运行配置生成器：",
    "This generates:": "这将生成：",
    "Validate your configuration:": "验证您的配置：",
    "The validator checks:": "验证器检查：",
    "Syntax errors": "语法错误",
    "Undefined symbols": "未定义的符号",
    "Circular dependencies": "循环依赖",
    "Range violations": "范围违规",
    "Type mismatches": "类型不匹配",

    # Platform selection
    "Select Platform:": "选择平台：",
    "Configure Peripherals:": "配置外设：",
    "Select OSAL Backend:": "选择 OSAL 后端：",

    # File paths and commands
    "Root configuration file": "根配置文件",
    "Generated configuration": "生成的配置",
    "Platform selection": "平台选择",
    "Native platform config": "本地平台配置",
    "Default configuration": "默认配置",
    "STM32 platform config": "STM32 平台配置",
    "Chip selection": "芯片选择",
    "Peripheral config": "外设配置",
    "STM32F4 defaults": "STM32F4 默认值",
    "STM32H7 defaults": "STM32H7 默认值",
    "OSAL configuration": "OSAL 配置",
    "HAL configuration": "HAL 配置",
    "Generated header": "生成的头文件",
    "Config → C header": "配置 → C 头文件",
    "Validation tool": "验证工具",
    "Migration tool": "迁移工具",
    "Comparison tool": "比较工具",

    # API Reference terms
    "Module": "模块",
    "Function": "函数",
    "Parameters": "参数",
    "Returns": "返回值",
    "Raises": "抛出",
    "Example": "示例",
    "Usage": "用法",
    "Description": "描述",
    "Arguments": "参数",
    "Options": "选项",

    # Development guide terms
    "Kconfig Syntax": "Kconfig 语法",
    "Basic Concepts": "基本概念",
    "Configuration Types": "配置类型",
    "Dependencies": "依赖",
    "Visibility": "可见性",
    "Default Values": "默认值",
    "Naming Conventions": "命名约定",
    "Common Patterns": "常见模式",
    "Validation": "验证",
    "Testing": "测试",
    "Common Pitfalls": "常见陷阱",

    # More specific translations
    "**1. Select Platform:**": "**1. 选择平台：**",
    "**2. Configure Peripherals:**": "**2. 配置外设：**",
    "**3. Select OSAL Backend:**": "**3. 选择 OSAL 后端：**",
    "**4. Generate Configuration:**": "**4. 生成配置：**",

    # Code generation
    "This generates": "这将生成",
    "with content like": "内容如下",

    # Configuration hierarchy
    "Platform Selection (Native, STM32, GD32, ESP32, NRF52)": "平台选择（本地、STM32、GD32、ESP32、NRF52）",
    "Chip Family (STM32F4, STM32H7, STM32L4, ...)": "芯片系列（STM32F4、STM32H7、STM32L4 等）",
    "Chip Variant (STM32F407, STM32F429, ...)": "芯片变体（STM32F407、STM32F429 等）",
    "Platform-Specific Settings": "平台特定设置",
    "Backend Selection (Bare-metal, FreeRTOS, RT-Thread, ...)": "后端选择（裸机、FreeRTOS、RT-Thread 等）",
    "System Parameters (Tick rate, Heap size, Stack size)": "系统参数（时钟频率、堆大小、栈大小）",
    "Linker Script Configuration": "链接脚本配置",
    "Debug Settings": "调试设置",
    "Statistics": "统计",
    "Timeout Configuration": "超时配置",
}

def translate_multiline(text):
    """Translate multi-line text."""
    # Try exact match first
    if text in TRANSLATIONS:
        return TRANSLATIONS[text]

    # Try line-by-line translation for code blocks
    lines = text.split('\n')
    translated_lines = []
    for line in lines:
        # Don't translate code, file paths, or commands
        if any(marker in line for marker in ['CONFIG_', '├──', '│', '└──', '.py', '.h', '.config', 'Kconfig']):
            translated_lines.append(line)
        else:
            # Try to translate the line
            translated = line
            for en, zh in TRANSLATIONS.items():
                if en in translated:
                    translated = translated.replace(en, zh)
            translated_lines.append(translated)

    result = '\n'.join(translated_lines)
    return result if result != text else ""

def process_po_file(po_file_path):
    """Process a .po file and add complete Chinese translations."""
    print(f"Processing: {po_file_path}")

    with open(po_file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    new_lines = []
    i = 0
    translated_count = 0

    while i < len(lines):
        line = lines[i]
        new_lines.append(line)

        # Look for msgid/msgstr pairs
        if line.startswith('msgid '):
            # Collect the full msgid (may span multiple lines)
            msgid_lines = [line]
            i += 1
            while i < len(lines) and lines[i].startswith('"'):
                msgid_lines.append(lines[i])
                i += 1

            # Now we should be at msgstr
            if i < len(lines) and lines[i].startswith('msgstr '):
                msgstr_line = lines[i]

                # Check if msgstr is empty
                if msgstr_line.strip() == 'msgstr ""':
                    # Extract msgid text
                    msgid_text = ''.join(msgid_lines)
                    msgid_text = msgid_text.replace('msgid "', '').replace('"\n', '\n').replace('"', '')
                    msgid_text = msgid_text.strip()

                    # Try to translate
                    if msgid_text:
                        translation = translate_multiline(msgid_text)
                        if translation:
                            # Format translation
                            if '\n' in translation:
                                # Multi-line translation
                                trans_lines = translation.split('\n')
                                new_msgstr = 'msgstr ""\n'
                                for trans_line in trans_lines:
                                    new_msgstr += f'"{trans_line}\\n"\n'
                                new_lines.append(new_msgstr)
                                translated_count += 1
                            else:
                                # Single-line translation
                                new_lines.append(f'msgstr "{translation}"\n')
                                translated_count += 1
                            i += 1
                            continue

                new_lines.append(msgstr_line)
                i += 1
                continue

        i += 1

    # Write back
    with open(po_file_path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

    print(f"  Added {translated_count} new translations")
    return translated_count

def main():
    """Main function to complete all translations."""
    locale_dir = Path("locale/zh_CN/LC_MESSAGES")

    po_files = [
        locale_dir / "user_guide" / "kconfig.po",
        locale_dir / "api" / "kconfig_tools.po",
        locale_dir / "development" / "kconfig_guide.po",
    ]

    total_translated = 0
    for po_file in po_files:
        if po_file.exists():
            translated = process_po_file(po_file)
            total_translated += translated
        else:
            print(f"Warning: {po_file} not found")

    print(f"\nTotal new translations added: {total_translated}")
    print("\nNext steps:")
    print("1. Review translations in .po files")
    print("2. Run: python build_docs.py --lang zh_CN")
    print("3. Verify: python build_docs.py --serve")

if __name__ == "__main__":
    main()
