#!/usr/bin/env python3
"""
分析未翻译内容
识别剩余未翻译内容的类型和分布
"""

import re
from pathlib import Path
import polib
from collections import defaultdict

def categorize_entry(msgid):
    """分类条目"""
    if not msgid or not msgid.strip():
        return "empty"

    msgid = msgid.strip()

    # 代码块
    if msgid.startswith('``') or msgid.startswith('`'):
        return "code"

    # 命令行
    if msgid.startswith('$') or msgid.startswith('#') or msgid.startswith('./'):
        return "command"

    # URL
    if msgid.startswith('http'):
        return "url"

    # 文件路径
    if '/' in msgid and ' ' not in msgid:
        return "path"

    # 符号/图标
    if len(msgid) <= 3 and not msgid.isalnum():
        return "symbol"

    # 数字
    if msgid.isdigit():
        return "number"

    # 全大写（常量）
    if msgid.isupper() and len(msgid) > 1:
        return "constant"

    # 单词（无空格）
    if ' ' not in msgid and len(msgid) < 30:
        return "word"

    # 短句（<50字符）
    if len(msgid) < 50:
        return "short_sentence"

    # 中等句子（50-150字符）
    if len(msgid) < 150:
        return "medium_sentence"

    # 长句子/段落（>150字符）
    return "long_paragraph"

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')
    po_files = sorted(locale_dir.rglob('*.po'))

    # 统计
    category_counts = defaultdict(int)
    category_examples = defaultdict(list)
    module_stats = {}

    total_untranslated = 0

    for po_file in po_files:
        try:
            po = polib.pofile(po_file)
        except:
            continue

        untrans_count = 0
        for entry in po:
            if entry.msgid and not entry.msgstr and not entry.obsolete:
                total_untranslated += 1
                untrans_count += 1

                category = categorize_entry(entry.msgid)
                category_counts[category] += 1

                # 保存示例（每类最多5个）
                if len(category_examples[category]) < 5:
                    category_examples[category].append(entry.msgid[:80])

        if untrans_count > 0:
            rel_path = po_file.relative_to(locale_dir)
            module_stats[str(rel_path)] = untrans_count

    # 打印结果
    print("="*80)
    print("未翻译内容分析")
    print("="*80)
    print(f"\n总未翻译条目: {total_untranslated}\n")

    print("按类型分布:")
    print("-"*80)
    sorted_categories = sorted(category_counts.items(), key=lambda x: x[1], reverse=True)
    for category, count in sorted_categories:
        percentage = (count / total_untranslated * 100) if total_untranslated > 0 else 0
        print(f"{category:20s}: {count:5d} ({percentage:5.1f}%)")

        # 显示示例
        if category_examples[category]:
            print(f"  示例:")
            for example in category_examples[category][:3]:
                print(f"    - {example}")
        print()

    print("\n按模块分布（前20个）:")
    print("-"*80)
    sorted_modules = sorted(module_stats.items(), key=lambda x: x[1], reverse=True)
    for module, count in sorted_modules[:20]:
        print(f"{module:50s}: {count:4d}")

    # 建议
    print("\n"+"="*80)
    print("翻译建议:")
    print("="*80)

    # 计算可自动处理的
    auto_processable = (
        category_counts.get("code", 0) +
        category_counts.get("command", 0) +
        category_counts.get("url", 0) +
        category_counts.get("path", 0) +
        category_counts.get("symbol", 0) +
        category_counts.get("number", 0) +
        category_counts.get("constant", 0)
    )

    manual_needed = (
        category_counts.get("short_sentence", 0) +
        category_counts.get("medium_sentence", 0) +
        category_counts.get("long_paragraph", 0)
    )

    words_needed = category_counts.get("word", 0)

    print(f"\n1. 可标记为不翻译: {auto_processable} 条 ({auto_processable/total_untranslated*100:.1f}%)")
    print(f"   - 代码、命令、URL、路径等技术内容")
    print(f"\n2. 需要术语翻译: {words_needed} 条 ({words_needed/total_untranslated*100:.1f}%)")
    print(f"   - 单个技术术语和短语")
    print(f"\n3. 需要人工翻译: {manual_needed} 条 ({manual_needed/total_untranslated*100:.1f}%)")
    print(f"   - 完整句子和段落")

    print(f"\n预计可自动处理: {auto_processable + words_needed} 条")
    print(f"预计需要人工翻译: {manual_needed} 条")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
