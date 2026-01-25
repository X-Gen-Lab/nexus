#!/usr/bin/env python3
"""
综合批量翻译工具 - 处理各种常见句子和描述
"""

import re
from pathlib import Path
import polib

COMPREHENSIVE_TRANSLATIONS = {
    # 平台描述
    "PC simulation platform for development and testing": "用于开发和测试的 PC 仿真平台",
    "Espressif ESP32 WiFi/Bluetooth SoC": "Espressif ESP32 WiFi/蓝牙 SoC",
    "Select one platform using the choice menu:": "使用选择菜单选择一个平台：",
    "Only one platform can be selected at a time.": "一次只能选择一个平台。",

    # 平台特性
    "No hardware required": "无需硬件",
    "Fast development iteration": "快速开发迭代",
    "Cross-platform support": "跨平台支持",
    "Rich peripheral set": "丰富的外设集",
    "DMA support": "DMA 支持",
    "Low power modes": "低功耗模式",

    # 平台限制
    "Simulated peripherals don't match real hardware timing": "模拟外设与实际硬件时序不匹配",
    "No real interrupt latency": "无真实中断延迟",
    "No power management": "无电源管理",
    "No hardware-specific features": "无硬件特定功能",

    # 使用建议
    "Use Native platform for algorithm development": "使用本地平台进行算法开发",
    "Validate configuration before hardware deployment": "在硬件部署前验证配置",
    "Use for CI/CD testing": "用于 CI/CD 测试",

    # STM32 描述
    "The STM32F4 platform supports STMicroelectronics STM32F4 series ARM Cortex-M4 microcontrollers.": "STM32F4 平台支持 STMicroelectronics STM32F4 系列 ARM Cortex-M4 微控制器。",
    "ARM Cortex-M4 with FPU": "带 FPU 的 ARM Cortex-M4",
    "Up to 180 MHz": "最高 180 MHz",

    # 芯片规格
    "168 MHz, 1 MB Flash, 192 KB RAM": "168 MHz，1 MB Flash，192 KB RAM",
    "180 MHz, 2 MB Flash, 256 KB RAM": "180 MHz，2 MB Flash，256 KB RAM",
    "180 MHz, 512 KB Flash, 128 KB RAM": "180 MHz，512 KB Flash，128 KB RAM",

    # 时钟配置
    "HSE: 8 MHz external crystal": "HSE：8 MHz 外部晶振",
    "PLL: 168 MHz system clock": "PLL：168 MHz 系统时钟",
    "AHB: 168 MHz": "AHB：168 MHz",
    "APB1: 42 MHz": "APB1：42 MHz",
    "APB2: 84 MHz": "APB2：84 MHz",

    # 配置命令
    "cp platforms/native/defconfig .config": "cp platforms/native/defconfig .config",
    "cp platforms/stm32/defconfig_stm32f4 .config": "cp platforms/stm32/defconfig_stm32f4 .config",
    "python scripts/kconfig/generate_config.py": "python scripts/kconfig/generate_config.py",

    # 通用描述
    "This section describes": "本节描述",
    "This chapter explains": "本章说明",
    "This guide covers": "本指南涵盖",
    "This tutorial demonstrates": "本教程演示",
    "This example shows": "本示例展示",

    # 配置说明
    "Enable this option": "启用此选项",
    "Disable this option": "禁用此选项",
    "Set this value": "设置此值",
    "Configure this parameter": "配置此参数",
    "Select this choice": "选择此项",

    # 要求和建议
    "Required": "必需",
    "Optional": "可选",
    "Recommended": "推荐",
    "Not recommended": "不推荐",
    "Deprecated": "已弃用",
    "Experimental": "实验性",

    # 状态和结果
    "Success": "成功",
    "Failed": "失败",
    "Error": "错误",
    "Warning": "警告",
    "Info": "信息",
    "Debug": "调试",

    # 操作动词
    "Enable": "启用",
    "Disable": "禁用",
    "Configure": "配置",
    "Set": "设置",
    "Get": "获取",
    "Read": "读取",
    "Write": "写入",
    "Initialize": "初始化",
    "Start": "启动",
    "Stop": "停止",
    "Reset": "重置",

    # 时间和频率
    "MHz": "MHz",
    "KHz": "KHz",
    "Hz": "Hz",
    "ms": "毫秒",
    "us": "微秒",
    "ns": "纳秒",

    # 存储单位
    "MB": "MB",
    "KB": "KB",
    "Byte": "字节",
    "Bytes": "字节",

    # 外设名称
    "GPIO": "GPIO",
    "UART": "UART",
    "SPI": "SPI",
    "I2C": "I2C",
    "ADC": "ADC",
    "DAC": "DAC",
    "PWM": "PWM",
    "Timer": "定时器",
    "DMA": "DMA",
    "RTC": "RTC",
    "WDT": "WDT",
    "CAN": "CAN",
    "USB": "USB",

    # 配置类型
    "bool": "布尔型",
    "int": "整数",
    "string": "字符串",
    "hex": "十六进制",
    "choice": "选择",

    # 逻辑值
    "true": "真",
    "false": "假",
    "yes": "是",
    "no": "否",
    "on": "开",
    "off": "关",

    # 方向
    "input": "输入",
    "output": "输出",
    "bidirectional": "双向",

    # 模式
    "mode": "模式",
    "normal mode": "正常模式",
    "debug mode": "调试模式",
    "sleep mode": "睡眠模式",
    "low power mode": "低功耗模式",

    # 优先级
    "priority": "优先级",
    "high priority": "高优先级",
    "medium priority": "中优先级",
    "low priority": "低优先级",
    "highest": "最高",
    "lowest": "最低",

    # 大小和范围
    "size": "大小",
    "length": "长度",
    "width": "宽度",
    "height": "高度",
    "range": "范围",
    "minimum": "最小值",
    "maximum": "最大值",
    "default": "默认值",

    # 状态
    "enabled": "已启用",
    "disabled": "已禁用",
    "active": "活动",
    "inactive": "非活动",
    "running": "运行中",
    "stopped": "已停止",
    "idle": "空闲",
    "busy": "忙碌",
    "ready": "就绪",
    "pending": "待处理",

    # 结果
    "OK": "正常",
    "FAIL": "失败",
    "PASS": "通过",
    "SKIP": "跳过",
    "TIMEOUT": "超时",

    # 文件和路径
    "file": "文件",
    "directory": "目录",
    "path": "路径",
    "folder": "文件夹",

    # 版本
    "version": "版本",
    "release": "发布",
    "build": "构建",
    "revision": "修订",

    # 许可
    "license": "许可证",
    "copyright": "版权",
    "author": "作者",
    "contributor": "贡献者",

    # 链接和引用
    "link": "链接",
    "reference": "参考",
    "see": "参见",
    "see also": "另请参阅",
    "refer to": "请参阅",

    # 注释
    "note": "注意",
    "warning": "警告",
    "important": "重要",
    "tip": "提示",
    "caution": "警告",

    # 示例
    "example": "示例",
    "sample": "样本",
    "demo": "演示",
    "test": "测试",

    # 文档结构
    "chapter": "章",
    "section": "节",
    "subsection": "小节",
    "paragraph": "段落",
    "list": "列表",
    "table": "表格",
    "figure": "图",
    "code": "代码",

    # 操作系统
    "Linux": "Linux",
    "Windows": "Windows",
    "macOS": "macOS",
    "RTOS": "RTOS",
    "FreeRTOS": "FreeRTOS",

    # 工具
    "compiler": "编译器",
    "linker": "链接器",
    "debugger": "调试器",
    "IDE": "IDE",
    "editor": "编辑器",

    # 网络
    "WiFi": "WiFi",
    "Bluetooth": "蓝牙",
    "Ethernet": "以太网",
    "TCP": "TCP",
    "UDP": "UDP",
    "HTTP": "HTTP",
    "HTTPS": "HTTPS",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in COMPREHENSIVE_TRANSLATIONS:
        return COMPREHENSIVE_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in COMPREHENSIVE_TRANSLATIONS.items():
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
    po_files = sorted(locale_dir.rglob('*.po'))

    print(f"处理 {len(po_files)} 个文件...")

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
