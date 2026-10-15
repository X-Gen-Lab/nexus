#!/usr/bin/env python3
"""
生成翻译报告
分析翻译进度并生成详细报告
"""

from pathlib import Path
import polib
from collections import defaultdict

def analyze_po_file(po_path):
    """分析单个 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except:
        return None

    total = 0
    translated = 0
    untranslated = 0
    fuzzy = 0

    for entry in po:
        if not entry.obsolete and entry.msgid:
            total += 1
            if entry.msgstr:
                translated += 1
                if 'fuzzy' in entry.flags:
                    fuzzy += 1
            else:
                untranslated += 1

    return {
        'total': total,
        'translated': translated,
        'untranslated': untranslated,
        'fuzzy': fuzzy,
        'percentage': (translated / total * 100) if total > 0 else 0
    }

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')
    po_files = sorted(locale_dir.rglob('*.po'))

    # 按模块分组
    modules = defaultdict(list)
    for po_file in po_files:
        rel_path = po_file.relative_to(locale_dir)
        module = str(rel_path.parent) if rel_path.parent != Path('.') else 'root'
        modules[module].append(po_file)

    # 生成报告
    report_lines = []
    report_lines.append("# Nexus 文档翻译进度报告")
    report_lines.append("")
    report_lines.append(f"生成时间: {Path.cwd()}")
    report_lines.append("")

    # 总体统计
    total_entries = 0
    total_translated = 0
    total_untranslated = 0

    module_stats = []

    for module in sorted(modules.keys()):
        module_total = 0
        module_translated = 0

        for po_file in modules[module]:
            stats = analyze_po_file(po_file)
            if stats:
                module_total += stats['total']
                module_translated += stats['translated']
                total_entries += stats['total']
                total_translated += stats['translated']
                total_untranslated += stats['untranslated']

        if module_total > 0:
            percentage = (module_translated / module_total * 100)
            module_stats.append((module, module_total, module_translated, percentage))

    # 总体进度
    overall_percentage = (total_translated / total_entries * 100) if total_entries > 0 else 0

    report_lines.append("## 总体进度")
    report_lines.append("")
    report_lines.append(f"- **总条目**: {total_entries:,}")
    report_lines.append(f"- **已翻译**: {total_translated:,} ({overall_percentage:.1f}%)")
    report_lines.append(f"- **待翻译**: {total_untranslated:,} ({100-overall_percentage:.1f}%)")
    report_lines.append("")

    # 进度条
    bar_length = 50
    filled = int(bar_length * overall_percentage / 100)
    bar = '█' * filled + '░' * (bar_length - filled)
    report_lines.append(f"```")
    report_lines.append(f"[{bar}] {overall_percentage:.1f}%")
    report_lines.append(f"```")
    report_lines.append("")

    # 按模块统计
    report_lines.append("## 各模块翻译进度")
    report_lines.append("")
    report_lines.append("| 模块 | 总条目 | 已翻译 | 完成度 | 状态 |")
    report_lines.append("|------|--------|--------|--------|------|")

    for module, total, translated, percentage in sorted(module_stats, key=lambda x: x[3], reverse=True):
        status = "✅" if percentage >= 90 else "🟢" if percentage >= 70 else "🟡" if percentage >= 50 else "🔴"
        report_lines.append(f"| {module} | {total} | {translated} | {percentage:.1f}% | {status} |")

    report_lines.append("")

    # 详细文件列表
    report_lines.append("## 详细文件列表")
    report_lines.append("")

    for module in sorted(modules.keys()):
        report_lines.append(f"### {module}")
        report_lines.append("")
        report_lines.append("| 文件 | 总计 | 已翻译 | 待翻译 | 完成度 |")
        report_lines.append("|------|------|--------|--------|--------|")

        for po_file in sorted(modules[module]):
            stats = analyze_po_file(po_file)
            if stats:
                filename = po_file.name
                report_lines.append(
                    f"| {filename} | {stats['total']} | {stats['translated']} | "
                    f"{stats['untranslated']} | {stats['percentage']:.1f}% |"
                )

        report_lines.append("")

    # 建议
    report_lines.append("## 后续工作建议")
    report_lines.append("")

    if overall_percentage >= 90:
        report_lines.append("🎉 **翻译工作已基本完成！**")
        report_lines.append("")
        report_lines.append("建议：")
        report_lines.append("- 审核已翻译内容的质量")
        report_lines.append("- 完善剩余的少量内容")
        report_lines.append("- 进行最终的校对和润色")
    elif overall_percentage >= 70:
        report_lines.append("✅ **翻译工作进展良好！**")
        report_lines.append("")
        report_lines.append("建议：")
        report_lines.append("- 优先完成低于 70% 的模块")
        report_lines.append("- 使用专业翻译工具辅助")
        report_lines.append("- 定期审核翻译质量")
    elif overall_percentage >= 50:
        report_lines.append("🟡 **翻译工作已过半！**")
        report_lines.append("")
        report_lines.append("建议：")
        report_lines.append("- 继续使用批量翻译工具")
        report_lines.append("- 重点翻译用户指南和教程")
        report_lines.append("- 考虑使用 DeepL 等翻译服务")
    else:
        report_lines.append("🔴 **需要加强翻译工作**")
        report_lines.append("")
        report_lines.append("建议：")
        report_lines.append("- 运行所有批量翻译脚本")
        report_lines.append("- 使用自动翻译工具")
        report_lines.append("- 考虑团队协作翻译")

    report_lines.append("")
    report_lines.append("---")
    report_lines.append("")
    report_lines.append("*此报告由 generate_translation_report.py 自动生成*")

    # 保存报告
    report_path = Path('../../docs/TRANSLATION_PROGRESS_REPORT.md')
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text('\n'.join(report_lines), encoding='utf-8')

    print(f"✓ 报告已生成: {report_path}")
    print(f"\n总体进度: {overall_percentage:.1f}%")
    print(f"已翻译: {total_translated:,} / {total_entries:,}")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
