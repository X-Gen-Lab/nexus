#!/usr/bin/env python3
"""
标记不需要翻译的内容
将代码、命令、URL等标记为保持原文
"""

import re
from pathlib import Path
import polib

def should_not_translate(msgid):
    """判断是否不需要翻译"""
    if not msgid or not msgid.strip():
        return False

    msgid = msgid.strip()

    # 代码块
    if (msgid.startswith('``') and msgid.endswith('``')) or \
       (msgid.startswith('`') and msgid.endswith('`')):
        return True

    # 命令行
    if msgid.startswith('$') or msgid.startswith('#') or \
       msgid.startswith('./') or msgid.startswith('python ') or \
       msgid.startswith('cmake ') or msgid.startswith('make ') or \
       msgid.startswith('git ') or msgid.startswith('cd ') or \
       msgid.startswith('mkdir ') or msgid.startswith('rm '):
        return True

    # URL
    if msgid.startswith('http://') or msgid.startswith('https://') or \
       msgid.startswith('www.') or msgid.startswith('ftp://'):
        return True

    # 文件路径（简单判断）
    if '/' in msgid and ' ' not in msgid and len(msgid.split('/')) > 2:
        return True

    # 纯数字
    if msgid.isdigit():
        return True

    # 全大写常量（超过3个字符）
    if msgid.isupper() and len(msgid) > 3 and '_' in msgid:
        return True

    # 符号
    if len(msgid) <= 3 and not msgid.isalnum():
        return True

    # 包含大量代码符号
    code_chars = sum(1 for c in msgid if c in '(){}[]<>=;')
    if code_chars > len(msgid) * 0.3:
        return True

    # RST 指令
    if msgid.startswith('..') or (msgid.startswith(':') and msgid.endswith(':')):
        return True

    # 多行命令块
    if '\n' in msgid and any(line.strip().startswith(('#', '$', 'cd ', 'mkdir ', 'cmake '))
                              for line in msgid.split('\n')):
        return True

    return False

def process_po_file(po_path):
    """处理 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        return 0

    count = 0
    for entry in po:
        if entry.msgid and not entry.msgstr and not entry.obsolete:
            if should_not_translate(entry.msgid):
                entry.msgstr = entry.msgid  # 保持原文
                count += 1

    if count > 0:
        po.save(po_path)

    return count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')
    po_files = sorted(locale_dir.rglob('*.po'))

    total = 0
    for po_file in po_files:
        count = process_po_file(po_file)
        if count > 0:
            print(f"✓ {po_file.relative_to(locale_dir)}: 标记 {count} 条")
            total += count

    print(f"\n总计标记为不翻译: {total} 条")
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
