#!/usr/bin/env python3
"""
Translate Kconfig documentation .po files to Chinese
为 Kconfig 文档 .po 文件添加中文翻译
"""

import re
from pathlib import Path

# Translation dictionary for common terms
TRANSLATIONS = {
    # Titles and headers
    "Kconfig Configuration System": "Kconfig 配置系统",
    "Overview": "概述",
    "Architecture": "架构",
    "Configuration Hierarchy": "配置层次结构",
    "Quick Start": "快速开始",
    "Configuration Tools": "配置工具",
    "Platform Configuration": "平台配置",
    "Peripheral Configuration": "外设配置",
    "OSAL Configuration": "OSAL 配置",
    "HAL Configuration": "HAL 配置",
    "Using Configuration in Code": "在代码中使用配置",
    "Build System Integration": "构建系统集成",
    "Best Practices": "最佳实践",
    "Troubleshooting": "故障排除",
    "API Reference": "API 参考",
    "Development Guide": "开发指南",
    "Command-Line Interface": "命令行接口",
    "Data Structures": "数据结构",
    "Error Handling": "错误处理",
    "Examples": "示例",
    "Common Patterns": "常见模式",
    "Naming Conventions": "命名约定",
    "Validation": "验证",
    "Testing": "测试",
    "Common Pitfalls": "常见陷阱",

    # Key features
    "Key Features:": "主要特性：",
    "Hierarchical Organization": "层次化组织",
    "Type Safety": "类型安全",
    "Multiple Platforms": "多平台支持",
    "OSAL Backends": "OSAL 后端",
    "Automatic Generation": "自动生成",
    "Validation Tools": "验证工具",
    "Migration Support": "迁移支持",

    # Platform names
    "Native": "本地",
    "STM32": "STM32",
    "GD32": "GD32",
    "ESP32": "ESP32",
    "NRF52": "NRF52",

    # OSAL backends
    "Bare-metal": "裸机",
    "FreeRTOS": "FreeRTOS",
    "RT-Thread": "RT-Thread",
    "Zephyr": "Zephyr",

    # Common terms
    "Platform": "平台",
    "Chip": "芯片",
    "Peripheral": "外设",
    "Instance": "实例",
    "Configuration": "配置",
    "Boolean": "布尔",
    "integer": "整数",
    "string": "字符串",
    "hexadecimal": "十六进制",
    "validation": "验证",
    "C header files": "C 头文件",
    "generated from configuration": "从配置生成",
    "Syntax checking": "语法检查",
    "dependency validation": "依赖验证",
    "range constraints": "范围约束",
    "Configuration migration": "配置迁移",
    "between versions": "版本之间",

    # Peripherals
    "UART": "UART",
    "GPIO": "GPIO",
    "SPI": "SPI",
    "I2C": "I2C",
    "Timer": "定时器",
    "ADC": "ADC",
    "DMA": "DMA",

    # Actions
    "Select": "选择",
    "Enable": "启用",
    "Disable": "禁用",
    "Configure": "配置",
    "Generate": "生成",
    "Validate": "验证",
    "Migrate": "迁移",
    "Build": "构建",

    # File types
    "defconfig": "defconfig",
    ".config": ".config",
    "Kconfig": "Kconfig",

    # Common phrases
    "The configuration system follows a four-level hierarchy:": "配置系统遵循四级层次结构：",
    "Platform Selection": "平台选择",
    "Chip Family": "芯片系列",
    "Chip Variant": "芯片变体",
    "Platform-Specific Settings": "平台特定设置",
    "Backend Selection": "后端选择",
}

def translate_text(text):
    """Translate English text to Chinese using the translation dictionary."""
    if not text or text.strip() == "":
        return text

    # Try exact match first
    if text in TRANSLATIONS:
        return TRANSLATIONS[text]

    # Try partial matches for longer texts
    result = text
    for en, zh in TRANSLATIONS.items():
        if en in result:
            result = result.replace(en, zh)

    return result if result != text else ""  # Return empty if no translation found

def process_po_file(po_file_path):
    """Process a .po file and add Chinese translations."""
    print(f"Processing: {po_file_path}")

    with open(po_file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find all msgid/msgstr pairs
    pattern = r'(#:.*?\n)(msgid\s+"([^"]*)"\n)(msgstr\s+"([^"]*)")'

    def replace_translation(match):
        comment = match.group(1)
        msgid_line = match.group(2)
        msgid_text = match.group(3)
        msgstr_line = match.group(4)
        msgstr_text = match.group(5)

        # Skip if already translated
        if msgstr_text:
            return match.group(0)

        # Translate
        translation = translate_text(msgid_text)
        if translation:
            return f'{comment}{msgid_line}msgstr "{translation}"'
        else:
            return match.group(0)

    # Apply translations
    new_content = re.sub(pattern, replace_translation, content, flags=re.MULTILINE)

    # Count translations
    original_empty = content.count('msgstr ""')
    new_empty = new_content.count('msgstr ""')
    translated = original_empty - new_empty

    # Write back
    with open(po_file_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"  Translated: {translated} strings")
    return translated

def main():
    """Main function to translate all Kconfig-related .po files."""
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

    print(f"\nTotal translations added: {total_translated}")
    print("\nNote: This script only translates common terms and headers.")
    print("For complete translation, please manually edit the .po files:")
    for po_file in po_files:
        print(f"  - {po_file}")

if __name__ == "__main__":
    main()
