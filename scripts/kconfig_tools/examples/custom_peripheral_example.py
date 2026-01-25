#!/usr/bin/env python3
"""
\file            custom_peripheral_example.py
\brief           自定义外设模板使用示例脚本
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         此脚本演示如何创建和使用自定义外设模板，包括从 JSON
                 文件加载模板和直接在代码中定义模板。
"""

import json
import os
import sys
from pathlib import Path

/* 添加父目录到 Python 路径 */
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from kconfig_tools import (
    KconfigGenerator,
    PeripheralTemplate,
    ParameterConfig,
    ChoiceConfig,
)


def create_timer_template(platform: str) -> PeripheralTemplate:
    """
    \brief           创建定时器外设模板
    \param[in]       platform: 平台名称
    \return          定时器外设模板
    """
    return PeripheralTemplate(
        name="TIMER",
        platform=platform,
        max_instances=4,
        instance_type="numeric",
        parameters=[
            ParameterConfig(
                name="PRESCALER",
                type="int",
                default=1,
                range=(1, 65536),
                help="定时器预分频值，用于降低计数频率"
            ),
            ParameterConfig(
                name="PERIOD",
                type="int",
                default=1000,
                range=(1, 4294967295),
                help="定时器周期值（微秒），决定定时器溢出时间"
            ),
            ParameterConfig(
                name="AUTO_RELOAD",
                type="bool",
                default=True,
                help="是否启用自动重载，启用后定时器会自动重新开始计数"
            ),
        ],
        choices=[
            ChoiceConfig(
                name="CLOCK_SOURCE",
                options=["INTERNAL", "EXTERNAL", "CASCADE"],
                default="INTERNAL",
                help="时钟源选择",
                values={"INTERNAL": 0, "EXTERNAL": 1, "CASCADE": 2}
            ),
            ChoiceConfig(
                name="TRIGGER_MODE",
                options=["DISABLED", "RISING_EDGE", "FALLING_EDGE", "BOTH_EDGES"],
                default="DISABLED",
                help="触发模式选择",
                values={"DISABLED": 0, "RISING_EDGE": 1, "FALLING_EDGE": 2, "BOTH_EDGES": 3}
            ),
            ChoiceConfig(
                name="INTERRUPT",
                options=["NONE", "UPDATE", "CAPTURE", "COMPARE"],
                default="NONE",
                help="中断类型选择",
                values={"NONE": 0, "UPDATE": 1, "CAPTURE": 2, "COMPARE": 3}
            ),
        ],
        help_text="定时器外设配置，支持多种时钟源和触发模式"
    )


def load_template_from_json(json_file: str, platform: str) -> PeripheralTemplate:
    """
    \brief           从 JSON 文件加载外设模板
    \param[in]       json_file: JSON 文件路径
    \param[in]       platform: 平台名称（覆盖 JSON 中的平台）
    \return          外设模板
    """
    with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    /* 覆盖平台名称 */
    data['platform'] = platform

    /* 转换参数配置 */
    parameters = []
    for param in data.get('parameters', []):
        /* 处理 range 字段 */
        if 'range' in param and param['range']:
            param['range'] = tuple(param['range'])
        parameters.append(ParameterConfig(**param))

    /* 转换选择项配置 */
    choices = []
    for choice in data.get('choices', []):
        choices.append(ChoiceConfig(**choice))

    /* 创建模板 */
    return PeripheralTemplate(
        name=data['name'],
        platform=data['platform'],
        max_instances=data['max_instances'],
        instance_type=data['instance_type'],
        parameters=parameters,
        choices=choices,
        help_text=data.get('help_text', '')
    )


def generate_from_code_template(platform: str, output_dir: str):
    """
    \brief           使用代码定义的模板生成 Kconfig 文件
    \param[in]       platform: 平台名称
    \param[in]       output_dir: 输出目录
    """
    print(f"使用代码定义的模板生成 TIMER 配置...")

    /* 创建模板 */
    template = create_timer_template(platform)

    /* 生成文件 */
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, "timer_kconfig")

    generator = KconfigGenerator(template)
    generator.generate_file(output_file)

    print(f"✓ 成功生成: {output_file}")


def generate_from_json_template(json_file: str, platform: str, output_dir: str):
    """
    \brief           从 JSON 文件加载模板并生成 Kconfig 文件
    \param[in]       json_file: JSON 模板文件路径
    \param[in]       platform: 平台名称
    \param[in]       output_dir: 输出目录
    """
    print(f"从 JSON 文件加载模板: {json_file}")

    /* 加载模板 */
    template = load_template_from_json(json_file, platform)

    /* 生成文件 */
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, f"{template.name.lower()}_kconfig")

    generator = KconfigGenerator(template)
    generator.generate_file(output_file)

    print(f"✓ 成功生成: {output_file}")


def main():
    """
    \brief           主函数
    """
    if len(sys.argv) < 2:
        print("用法: python custom_peripheral_example.py <模式> [参数]")
        print("\n模式:")
        print("  code <平台> [输出目录]")
        print("    使用代码定义的模板生成 TIMER 配置")
        print("\n  json <JSON文件> <平台> [输出目录]")
        print("    从 JSON 文件加载模板并生成配置")
        print("\n示例:")
        print("  python custom_peripheral_example.py code NATIVE")
        print("  python custom_peripheral_example.py code NATIVE output/")
        print("  python custom_peripheral_example.py json custom_peripheral.json NATIVE")
        print("  python custom_peripheral_example.py json custom_peripheral.json STM32 output/")
        sys.exit(1)

    mode = sys.argv[1].lower()

    try:
        if mode == "code":
            /* 代码模式 */
            if len(sys.argv) < 3:
                print("错误: 缺少平台参数")
                sys.exit(1)

            platform = sys.argv[2].upper()
            output_dir = sys.argv[3] if len(sys.argv) > 3 else "output"

            generate_from_code_template(platform, output_dir)

        elif mode == "json":
            /* JSON 模式 */
            if len(sys.argv) < 4:
                print("错误: 缺少 JSON 文件或平台参数")
                sys.exit(1)

            json_file = sys.argv[2]
            platform = sys.argv[3].upper()
            output_dir = sys.argv[4] if len(sys.argv) > 4 else "output"

            if not os.path.isfile(json_file):
                print(f"错误: JSON 文件不存在: {json_file}")
                sys.exit(1)

            generate_from_json_template(json_file, platform, output_dir)

        else:
            print(f"错误: 未知模式: {mode}")
            sys.exit(1)

        print("\n生成完成！")

    except Exception as e:
        print(f"\n错误: {str(e)}")
        sys.exit(1)


if __name__ == "__main__":
    main()
