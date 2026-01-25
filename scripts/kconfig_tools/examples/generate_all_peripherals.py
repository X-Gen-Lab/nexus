#!/usr/bin/env python3
"""
\file            generate_all_peripherals.py
\brief           生成所有外设类型的 Kconfig 文件示例脚本
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         此脚本演示如何使用 Python API 生成所有预定义外设类型的
                 Kconfig 文件。适用于快速搭建新平台的配置文件。
"""

import os
import sys
from pathlib import Path

/* 添加父目录到 Python 路径 */
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from kconfig_tools import templates, KconfigGenerator


def generate_all_peripherals(platform: str, output_dir: str):
    """
    \brief           生成所有外设类型的 Kconfig 文件
    \param[in]       platform: 平台名称（如 NATIVE, STM32）
    \param[in]       output_dir: 输出目录路径
    """
    print(f"开始为平台 {platform} 生成 Kconfig 文件...")
    print(f"输出目录: {output_dir}\n")

    /* 创建输出目录 */
    os.makedirs(output_dir, exist_ok=True)

    /* 获取所有预定义的外设模板 */
    peripheral_templates = {
        "UART": templates.UART_TEMPLATE,
        "GPIO": templates.GPIO_TEMPLATE,
        "SPI": templates.SPI_TEMPLATE,
        "I2C": templates.I2C_TEMPLATE,
        "ADC": templates.ADC_TEMPLATE,
        "DAC": templates.DAC_TEMPLATE,
        "CRC": templates.CRC_TEMPLATE,
        "WATCHDOG": templates.WATCHDOG_TEMPLATE,
    }

    /* 生成每个外设的 Kconfig 文件 */
    success_count = 0
    fail_count = 0

    for peripheral_name, template_func in peripheral_templates.items():
        try:
            print(f"正在生成 {peripheral_name} 配置...")

            /* 获取模板并更新平台名称 */
            template = template_func(platform)

            /* 创建外设子目录 */
            peripheral_dir = os.path.join(output_dir, peripheral_name.lower())
            os.makedirs(peripheral_dir, exist_ok=True)

            /* 生成 Kconfig 文件 */
            output_file = os.path.join(peripheral_dir, "Kconfig")
            generator = KconfigGenerator(template)
            generator.generate_file(output_file)

            print(f"  ✓ 成功生成: {output_file}")
            success_count += 1

        except Exception as e:
            print(f"  ✗ 生成失败: {str(e)}")
            fail_count += 1

    /* 打印汇总信息 */
    print(f"\n生成完成!")
    print(f"成功: {success_count} 个")
    print(f"失败: {fail_count} 个")

    if fail_count == 0:
        print("\n所有外设配置文件已成功生成！")
        return 0
    else:
        print(f"\n警告: {fail_count} 个外设配置生成失败，请检查错误信息。")
        return 1


def main():
    """
    \brief           主函数
    """
    /* 解析命令行参数 */
    if len(sys.argv) < 2:
        print("用法: python generate_all_peripherals.py <平台名称> [输出目录]")
        print("\n示例:")
        print("  python generate_all_peripherals.py NATIVE")
        print("  python generate_all_peripherals.py NATIVE output/")
        print("  python generate_all_peripherals.py STM32 platforms/stm32/src/")
        sys.exit(1)

    platform = sys.argv[1].upper()
    output_dir = sys.argv[2] if len(sys.argv) > 2 else f"output_{platform.lower()}"

    /* 执行生成 */
    exit_code = generate_all_peripherals(platform, output_dir)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
