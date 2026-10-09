#!/usr/bin/env python3
"""
批量翻译工具 - Kconfig 配置文档
专门处理 Kconfig 相关的配置选项和说明
"""

import re
from pathlib import Path
import polib

KCONFIG_TRANSLATIONS = {
    # Kconfig 基础术语
    "Kconfig": "Kconfig",
    "menuconfig": "menuconfig",
    "defconfig": "defconfig",

    # 配置选项类型
    "bool": "布尔型",
    "tristate": "三态",
    "string": "字符串",
    "int": "整数",
    "hex": "十六进制",

    # 配置状态
    "y": "是",
    "n": "否",
    "m": "模块",

    # 平台相关
    "Platform selection": "平台选择",
    "Select target platform": "选择目标平台",
    "Platform-specific options": "平台特定选项",
    "Platform configuration": "平台配置",
    "Target platform": "目标平台",
    "Hardware platform": "硬件平台",

    # STM32 相关
    "STM32 platform": "STM32 平台",
    "STM32F4 series": "STM32F4 系列",
    "STM32H7 series": "STM32H7 系列",
    "STM32 chip selection": "STM32 芯片选择",
    "STM32 peripheral configuration": "STM32 外设配置",

    # GD32 相关
    "GD32 platform": "GD32 平台",
    "GD32 series": "GD32 系列",
    "GD32 chip selection": "GD32 芯片选择",

    # OSAL 相关
    "OSAL configuration": "OSAL 配置",
    "OSAL backend selection": "OSAL 后端选择",
    "Select OSAL backend": "选择 OSAL 后端",
    "FreeRTOS backend": "FreeRTOS 后端",
    "Bare metal backend": "裸机后端",
    "RT-Thread backend": "RT-Thread 后端",
    "Zephyr backend": "Zephyr 后端",

    # OSAL 配置项
    "Task configuration": "任务配置",
    "Maximum number of tasks": "最大任务数",
    "Default task stack size": "默认任务栈大小",
    "Task priority levels": "任务优先级级别",
    "Idle task stack size": "空闲任务栈大小",

    # 互斥锁和信号量
    "Mutex configuration": "互斥锁配置",
    "Semaphore configuration": "信号量配置",
    "Maximum mutexes": "最大互斥锁数",
    "Maximum semaphores": "最大信号量数",
    "Enable recursive mutexes": "启用递归互斥锁",
    "Enable priority inheritance": "启用优先级继承",

    # 队列配置
    "Queue configuration": "队列配置",
    "Maximum queues": "最大队列数",
    "Default queue length": "默认队列长度",
    "Queue item size": "队列项大小",

    # 定时器配置
    "Timer configuration": "定时器配置",
    "Software timer support": "软件定时器支持",
    "Maximum timers": "最大定时器数",
    "Timer task priority": "定时器任务优先级",
    "Timer task stack size": "定时器任务栈大小",
    "Timer queue length": "定时器队列长度",

    # 内存配置
    "Memory configuration": "内存配置",
    "Heap size": "堆大小",
    "Stack size": "栈大小",
    "Enable memory protection": "启用内存保护",
    "Memory allocation scheme": "内存分配方案",

    # 外设配置
    "Peripheral configuration": "外设配置",
    "Enable GPIO": "启用 GPIO",
    "Enable UART": "启用 UART",
    "Enable SPI": "启用 SPI",
    "Enable I2C": "启用 I2C",
    "Enable ADC": "启用 ADC",
    "Enable DAC": "启用 DAC",
    "Enable PWM": "启用 PWM",
    "Enable Timer": "启用定时器",
    "Enable DMA": "启用 DMA",
    "Enable RTC": "启用 RTC",
    "Enable WDT": "启用 WDT",
    "Enable CAN": "启用 CAN",
    "Enable USB": "启用 USB",

    # GPIO 配置
    "GPIO configuration": "GPIO 配置",
    "Number of GPIO ports": "GPIO 端口数",
    "GPIO interrupt support": "GPIO 中断支持",
    "GPIO alternate function": "GPIO 复用功能",

    # UART 配置
    "UART configuration": "UART 配置",
    "Number of UART instances": "UART 实例数",
    "UART buffer size": "UART 缓冲区大小",
    "UART DMA support": "UART DMA 支持",
    "Default baud rate": "默认波特率",

    # SPI 配置
    "SPI configuration": "SPI 配置",
    "Number of SPI instances": "SPI 实例数",
    "SPI DMA support": "SPI DMA 支持",
    "SPI buffer size": "SPI 缓冲区大小",

    # I2C 配置
    "I2C configuration": "I2C 配置",
    "Number of I2C instances": "I2C 实例数",
    "I2C DMA support": "I2C DMA 支持",
    "I2C clock speed": "I2C 时钟速度",

    # 工具配置
    "Tools configuration": "工具配置",
    "Enable shell": "启用 Shell",
    "Enable logging": "启用日志",
    "Enable config manager": "启用配置管理器",
    "Enable init framework": "启用初始化框架",

    # Shell 配置
    "Shell configuration": "Shell 配置",
    "Maximum command length": "最大命令长度",
    "Command history size": "命令历史大小",
    "Maximum arguments": "最大参数数量",
    "Shell prompt": "Shell 提示符",
    "Enable auto-completion": "启用自动补全",
    "Enable command history": "启用命令历史",

    # 日志配置
    "Logging configuration": "日志配置",
    "Log level": "日志级别",
    "Log buffer size": "日志缓冲区大小",
    "Enable timestamps": "启用时间戳",
    "Enable colors": "启用颜色",
    "Log output backend": "日志输出后端",

    # 日志级别
    "Error level": "错误级别",
    "Warning level": "警告级别",
    "Info level": "信息级别",
    "Debug level": "调试级别",
    "Verbose level": "详细级别",

    # 配置管理器
    "Config manager configuration": "配置管理器配置",
    "Storage backend": "存储后端",
    "Flash storage": "Flash 存储",
    "RAM storage": "RAM 存储",
    "Maximum config entries": "最大配置条目数",
    "Config key length": "配置键长度",
    "Config value length": "配置值长度",

    # 构建配置
    "Build configuration": "构建配置",
    "Build type": "构建类型",
    "Debug build": "调试构建",
    "Release build": "发布构建",
    "Enable optimization": "启用优化",
    "Optimization level": "优化级别",
    "Enable assertions": "启用断言",
    "Enable debug symbols": "启用调试符号",

    # 调试配置
    "Debug configuration": "调试配置",
    "Enable debug output": "启用调试输出",
    "Debug UART port": "调试 UART 端口",
    "Debug baud rate": "调试波特率",
    "Enable stack checking": "启用栈检查",
    "Enable heap checking": "启用堆检查",

    # 测试配置
    "Test configuration": "测试配置",
    "Enable unit tests": "启用单元测试",
    "Enable integration tests": "启用集成测试",
    "Test framework": "测试框架",
    "Enable code coverage": "启用代码覆盖率",
    "Enable mocking": "启用模拟",

    # 文档配置
    "Documentation configuration": "文档配置",
    "Generate API docs": "生成 API 文档",
    "Generate user guide": "生成用户指南",
    "Documentation format": "文档格式",

    # 依赖关系
    "depends on": "依赖于",
    "select": "选择",
    "imply": "暗示",
    "default": "默认",
    "range": "范围",
    "help": "帮助",

    # 配置说明
    "This option enables": "此选项启用",
    "This option disables": "此选项禁用",
    "This option configures": "此选项配置",
    "This option sets": "此选项设置",
    "This option selects": "此选项选择",
    "When enabled": "启用时",
    "When disabled": "禁用时",
    "If selected": "如果选择",
    "If not selected": "如果未选择",

    # 配置提示
    "Select this option to": "选择此选项以",
    "Enable this to": "启用此项以",
    "Disable this to": "禁用此项以",
    "Set this to": "将此设置为",
    "Choose this for": "选择此项用于",

    # 配置警告
    "Warning": "警告",
    "Note": "注意",
    "Important": "重要",
    "Deprecated": "已弃用",
    "Experimental": "实验性",
    "Recommended": "推荐",
    "Not recommended": "不推荐",
    "Required": "必需",
    "Optional": "可选",

    # 配置组
    "General configuration": "常规配置",
    "Advanced configuration": "高级配置",
    "Expert configuration": "专家配置",
    "Hardware configuration": "硬件配置",
    "Software configuration": "软件配置",
    "System configuration": "系统配置",

    # 配置菜单
    "Main menu": "主菜单",
    "Submenu": "子菜单",
    "Configuration menu": "配置菜单",
    "Options": "选项",
    "Settings": "设置",
    "Preferences": "首选项",

    # 配置操作
    "Save configuration": "保存配置",
    "Load configuration": "加载配置",
    "Reset configuration": "重置配置",
    "Export configuration": "导出配置",
    "Import configuration": "导入配置",
    "Validate configuration": "验证配置",

    # 配置文件
    "Configuration file": "配置文件",
    "Default configuration": "默认配置",
    "Custom configuration": "自定义配置",
    "Platform defconfig": "平台默认配置",
    "Board defconfig": "开发板默认配置",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in KCONFIG_TRANSLATIONS:
        return KCONFIG_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in KCONFIG_TRANSLATIONS.items():
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

    # 只处理 kconfig 相关文件
    po_files = []
    for pattern in ['**/kconfig*.po', '**/config*.po']:
        po_files.extend(locale_dir.glob(pattern))

    po_files = sorted(set(po_files))

    print(f"处理 {len(po_files)} 个 Kconfig 相关文件...")

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
