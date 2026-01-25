#!/usr/bin/env python3
"""
批量翻译工具 - 平台指南
"""

import re
from pathlib import Path
import polib

PLATFORM_TRANSLATIONS = {
    # 平台通用
    "Platform guide": "平台指南",
    "Platform overview": "平台概述",
    "Platform features": "平台特性",
    "Platform support": "平台支持",
    "Supported platforms": "支持的平台",
    "Platform selection": "平台选择",
    "Platform configuration": "平台配置",

    # STM32 系列
    "STM32 platform": "STM32 平台",
    "STM32F4 series": "STM32F4 系列",
    "STM32H7 series": "STM32H7 系列",
    "STM32 Discovery": "STM32 Discovery",
    "STM32 Nucleo": "STM32 Nucleo",
    "STM32CubeMX": "STM32CubeMX",

    # GD32 系列
    "GD32 platform": "GD32 平台",
    "GD32F4 series": "GD32F4 系列",
    "GD32 development board": "GD32 开发板",

    # 本地平台
    "Native platform": "本地平台",
    "Host platform": "主机平台",
    "Simulation platform": "仿真平台",

    # 硬件特性
    "Hardware features": "硬件特性",
    "CPU core": "CPU 内核",
    "Clock speed": "时钟速度",
    "Flash memory": "Flash 存储器",
    "RAM memory": "RAM 存储器",
    "Peripherals": "外设",
    "GPIO pins": "GPIO 引脚",
    "Communication interfaces": "通信接口",

    # 开发板
    "Development board": "开发板",
    "Evaluation board": "评估板",
    "Discovery board": "Discovery 板",
    "Nucleo board": "Nucleo 板",
    "Custom board": "自定义板",

    # 引脚配置
    "Pin configuration": "引脚配置",
    "Pin mapping": "引脚映射",
    "Pin assignment": "引脚分配",
    "Alternate function": "复用功能",
    "Pin mode": "引脚模式",

    # 时钟配置
    "Clock configuration": "时钟配置",
    "System clock": "系统时钟",
    "Peripheral clock": "外设时钟",
    "Clock source": "时钟源",
    "Clock tree": "时钟树",
    "PLL configuration": "PLL 配置",

    # 中断配置
    "Interrupt configuration": "中断配置",
    "Interrupt priority": "中断优先级",
    "Interrupt handler": "中断处理程序",
    "NVIC configuration": "NVIC 配置",
    "Interrupt vector": "中断向量",

    # DMA 配置
    "DMA configuration": "DMA 配置",
    "DMA channel": "DMA 通道",
    "DMA stream": "DMA 流",
    "DMA transfer": "DMA 传输",
    "DMA priority": "DMA 优先级",

    # 电源管理
    "Power management": "电源管理",
    "Power modes": "电源模式",
    "Sleep mode": "睡眠模式",
    "Stop mode": "停止模式",
    "Standby mode": "待机模式",
    "Low power mode": "低功耗模式",

    # 启动和引导
    "Boot configuration": "启动配置",
    "Bootloader": "引导加载程序",
    "Boot mode": "启动模式",
    "Boot sequence": "启动序列",
    "Reset handler": "复位处理程序",

    # 内存配置
    "Memory layout": "内存布局",
    "Memory map": "内存映射",
    "Flash layout": "Flash 布局",
    "RAM layout": "RAM 布局",
    "Linker script": "链接器脚本",

    # 调试接口
    "Debug interface": "调试接口",
    "JTAG interface": "JTAG 接口",
    "SWD interface": "SWD 接口",
    "Debug probe": "调试探针",
    "ST-Link": "ST-Link",
    "J-Link": "J-Link",

    # 烧录
    "Flashing": "烧录",
    "Flash programming": "Flash 编程",
    "Flash tool": "烧录工具",
    "Flash address": "烧录地址",
    "Flash command": "烧录命令",

    # 外设支持
    "Peripheral support": "外设支持",
    "GPIO support": "GPIO 支持",
    "UART support": "UART 支持",
    "SPI support": "SPI 支持",
    "I2C support": "I2C 支持",
    "ADC support": "ADC 支持",
    "Timer support": "定时器支持",

    # 示例
    "Platform examples": "平台示例",
    "Example projects": "示例项目",
    "Demo applications": "演示应用",
    "Sample code": "示例代码",

    # 移植
    "Platform porting": "平台移植",
    "Porting guide": "移植指南",
    "HAL porting": "HAL 移植",
    "Driver porting": "驱动移植",
    "Board support": "板级支持",

    # 限制
    "Platform limitations": "平台限制",
    "Known issues": "已知问题",
    "Workarounds": "变通方法",
    "Restrictions": "限制",

    # 性能
    "Platform performance": "平台性能",
    "Benchmark results": "基准测试结果",
    "Performance metrics": "性能指标",
    "Optimization tips": "优化技巧",

    # 兼容性
    "Platform compatibility": "平台兼容性",
    "Compatible boards": "兼容板",
    "Compatible chips": "兼容芯片",
    "Version compatibility": "版本兼容性",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in PLATFORM_TRANSLATIONS:
        return PLATFORM_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in PLATFORM_TRANSLATIONS.items():
        if key.lower() == msgid.lower():
            return value

    return ""

def process_po_file(po_path):
    """处理 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        return 0

    count = 0
    for entry in po:
        if entry.msgid and not entry.msgstr and not entry.obsolete:
            translation = translate_entry(entry.msgid)
            if translation:
                entry.msgstr = translation
                count += 1

    if count > 0:
        po.save(po_path)

    return count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    # 处理 platform_guides 目录
    po_files = sorted(locale_dir.glob('platform_guides/*.po'))

    print(f"处理 {len(po_files)} 个平台指南文件...")

    total = 0
    for po_file in po_files:
        count = process_po_file(po_file)
        if count > 0:
            print(f"✓ {po_file.relative_to(locale_dir)}: {count} 条")
            total += count

    print(f"\n总计翻译: {total} 条")
    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
