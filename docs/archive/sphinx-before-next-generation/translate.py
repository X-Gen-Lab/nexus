#!/usr/bin/env python3
"""
Nexus 文档翻译主工具
统一的翻译工具入口，提供简洁的命令行界面
"""

import sys
import subprocess
from pathlib import Path

def print_header():
    """打印标题"""
    print("=" * 80)
    print("Nexus 文档翻译工具")
    print("=" * 80)
    print()

def print_menu():
    """打印菜单"""
    print("请选择操作：")
    print()
    print("  1. 查看翻译统计")
    print("  2. 运行所有批量翻译工具")
    print("  3. 分析未翻译内容")
    print("  4. 生成翻译报告")
    print("  5. 标记不需翻译的内容")
    print("  6. 构建中文文档")
    print("  7. 运行特定翻译工具")
    print("  0. 退出")
    print()

def run_command(script_path, description):
    """运行命令"""
    print(f"\n{'='*80}")
    print(f"▶ {description}")
    print('='*80)

    result = subprocess.run([sys.executable, str(script_path)],
                          capture_output=False, text=True)

    if result.returncode == 0:
        print(f"✓ 完成")
    else:
        print(f"✗ 失败")

    return result.returncode == 0

def show_stats():
    """显示翻译统计"""
    run_command("translate_docs.py", "查看翻译统计")

def run_all_translators():
    """运行所有翻译工具"""
    run_command("final_batch_translate.py", "运行所有批量翻译工具")

def analyze_untranslated():
    """分析未翻译内容"""
    run_command("translation_tools/utilities/analyze_untranslated.py",
                "分析未翻译内容")

def generate_report():
    """生成翻译报告"""
    run_command("translation_tools/utilities/generate_translation_report.py",
                "生成翻译报告")

def mark_no_translate():
    """标记不需翻译的内容"""
    run_command("translation_tools/utilities/mark_no_translate.py",
                "标记不需翻译的内容")

def build_docs():
    """构建中文文档"""
    run_command("build_docs.py", "构建中文文档")
    subprocess.run([sys.executable, "build_docs.py", "--lang", "zh_CN"])

def run_specific_translator():
    """运行特定翻译工具"""
    print("\n可用的批量翻译工具：")
    print()
    print("第1阶段工具：")

    translators_phase1 = [
        ("part1", "教程和入门指南"),
        ("part2", "构建系统和开发指南"),
        ("part3", "API 文档和配置说明"),
        ("part4", "参数和配置项"),
        ("sentences", "完整句子"),
        ("kconfig", "Kconfig 配置文档"),
        ("development", "开发指南"),
        ("platforms", "平台指南"),
        ("tutorials", "教程"),
        ("comprehensive", "综合翻译"),
        ("mega", "超大型翻译"),
        ("common_patterns", "常见模式"),
        ("script_validation", "脚本验证系统"),
        ("examples", "示例和演示"),
    ]

    for i, (name, desc) in enumerate(translators_phase1, 1):
        print(f"  {i:2d}. {desc}")

    print("\n第2阶段工具：")

    translators_phase2 = [
        ("phase2_docs", "文档贡献和开发指南"),
        ("phase2_tutorials", "教程和示例应用"),
        ("phase2_platforms", "平台和 Kconfig 配置"),
        ("phase2_api", "API 文档和参考"),
        ("phase2_terms", "技术术语和短语"),
    ]

    for i, (name, desc) in enumerate(translators_phase2, len(translators_phase1) + 1):
        print(f"  {i:2d}. {desc}")

    print()
    choice = input("请选择工具编号 (0 返回): ").strip()

    if choice == "0":
        return

    try:
        idx = int(choice) - 1
        all_translators = translators_phase1 + translators_phase2
        if 0 <= idx < len(all_translators):
            name, desc = all_translators[idx]
            script = f"translation_tools/batch_translators/batch_translate_{name}.py"
            run_command(script, f"运行 {desc} 翻译工具")
        else:
            print("无效的选择")
    except ValueError:
        print("无效的输入")

def main():
    """主函数"""
    print_header()

    while True:
        print_menu()
        choice = input("请输入选项 (0-7): ").strip()

        if choice == "0":
            print("\n再见！")
            break
        elif choice == "1":
            show_stats()
        elif choice == "2":
            run_all_translators()
        elif choice == "3":
            analyze_untranslated()
        elif choice == "4":
            generate_report()
        elif choice == "5":
            mark_no_translate()
        elif choice == "6":
            build_docs()
        elif choice == "7":
            run_specific_translator()
        else:
            print("\n无效的选择，请重试")

        input("\n按 Enter 继续...")
        print("\n" * 2)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n操作已取消")
        sys.exit(0)
