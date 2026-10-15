#!/usr/bin/env python3
"""
自动翻译 .po 文件中的未翻译内容
使用内置的翻译词典和规则
"""

import os
import re
from pathlib import Path

# 翻译词典 - 技术术语保持一致
TRANSLATION_DICT = {
    # 基础术语
    "Overview": "概述",
    "Prerequisites": "前置条件",
    "Usage": "用法",
    "Example": "示例",
    "Examples": "示例",
    "Configuration": "配置",
    "Installation": "安装",
    "Getting Started": "快速开始",
    "Tutorial": "教程",
    "Guide": "指南",
    "Reference": "参考",
    "API": "API",
    "Documentation": "文档",

    # Shell 相关
    "Shell": "Shell",
    "Command": "命令",
    "Prompt": "提示符",
    "History": "历史记录",
    "Auto-completion": "自动补全",
    "Backend": "后端",
    "UART": "UART",
    "Console": "控制台",

    # 按键
    "Left arrow": "左箭头",
    "Right arrow": "右箭头",
    "Up arrow": "上箭头",
    "Down arrow": "下箭头",
    "Move cursor left": "光标左移",
    "Move cursor right": "光标右移",
    "Move to line start": "移动到行首",
    "Move to line end": "移动到行尾",
    "Delete character before cursor": "删除光标前字符",
    "Delete character under cursor": "删除光标处字符",
    "Delete from cursor to end": "删除光标到行尾",
    "Delete from start to cursor": "删除行首到光标",
    "Previous command": "上一条命令",
    "Next command": "下一条命令",
    "Auto-complete": "自动补全",
    "Cancel current line": "取消当前行",
    "Clear screen": "清屏",

    # 状态码
    "Success": "成功",
    "Invalid parameter": "无效参数",
    "Not initialized": "未初始化",
    "Already initialized": "已初始化",
    "No backend": "无后端",
    "Command not found": "命令未找到",
    "Command exists": "命令已存在",
    "Buffer full": "缓冲区已满",

    # 配置项
    "Maximum number of commands": "最大命令数",
    "Command buffer size": "命令缓冲区大小",
    "Default prompt": "默认提示符",
    "Maximum arguments": "最大参数数量",
    "History size": "历史记录大小",

    # Porting 相关
    "Porting Guide": "移植指南",
    "This guide explains how to port Nexus to a new MCU platform.": "本指南说明如何将 Nexus 移植到新的 MCU 平台。",
    "MCU datasheet and reference manual": "MCU 数据手册和参考手册",
    "Vendor SDK or register definitions": "厂商 SDK 或寄存器定义",
    "ARM GCC toolchain": "ARM GCC 工具链",
    "for ARM Cortex-M targets": "用于 ARM Cortex-M 目标",
    "Basic understanding of the target MCU architecture": "对目标 MCU 架构的基本理解",

    # 通用短语
    "Show help": "显示帮助",
    "Show version": "显示版本",
    "Echo text": "回显文本",
    "List commands": "列出命令",
}

def translate_text(text):
    """翻译文本"""
    if not text or text.strip() == "":
        return ""

    # 如果是代码或特殊格式，不翻译
    if text.startswith("``") and text.endswith("``"):
        return text

    # 检查是否在词典中
    if text in TRANSLATION_DICT:
        return TRANSLATION_DICT[text]

    # 对于简单的技术术语，保持原样
    if re.match(r'^[A-Z_]+$', text):  # 全大写常量
        return text

    if re.match(r'^``.*``$', text):  # 代码块
        return text

    # 返回原文（需要手动翻译）
    return ""

def process_po_file(po_file):
    """处理单个 .po 文件"""
    print(f"Processing: {po_file}")

    with open(po_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    modified = False
    i = 0
    while i < len(lines):
        line = lines[i]

        # 查找 msgid
        if line.startswith('msgid "') and not line.startswith('msgid ""'):
            # 提取 msgid 内容
            msgid_content = line[7:-2]  # 移除 'msgid "' 和 '"\n'

            # 查找对应的 msgstr
            j = i + 1
            while j < len(lines) and not lines[j].startswith('msgstr'):
                j += 1

            if j < len(lines) and lines[j] == 'msgstr ""\n':
                # 尝试翻译
                translation = translate_text(msgid_content)
                if translation:
                    lines[j] = f'msgstr "{translation}"\n'
                    modified = True
                    print(f"  Translated: {msgid_content} -> {translation}")

        i += 1

    if modified:
        with open(po_file, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        print(f"  ✓ Updated {po_file}")
    else:
        print(f"  - No changes needed")

    return modified

def main():
    """主函数"""
    locale_dir = Path("locale/zh_CN/LC_MESSAGES")

    if not locale_dir.exists():
        print(f"Error: {locale_dir} not found")
        return

    # 查找所有 .po 文件
    po_files = list(locale_dir.rglob("*.po"))

    print(f"Found {len(po_files)} .po files")
    print("=" * 60)

    total_modified = 0
    for po_file in po_files:
        if process_po_file(po_file):
            total_modified += 1
        print()

    print("=" * 60)
    print(f"Summary: Modified {total_modified} / {len(po_files)} files")
    print("\nNote: Many technical terms and code blocks are intentionally")
    print("left untranslated. Manual review is recommended.")

if __name__ == "__main__":
    main()
