#!/usr/bin/env python3
"""
\file            quick_start.py
\brief           快速开始示例脚本
\author          Nexus Team
\version         1.0.0
\date            2026-01-20

\copyright       Copyright (c) 2026 Nexus Team

\details         此脚本提供交互式菜单，帮助用户快速上手 Kconfig 工具系统。
"""

import os
import sys
from pathlib import Path

/* 添加父目录到 Python 路径 */
sys.path.insert(0, str(Path(__file__).parent.parent.parent))


def print_menu():
    """
    \brief           打印主菜单
    """
    print("\n" + "=" * 60)
    print("Kconfig 命名规范系统 - 快速开始")
    print("=" * 60)
    print("\n请选择操作:")
    print("  1. 生成单个外设 Kconfig 文件")
    print("  2. 批量生成所有外设 Kconfig 文件")
    print("  3. 验证单个 Kconfig 文件")
    print("  4. 验证项目所有 Kconfig 文件")
    print("  5. 查看命名规范说明")
    print("  6. 查看示例配置文件")
    print("  0. 退出")
    print("=" * 60)


def generate_single_peripheral():
    """
    \brief           生成单个外设配置
    """
    print("\n生成单个外设 Kconfig 文件")
    print("-" * 60)

    /* 外设类型选择 */
    print("\n支持的外设类型:")
    peripherals = ["UART", "GPIO", "SPI", "I2C", "ADC", "DAC", "CRC", "WATCHDOG"]
    for i, p in enumerate(peripherals, 1):
        print(f"  {i}. {p}")

    choice = input("\n请选择外设类型 (1-8): ").strip()
    try:
        peripheral = peripherals[int(choice) - 1]
    except (ValueError, IndexError):
        print("无效的选择！")
        return

    /* 平台选择 */
    platform = input("请输入平台名称 (如 NATIVE, STM32): ").strip().upper()
    if not platform:
        print("平台名称不能为空！")
        return

    /* 实例数量 */
    instances = input(f"请输入实例数量 (默认 2): ").strip()
    instances = int(instances) if instances else 2

    /* 输出路径 */
    output = input(f"请输入输出文件路径 (默认 output/{peripheral.lower()}_kconfig): ").strip()
    if not output:
        output = f"output/{peripheral.lower()}_kconfig"

    /* 生成命令 */
    cmd = f"python scripts/kconfig_tools/cli.py generate -p {peripheral} -P {platform} -n {instances} -o {output}"
    print(f"\n执行命令: {cmd}")
    os.system(cmd)


def batch_generate_all():
    """
    \brief           批量生成所有外设
    """
    print("\n批量生成所有外设 Kconfig 文件")
    print("-" * 60)

    platform = input("请输入平台名称 (如 NATIVE, STM32): ").strip().upper()
    if not platform:
        print("平台名称不能为空！")
        return

    output_dir = input(f"请输入输出目录 (默认 output_{platform.lower()}): ").strip()
    if not output_dir:
        output_dir = f"output_{platform.lower()}"

    /* 使用示例脚本 */
    cmd = f"python scripts/kconfig_tools/examples/generate_all_peripherals.py {platform} {output_dir}"
    print(f"\n执行命令: {cmd}")
    os.system(cmd)


def validate_single_file():
    """
    \brief           验证单个文件
    """
    print("\n验证单个 Kconfig 文件")
    print("-" * 60)

    file_path = input("请输入 Kconfig 文件路径: ").strip()
    if not file_path:
        print("文件路径不能为空！")
        return

    if not os.path.isfile(file_path):
        print(f"文件不存在: {file_path}")
        return

    cmd = f"python scripts/kconfig_tools/cli.py validate -f {file_path}"
    print(f"\n执行命令: {cmd}")
    os.system(cmd)


def validate_project():
    """
    \brief           验证项目所有文件
    """
    print("\n验证项目所有 Kconfig 文件")
    print("-" * 60)

    directory = input("请输入项目目录路径 (如 platforms/): ").strip()
    if not directory:
        print("目录路径不能为空！")
        return

    if not os.path.isdir(directory):
        print(f"目录不存在: {directory}")
        return

    save_report = input("是否保存详细报告? (y/n, 默认 n): ").strip().lower()
    report_file = ""
    if save_report == 'y':
        report_file = input("请输入报告文件路径 (默认 validation_report.txt): ").strip()
        if not report_file:
            report_file = "validation_report.txt"

    /* 使用示例脚本 */
    if report_file:
        cmd = f"python scripts/kconfig_tools/examples/validate_project.py {directory} --report {report_file}"
    else:
        cmd = f"python scripts/kconfig_tools/examples/validate_project.py {directory}"

    print(f"\n执行命令: {cmd}")
    os.system(cmd)


def show_naming_rules():
    """
    \brief           显示命名规范说明
    """
    print("\n" + "=" * 60)
    print("Kconfig 命名规范说明")
    print("=" * 60)

    print("\n1. 平台级配置符号:")
    print("   - 平台使能: {PLATFORM}_ENABLE")
    print("     示例: NATIVE_ENABLE")
    print("   - 平台特性: {PLATFORM}_{FEATURE}_ENABLE")
    print("     示例: NATIVE_UART_ENABLE")

    print("\n2. 外设级配置符号:")
    print("   - 外设使能: {PLATFORM}_{PERIPHERAL}_ENABLE")
    print("     示例: NATIVE_UART_ENABLE")
    print("   - 最大实例数: {PLATFORM}_{PERIPHERAL}_MAX_INSTANCES")
    print("     示例: NATIVE_UART_MAX_INSTANCES")

    print("\n3. 实例级配置符号:")
    print("   - 实例使能: INSTANCE_NX_{PERIPHERAL}_{N}")
    print("     示例: INSTANCE_NX_UART_0")
    print("   - 实例参数: {PERIPHERAL}{N}_{PARAMETER}")
    print("     示例: UART0_BAUDRATE")

    print("\n4. 选择项命名:")
    print("   - 选择项选项: NX_{PERIPHERAL}{N}_{CATEGORY}_{OPTION}")
    print("     示例: NX_UART0_PARITY_NONE")
    print("   - 选择项值: {PERIPHERAL}{N}_{CATEGORY}_VALUE")
    print("     示例: UART0_PARITY_VALUE")

    print("\n5. 特殊规则:")
    print("   - GPIO 端口使用字母标识: GPIOA, GPIOB, GPIOC")
    print("   - GPIO 引脚使用 PIN 前缀: GPIOA_PIN0_MODE")

    input("\n按回车键返回主菜单...")


def show_example_configs():
    """
    \brief           显示示例配置文件
    """
    print("\n" + "=" * 60)
    print("示例配置文件")
    print("=" * 60)

    examples_dir = Path(__file__).parent
    config_files = [
        "batch_config.yaml",
        "stm32_config.yaml",
        "minimal_config.yaml",
        "custom_peripheral.json"
    ]

    print("\n可用的示例配置文件:")
    for i, config_file in enumerate(config_files, 1):
        file_path = examples_dir / config_file
        if file_path.exists():
            print(f"  {i}. {config_file}")
        else:
            print(f"  {i}. {config_file} (未找到)")

    choice = input("\n请选择要查看的配置文件 (1-4, 0 返回): ").strip()
    if choice == '0':
        return

    try:
        config_file = config_files[int(choice) - 1]
        file_path = examples_dir / config_file

        if not file_path.exists():
            print(f"文件不存在: {file_path}")
            input("\n按回车键返回主菜单...")
            return

        print(f"\n文件内容: {config_file}")
        print("-" * 60)
        with open(file_path, 'r', encoding='utf-8') as f:
            print(f.read())

    except (ValueError, IndexError):
        print("无效的选择！")

    input("\n按回车键返回主菜单...")


def main():
    """
    \brief           主函数
    """
    while True:
        print_menu()
        choice = input("\n请选择 (0-6): ").strip()

        if choice == '0':
            print("\n感谢使用！再见！")
            break
        elif choice == '1':
            generate_single_peripheral()
        elif choice == '2':
            batch_generate_all()
        elif choice == '3':
            validate_single_file()
        elif choice == '4':
            validate_project()
        elif choice == '5':
            show_naming_rules()
        elif choice == '6':
            show_example_configs()
        else:
            print("\n无效的选择，请重试！")

        input("\n按回车键继续...")


if __name__ == "__main__":
    main()
