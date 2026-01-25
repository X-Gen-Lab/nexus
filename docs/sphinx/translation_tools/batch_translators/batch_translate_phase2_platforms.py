#!/usr/bin/env python3
"""
批量翻译工具 - 第2阶段：平台和 Kconfig
处理平台指南和 Kconfig 配置相关的完整句子
"""

import re
from pathlib import Path
import polib

# 平台和配置相关翻译
PLATFORM_TRANSLATIONS = {
    # 平台概述
    "This guide provides detailed configuration information for each supported platform in the Nexus Embedded Platform.": "本指南提供 Nexus 嵌入式平台中每个支持平台的详细配置信息。",
    "The Nexus platform supports multiple hardware platforms, each with unique characteristics and configuration requirements:": "Nexus 平台支持多个硬件平台，每个平台都有独特的特性和配置要求：",
    "**Native**: PC simulation platform for development and testing": "**Native**：用于开发和测试的 PC 仿真平台",
    "**ESP32**: Espressif ESP32 WiFi/Bluetooth SoC": "**ESP32**：Espressif ESP32 WiFi/蓝牙 SoC",
    "The Native platform provides PC simulation for development and testing without hardware. It runs on Windows, Linux, and macOS.": "Native 平台提供无需硬件的 PC 仿真，用于开发和测试。它可在 Windows、Linux 和 macOS 上运行。",

    # STM32F4 规格
    "STM32F407: 168 MHz, 1 MB Flash, 192 KB RAM": "STM32F407：168 MHz，1 MB Flash，192 KB RAM",
    "STM32F429: 180 MHz, 2 MB Flash, 256 KB RAM": "STM32F429：180 MHz，2 MB Flash，256 KB RAM",
    "STM32F446: 180 MHz, 512 KB Flash, 128 KB RAM": "STM32F446：180 MHz，512 KB Flash，128 KB RAM",

    # 配置命令
    "cp platforms/stm32/defconfig_stm32f4 .config": "cp platforms/stm32/defconfig_stm32f4 .config",
    "python scripts/kconfig/generate_config.py": "python scripts/kconfig/generate_config.py",

    # 烧录命令
    "openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \\": "openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \\",
    '    -c "program build/applications/blinky/blinky.elf verify reset exit"': '    -c "program build/applications/blinky/blinky.elf verify reset exit"',

    # 时钟配置
    "PLL: 180 MHz system clock": "PLL：180 MHz 系统时钟",
    "AHB: 180 MHz": "AHB：180 MHz",
    "APB1: 45 MHz": "APB1：45 MHz",
    "APB2: 90 MHz": "APB2：90 MHz",
    "8 MHz HSE": "8 MHz HSE",

    # 硬件特性
    "ST-LINK/V2 debugger": "ST-LINK/V2 调试器",
    "LEDs on PD12-PD15": "PD12-PD15 上的 LED",
    "User button on PA0": "PA0 上的用户按钮",
    "ST-LINK/V2-B debugger": "ST-LINK/V2-B 调试器",
    "2.4\" LCD display": "2.4\" LCD 显示屏",
    "LEDs on PG13-PG14": "PG13-PG14 上的 LED",

    # 调试和烧录
    "openocd -f interface/stlink.cfg -f target/stm32f4x.cfg": "openocd -f interface/stlink.cfg -f target/stm32f4x.cfg",
    "Use ST-LINK Utility for flashing": "使用 ST-LINK Utility 进行烧录",
    "Supports mass erase and option bytes": "支持批量擦除和选项字节",

    # 性能和限制
    "Maximum 180 MHz clock": "最大 180 MHz 时钟",
    "Limited RAM compared to STM32H7": "与 STM32H7 相比 RAM 有限",
    "Use DMA for high-throughput peripherals": "对高吞吐量外设使用 DMA",
    "Enable FPU for floating-point operations": "为浮点运算启用 FPU",
    "Configure clock tree carefully": "仔细配置时钟树",

    # 通用平台术语
    "Platform Overview": "平台概述",
    "Supported Platforms": "支持的平台",
    "Platform Features": "平台功能",
    "Hardware Specifications": "硬件规格",
    "Memory Layout": "内存布局",
    "Clock Configuration": "时钟配置",
    "Peripheral Support": "外设支持",
    "Debug Interface": "调试接口",
    "Flash Programming": "Flash 编程",
    "Power Management": "电源管理",

    # Kconfig 术语
    "Configuration Options": "配置选项",
    "Platform Configuration": "平台配置",
    "Select platform": "选择平台",
    "Enable feature": "启用功能",
    "Disable feature": "禁用功能",
    "Default configuration": "默认配置",
    "Custom configuration": "自定义配置",
    "Configuration file": "配置文件",
    "Save configuration": "保存配置",
    "Load configuration": "加载配置",

    # 外设
    "Peripheral Configuration": "外设配置",
    "GPIO Configuration": "GPIO 配置",
    "UART Configuration": "UART 配置",
    "SPI Configuration": "SPI 配置",
    "I2C Configuration": "I2C 配置",
    "Timer Configuration": "定时器配置",
    "ADC Configuration": "ADC 配置",
    "DMA Configuration": "DMA 配置",
    "Interrupt Configuration": "中断配置",

    # 构建选项
    "Build Options": "构建选项",
    "Optimization Level": "优化级别",
    "Debug Symbols": "调试符号",
    "Link Time Optimization": "链接时优化",
    "Size Optimization": "大小优化",
    "Speed Optimization": "速度优化",

    # 调试选项
    "Debug Options": "调试选项",
    "Enable Debugging": "启用调试",
    "Debug Level": "调试级别",
    "Verbose Output": "详细输出",
    "Trace Support": "跟踪支持",
    "Profiling Support": "性能分析支持",

    # 常见短语
    "For more information, see": "有关更多信息，请参阅",
    "Refer to the datasheet for details": "有关详细信息，请参阅数据手册",
    "Consult the reference manual": "查阅参考手册",
    "See the platform guide": "请参阅平台指南",
    "Check the hardware documentation": "查看硬件文档",

    # 注意事项
    "Note: This feature requires": "注意：此功能需要",
    "Warning: This may cause": "警告：这可能导致",
    "Important: Make sure to": "重要：确保",
    "Tip: You can also": "提示：您还可以",
    "Caution: Be careful when": "注意：小心",
}

def translate_po_file(po_path):
    """翻译单个 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        print(f"错误: 无法加载 {po_path}: {e}")
        return 0

    translated_count = 0

    for entry in po:
        if entry.msgid and not entry.msgstr and not entry.obsolete:
            # 精确匹配
            if entry.msgid in PLATFORM_TRANSLATIONS:
                entry.msgstr = PLATFORM_TRANSLATIONS[entry.msgid]
                translated_count += 1
            # 去除首尾空白后匹配
            elif entry.msgid.strip() in PLATFORM_TRANSLATIONS:
                entry.msgstr = PLATFORM_TRANSLATIONS[entry.msgid.strip()]
                translated_count += 1

    if translated_count > 0:
        po.save(po_path)

    return translated_count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 重点处理平台和 Kconfig 文件
    target_files = [
        'platform_guides/index.po',
        'platform_guides/native.po',
        'platform_guides/stm32f4.po',
        'platform_guides/stm32h7.po',
        'platform_guides/gd32.po',
        'user_guide/kconfig_platforms.po',
        'user_guide/kconfig_peripherals.po',
        'user_guide/kconfig_osal.po',
        'user_guide/kconfig_tools.po',
        'user_guide/kconfig_tutorial.po',
        'reference/kconfig_index.po',
    ]

    print(f"处理 {len(target_files)} 个平台和配置文件...")

    total_translated = 0

    for file_path in target_files:
        full_path = locale_dir / file_path
        if full_path.exists():
            count = translate_po_file(full_path)
            if count > 0:
                print(f"  {file_path}: {count} 条")
            total_translated += count

    print(f"\n总计翻译: {total_translated} 条")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
