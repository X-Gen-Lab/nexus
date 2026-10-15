#!/usr/bin/env python3
"""
Nexus Documentation Translation Helper
帮助自动化翻译工作流程的工具

Features:
- Auto-translate common technical terms
- Preserve code blocks and RST markup
- Generate translation statistics
- Validate .po files
"""

import argparse
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

# Technical term dictionary (English -> Chinese)
TERM_DICT = {
    # Core concepts
    'Hardware Abstraction Layer': '硬件抽象层',
    'HAL': 'HAL',
    'OS Abstraction Layer': '操作系统抽象层',
    'OSAL': 'OSAL',
    'Configuration': '配置',
    'Platform': '平台',
    'Peripheral': '外设',
    'Framework': '框架',

    # Components
    'GPIO': 'GPIO',
    'UART': 'UART',
    'SPI': 'SPI',
    'I2C': 'I2C',
    'Timer': '定时器',
    'ADC': 'ADC',
    'PWM': 'PWM',
    'DMA': 'DMA',

    # Programming concepts
    'Factory Pattern': '工厂模式',
    'Interface': '接口',
    'Callback': '回调',
    'Interrupt': '中断',
    'Task': '任务',
    'Thread': '线程',
    'Mutex': '互斥锁',
    'Semaphore': '信号量',
    'Queue': '队列',

    # Documentation sections
    'Overview': '概述',
    'Getting Started': '快速入门',
    'User Guide': '用户指南',
    'Tutorials': '教程',
    'API Reference': 'API 参考',
    'Development': '开发指南',
    'Prerequisites': '前置条件',
    'Installation': '安装',
    'Quick Start': '快速开始',
    'Examples': '示例',
    'FAQ': '常见问题',

    # Common phrases
    'See also': '另请参阅',
    'Note': '注意',
    'Warning': '警告',
    'Tip': '提示',
    'Important': '重要',
    'Example': '示例',
    'Usage': '用法',
    'Parameters': '参数',
    'Returns': '返回值',
    'Description': '描述',

    # Build and tools
    'Build System': '构建系统',
    'Toolchain': '工具链',
    'Compiler': '编译器',
    'Debugger': '调试器',
    'Flash': '烧录',
    'Debug': '调试',

    # Platforms
    'Native': 'Native',
    'STM32F4': 'STM32F4',
    'STM32H7': 'STM32H7',
    'GD32': 'GD32',
    'ESP32': 'ESP32',
}


def parse_po_file(po_path: Path) -> List[Dict]:
    """Parse a .po file and return list of message entries."""
    entries = []
    current_entry = {}

    with open(po_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')

            if line.startswith('#:'):
                if current_entry:
                    entries.append(current_entry)
                current_entry = {'location': line[2:].strip()}
            elif line.startswith('msgid'):
                match = re.match(r'msgid "(.*)"', line)
                if match:
                    current_entry['msgid'] = match.group(1)
            elif line.startswith('msgstr'):
                match = re.match(r'msgstr "(.*)"', line)
                if match:
                    current_entry['msgstr'] = match.group(1)
            elif line.startswith('"') and 'msgid' in current_entry:
                # Continuation line
                match = re.match(r'"(.*)"', line)
                if match:
                    if 'msgstr' not in current_entry:
                        current_entry['msgid'] += match.group(1)
                    else:
                        current_entry['msgstr'] += match.group(1)

        if current_entry:
            entries.append(current_entry)

    return entries


def get_translation_stats(locale_dir: Path, lang: str) -> Dict:
    """Get translation statistics for a language."""
    stats = {
        'total': 0,
        'translated': 0,
        'untranslated': 0,
        'fuzzy': 0,
        'files': []
    }

    po_dir = locale_dir / lang / 'LC_MESSAGES'
    if not po_dir.exists():
        return stats

    for po_file in po_dir.rglob('*.po'):
        entries = parse_po_file(po_file)
        file_stats = {
            'path': str(po_file.relative_to(locale_dir)),
            'total': len(entries),
            'translated': sum(1 for e in entries if e.get('msgstr', '')),
            'untranslated': sum(1 for e in entries if not e.get('msgstr', '')),
        }

        stats['total'] += file_stats['total']
        stats['translated'] += file_stats['translated']
        stats['untranslated'] += file_stats['untranslated']
        stats['files'].append(file_stats)

    return stats


def auto_translate_terms(po_path: Path, dry_run: bool = False) -> int:
    """Auto-translate common technical terms in a .po file."""
    if not po_path.exists():
        print(f"Error: {po_path} not found")
        return 0

    with open(po_path, 'r', encoding='utf-8') as f:
        content = f.read()

    translated_count = 0
    lines = content.split('\n')
    new_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]

        # Check if this is an untranslated msgid
        if line.startswith('msgid "') and i + 1 < len(lines):
            msgid_match = re.match(r'msgid "(.*)"', line)
            if msgid_match:
                msgid = msgid_match.group(1)

                # Check next line for msgstr
                if lines[i + 1].startswith('msgstr ""'):
                    # Try to translate
                    translation = TERM_DICT.get(msgid)
                    if translation:
                        new_lines.append(line)
                        new_lines.append(f'msgstr "{translation}"')
                        translated_count += 1
                        i += 2
                        continue

        new_lines.append(line)
        i += 1

    if not dry_run and translated_count > 0:
        with open(po_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(new_lines))
        print(f"✓ Auto-translated {translated_count} terms in {po_path.name}")
    elif dry_run:
        print(f"Would auto-translate {translated_count} terms in {po_path.name}")

    return translated_count


def validate_po_file(po_path: Path) -> List[str]:
    """Validate a .po file for common issues."""
    issues = []

    with open(po_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check for broken RST markup
    rst_patterns = [
        (r':doc:', 'RST :doc: reference'),
        (r':ref:', 'RST :ref: reference'),
        (r'\.\. code-block::', 'RST code-block directive'),
        (r'\*\*', 'RST bold markup'),
        (r'``', 'RST inline code'),
    ]

    entries = parse_po_file(po_path)
    for entry in entries:
        msgid = entry.get('msgid', '')
        msgstr = entry.get('msgstr', '')

        if not msgstr:
            continue

        # Check RST markup preservation
        for pattern, name in rst_patterns:
            msgid_count = len(re.findall(pattern, msgid))
            msgstr_count = len(re.findall(pattern, msgstr))

            if msgid_count != msgstr_count:
                issues.append(
                    f"Line {entry.get('location', '?')}: "
                    f"{name} count mismatch (msgid: {msgid_count}, msgstr: {msgstr_count})"
                )

    return issues


def print_stats(stats: Dict, lang: str):
    """Print translation statistics."""
    print(f"\n{'='*60}")
    print(f"Translation Statistics for {lang}")
    print(f"{'='*60}")
    print(f"Total strings:       {stats['total']}")
    print(f"Translated:          {stats['translated']} ({stats['translated']/stats['total']*100:.1f}%)")
    print(f"Untranslated:        {stats['untranslated']} ({stats['untranslated']/stats['total']*100:.1f}%)")
    print(f"\nPer-file breakdown:")
    print(f"{'-'*60}")

    for file_stat in sorted(stats['files'], key=lambda x: x['translated']/x['total'] if x['total'] > 0 else 0):
        if file_stat['total'] == 0:
            continue
        pct = file_stat['translated'] / file_stat['total'] * 100
        status = '✓' if pct == 100 else '○' if pct > 50 else '✗'
        print(f"{status} {file_stat['path']:50} {file_stat['translated']:3}/{file_stat['total']:3} ({pct:5.1f}%)")


def main():
    parser = argparse.ArgumentParser(
        description='Nexus Documentation Translation Helper',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s --stats zh_CN              Show translation statistics
  %(prog)s --auto-translate zh_CN     Auto-translate common terms
  %(prog)s --validate zh_CN           Validate .po files
  %(prog)s --all zh_CN                Do everything
        '''
    )

    parser.add_argument('lang', help='Language code (e.g., zh_CN)')
    parser.add_argument('--stats', action='store_true',
                        help='Show translation statistics')
    parser.add_argument('--auto-translate', action='store_true',
                        help='Auto-translate common technical terms')
    parser.add_argument('--validate', action='store_true',
                        help='Validate .po files for issues')
    parser.add_argument('--all', action='store_true',
                        help='Run all operations')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show what would be done without making changes')

    args = parser.parse_args()

    # Determine script location
    script_dir = Path(__file__).parent
    locale_dir = script_dir / 'locale'

    if not locale_dir.exists():
        print(f"Error: locale directory not found: {locale_dir}")
        return 1

    lang_dir = locale_dir / args.lang / 'LC_MESSAGES'
    if not lang_dir.exists():
        print(f"Error: Language directory not found: {lang_dir}")
        print(f"Run: python build_docs.py --init-po {args.lang}")
        return 1

    # Run requested operations
    if args.all or args.stats:
        stats = get_translation_stats(locale_dir, args.lang)
        print_stats(stats, args.lang)

    if args.all or args.auto_translate:
        print(f"\n{'='*60}")
        print(f"Auto-translating common terms in {args.lang}")
        print(f"{'='*60}")
        total_translated = 0
        for po_file in lang_dir.rglob('*.po'):
            count = auto_translate_terms(po_file, args.dry_run)
            total_translated += count
        print(f"\nTotal: {total_translated} terms auto-translated")

    if args.all or args.validate:
        print(f"\n{'='*60}")
        print(f"Validating .po files for {args.lang}")
        print(f"{'='*60}")
        total_issues = 0
        for po_file in lang_dir.rglob('*.po'):
            issues = validate_po_file(po_file)
            if issues:
                print(f"\n{po_file.name}:")
                for issue in issues:
                    print(f"  ⚠ {issue}")
                total_issues += len(issues)

        if total_issues == 0:
            print("✓ No issues found")
        else:
            print(f"\n⚠ Total: {total_issues} issues found")

    return 0


if __name__ == '__main__':
    sys.exit(main())
