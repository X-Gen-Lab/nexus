#!/usr/bin/env python3
"""
标记不需要翻译的 .po 文件条目
对于代码、命令、技术术语等，直接复制原文作为翻译
"""

import re
from pathlib import Path

def should_not_translate(msgid):
    """判断是否不需要翻译"""
    if not msgid or not msgid.strip():
        return False

    msgid = msgid.strip()

    # 代码块标记
    if msgid.startswith('``') and msgid.endswith('``'):
        return True

    # 纯数字
    if msgid.isdigit():
        return True

    # 全大写常量
    if re.match(r'^[A-Z_][A-Z0-9_]*$', msgid):
        return True

    # Shell 命令
    if msgid.startswith('$') or msgid.startswith('./') or msgid.startswith('python '):
        return True

    # URL
    if msgid.startswith('http://') or msgid.startswith('https://'):
        return True

    # RST 指令
    if msgid.startswith('..') or msgid.startswith(':'):
        return True

    # 文件路径
    if '/' in msgid and not ' ' in msgid:
        return True

    # 包含大量代码符号
    code_chars = sum(1 for c in msgid if c in '(){}[]<>=;,.')
    if code_chars > len(msgid) * 0.3:
        return True

    return False

def process_po_file(po_path):
    """处理单个 .po 文件"""
    with open(po_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    modified = False
    i = 0

    while i < len(lines):
        line = lines[i]

        # 查找 msgid
        if line.startswith('msgid "') and not line.startswith('msgid ""'):
            # 提取 msgid
            msgid = line[7:-2]  # 移除 'msgid "' 和 '"\n'

            # 查找对应的 msgstr
            j = i + 1
            while j < len(lines) and not lines[j].startswith('msgstr'):
                j += 1

            # 如果 msgstr 为空且不需要翻译
            if j < len(lines) and lines[j] == 'msgstr ""\n':
                if should_not_translate(msgid):
                    # 直接使用原文
                    lines[j] = f'msgstr "{msgid}"\n'
                    modified = True

        i += 1

    if modified:
        with open(po_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        return True

    return False

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    po_files = sorted(locale_dir.rglob('*.po'))

    print(f"处理 {len(po_files)} 个 .po 文件...")
    print("=" * 60)

    modified_count = 0
    for po_file in po_files:
        if process_po_file(po_file):
            modified_count += 1
            print(f"✓ {po_file.relative_to(locale_dir)}")

    print("=" * 60)
    print(f"完成！修改了 {modified_count} 个文件")
    print("\n说明：代码、命令、技术术语等已标记为不需要翻译")

if __name__ == '__main__':
    main()
