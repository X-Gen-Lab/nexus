#!/usr/bin/env python3
"""
运行所有批量翻译工具
一次性执行所有翻译脚本
"""

import subprocess
import sys
from pathlib import Path

def run_script(script_path, description):
    """运行单个翻译脚本"""
    print(f"\n{'='*80}")
    print(f"▶ {description}")
    print(f"  {script_path}")
    print('='*80)

    result = subprocess.run(
        [sys.executable, str(script_path)],
        capture_output=False,
        text=True
    )

    if result.returncode == 0:
        print(f"✓ 完成")
    else:
        print(f"✗ 失败")

    return result.returncode == 0

def main():
    # 批量翻译工具列表
    batch_translators = [
        ('translation_tools/batch_translators/batch_translate_part1.py', '教程和入门指南'),
        ('translation_tools/batch_translators/batch_translate_part2.py', '构建系统和开发指南'),
        ('translation_tools/batch_translators/batch_translate_part3.py', 'API 文档和配置说明'),
        ('translation_tools/batch_translators/batch_translate_part4.py', '参数和配置项'),
        ('translation_tools/batch_translators/batch_translate_sentences.py', '完整句子'),
        ('translation_tools/batch_translators/batch_translate_kconfig.py', 'Kconfig 配置文档'),
        ('translation_tools/batch_translators/batch_translate_development.py', '开发指南'),
        ('translation_tools/batch_translators/batch_translate_platforms.py', '平台指南'),
        ('translation_tools/batch_translators/batch_translate_tutorials.py', '教程'),
        ('translation_tools/batch_translators/batch_translate_comprehensive.py', '综合翻译'),
        ('translation_tools/batch_translators/batch_translate_mega.py', '超大型翻译'),
        ('translation_tools/batch_translators/batch_translate_common_patterns.py', '常见模式'),
        ('translation_tools/batch_translators/batch_translate_script_validation.py', '脚本验证系统'),
        ('translation_tools/batch_translators/batch_translate_examples.py', '示例和演示'),
        ('translation_tools/batch_translators/batch_translate_phase2_docs.py', '第2阶段：文档贡献'),
        ('translation_tools/batch_translators/batch_translate_phase2_tutorials.py', '第2阶段：教程示例'),
        ('translation_tools/batch_translators/batch_translate_phase2_platforms.py', '第2阶段：平台配置'),
        ('translation_tools/batch_translators/batch_translate_phase2_api.py', '第2阶段：API 文档'),
        ('translation_tools/batch_translators/batch_translate_phase2_terms.py', '第2阶段：技术术语'),
    ]

    print("=" * 80)
    print("Nexus 文档批量翻译工具")
    print("=" * 80)
    print(f"\n将运行 {len(batch_translators)} 个批量翻译工具\n")

    success_count = 0
    failed_scripts = []

    for script_path, description in batch_translators:
        full_path = Path(script_path)
        if full_path.exists():
            if run_script(full_path, description):
                success_count += 1
            else:
                failed_scripts.append((script_path, description))
        else:
            print(f"\n警告: {script_path} 不存在，跳过")
            failed_scripts.append((script_path, description))

    # 显示总结
    print(f"\n{'='*80}")
    print(f"批量翻译完成!")
    print(f"成功: {success_count}/{len(batch_translators)}")
    if failed_scripts:
        print(f"失败: {len(failed_scripts)}")
        for script, desc in failed_scripts:
            print(f"  - {desc} ({script})")
    print('='*80)

    # 显示最终统计
    print("\n运行翻译统计...")
    subprocess.run([sys.executable, 'translate_docs.py', '--stats'])

    return 0 if success_count == len(batch_translators) else 1

if __name__ == '__main__':
    sys.exit(main())
