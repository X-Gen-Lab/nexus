#!/usr/bin/env python3
"""
Nexus 文档翻译辅助工具
自动标记不需要翻译的内容，并提供翻译建议
"""

import os
import re
from pathlib import Path
import polib

# 不需要翻译的模式
NO_TRANSLATE_PATTERNS = [
    r'^``.*``$',  # 代码块
    r'^\$.*$',  # Shell 命令
    r'^#.*$',  # 注释
    r'^[A-Z_][A-Z0-9_]*$',  # 常量名
    r'^\d+$',  # 纯数字
    r'^https?://.*$',  # URL
    r'^\.\..*$',  # RST 指令
    r'^\*\*.*\*\*$',  # 粗体标记
    r'^:.*:$',  # RST 角色
]

# 核心翻译词典
CORE_TRANSLATIONS = {
    # 文档结构
    "Overview": "概述",
    "Introduction": "简介",
    "Prerequisites": "前置条件",
    "Requirements": "要求",
    "Installation": "安装",
    "Configuration": "配置",
    "Usage": "用法",
    "Examples": "示例",
    "Tutorial": "教程",
    "Guide": "指南",
    "Reference": "参考",
    "API Reference": "API 参考",
    "User Guide": "用户指南",
    "Developer Guide": "开发者指南",
    "Getting Started": "快速开始",
    "Quick Start": "快速开始",
    "Features": "特性",
    "Architecture": "架构",
    "Design": "设计",
    "Implementation": "实现",
    "Testing": "测试",
    "Debugging": "调试",
    "Troubleshooting": "故障排除",
    "FAQ": "常见问题",
    "Glossary": "术语表",
    "Index": "索引",
    "Contents": "目录",
    "Table of Contents": "目录",

    # 通用术语
    "Note": "注意",
    "Warning": "警告",
    "Important": "重要",
    "Tip": "提示",
    "See also": "另请参阅",
    "Example": "示例",
    "Description": "描述",
    "Parameters": "参数",
    "Returns": "返回值",
    "Return value": "返回值",
    "Error": "错误",
    "Success": "成功",
    "Failed": "失败",
    "Optional": "可选",
    "Required": "必需",
    "Default": "默认",
    "Type": "类型",
    "Value": "值",
    "Name": "名称",
    "Version": "版本",
    "Author": "作者",
    "License": "许可证",
    "Copyright": "版权",

    # 操作
    "Build": "构建",
    "Compile": "编译",
    "Run": "运行",
    "Execute": "执行",
    "Install": "安装",
    "Configure": "配置",
    "Initialize": "初始化",
    "Create": "创建",
    "Delete": "删除",
    "Update": "更新",
    "Modify": "修改",
    "Add": "添加",
    "Remove": "移除",
    "Enable": "启用",
    "Disable": "禁用",
    "Start": "启动",
    "Stop": "停止",
    "Restart": "重启",
    "Reset": "重置",
    "Clear": "清除",
    "Save": "保存",
    "Load": "加载",
    "Read": "读取",
    "Write": "写入",
    "Open": "打开",
    "Close": "关闭",
    "Connect": "连接",
    "Disconnect": "断开",
    "Send": "发送",
    "Receive": "接收",
    "Transmit": "传输",
    "Transfer": "传输",

    # 状态
    "Status": "状态",
    "State": "状态",
    "Ready": "就绪",
    "Busy": "忙碌",
    "Idle": "空闲",
    "Active": "活动",
    "Inactive": "非活动",
    "Enabled": "已启用",
    "Disabled": "已禁用",
    "Running": "运行中",
    "Stopped": "已停止",
    "Pending": "待处理",
    "Complete": "完成",
    "Incomplete": "未完成",
    "Valid": "有效",
    "Invalid": "无效",
    "Available": "可用",
    "Unavailable": "不可用",
}

def should_skip_translation(text):
    """判断是否应该跳过翻译"""
    if not text or len(text.strip()) == 0:
        return True

    # 检查是否匹配不翻译模式
    for pattern in NO_TRANSLATE_PATTERNS:
        if re.match(pattern, text.strip()):
            return True

    # 如果全是ASCII且包含特殊字符，可能是代码
    if text.isascii() and any(c in text for c in ['(', ')', '{', '}', '[', ']', '<', '>', '=', ';']):
        return True

    return False

def get_translation_suggestion(text):
    """获取翻译建议"""
    text = text.strip()

    # 直接匹配
    if text in CORE_TRANSLATIONS:
        return CORE_TRANSLATIONS[text]

    # 不区分大小写匹配
    text_lower = text.lower()
    for key, value in CORE_TRANSLATIONS.items():
        if key.lower() == text_lower:
            return value

    return None

def process_po_file(po_path, auto_translate=False, mark_fuzzy=False):
    """处理 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        print(f"Error loading {po_path}: {e}")
        return 0, 0, 0

    total = 0
    translated = 0
    auto_translated = 0

    for entry in po:
        if entry.msgid and not entry.obsolete:
            total += 1

            # 已翻译
            if entry.msgstr and entry.msgstr.strip():
                translated += 1
                continue

            # 跳过不需要翻译的内容
            if should_skip_translation(entry.msgid):
                entry.msgstr = entry.msgid  # 保持原文
                if mark_fuzzy:
                    entry.flags.append('fuzzy')
                auto_translated += 1
                continue

            # 自动翻译
            if auto_translate:
                suggestion = get_translation_suggestion(entry.msgid)
                if suggestion:
                    entry.msgstr = suggestion
                    auto_translated += 1

    # 保存
    if auto_translated > 0:
        po.save(po_path)

    return total, translated, auto_translated

def main():
    import argparse

    parser = argparse.ArgumentParser(description='Nexus 文档翻译辅助工具')
    parser.add_argument('--auto', action='store_true', help='自动翻译常用术语')
    parser.add_argument('--mark-fuzzy', action='store_true', help='标记自动翻译为 fuzzy')
    parser.add_argument('--stats', action='store_true', help='只显示统计信息')
    parser.add_argument('--file', help='处理指定文件')

    args = parser.parse_args()

    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 查找所有 .po 文件
    if args.file:
        po_files = [Path(args.file)]
    else:
        po_files = sorted(locale_dir.rglob('*.po'))

    print(f"找到 {len(po_files)} 个 .po 文件")
    print("=" * 80)

    total_entries = 0
    total_translated = 0
    total_auto = 0

    for po_file in po_files:
        entries, translated, auto = process_po_file(
            po_file,
            auto_translate=args.auto,
            mark_fuzzy=args.mark_fuzzy
        )

        total_entries += entries
        total_translated += translated
        total_auto += auto

        if not args.stats or auto > 0:
            rel_path = po_file.relative_to(locale_dir)
            percent = (translated / entries * 100) if entries > 0 else 0
            print(f"{rel_path}")
            print(f"  总计: {entries}, 已翻译: {translated} ({percent:.1f}%), 自动: {auto}")

    print("=" * 80)
    print(f"总计:")
    print(f"  条目: {total_entries}")
    print(f"  已翻译: {total_translated} ({total_translated/total_entries*100:.1f}%)")
    print(f"  自动翻译: {total_auto}")
    print(f"  待翻译: {total_entries - total_translated - total_auto}")

    if not args.auto:
        print("\n提示: 使用 --auto 参数自动翻译常用术语")
        print("提示: 使用 --mark-fuzzy 标记自动翻译需要人工审核")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
