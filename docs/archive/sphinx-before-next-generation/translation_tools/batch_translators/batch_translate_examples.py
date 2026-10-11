#!/usr/bin/env python3
"""
批量翻译工具 - 示例和演示
"""

import re
from pathlib import Path
import polib

EXAMPLES_TRANSLATIONS = {
    # 示例位置和描述
    "All examples are located in the ``applications/`` directory of the repository.": "所有示例都位于仓库的 ``applications/`` 目录中。",
    "The Nexus repository includes the following example applications:": "Nexus 仓库包含以下示例应用程序：",

    # 示例类型
    "Basic GPIO control": "基本 GPIO 控制",
    "LED blinking, GPIO initialization, HAL basics": "LED 闪烁、GPIO 初始化、HAL 基础",
    "Config storage, namespaces, import/export, UART output": "配置存储、命名空间、导入/导出、UART 输出",
    "Multi-tasking with OSAL": "使用 OSAL 的多任务",
    "Tasks, mutexes, semaphores, queues, producer-consumer pattern": "任务、互斥锁、信号量、队列、生产者-消费者模式",
    "Command-line interface": "命令行界面",
    "Shell commands, UART CLI, command registration, line editing": "Shell 命令、UART CLI、命令注册、行编辑",

    # 位置
    "**Location**: ``applications/blinky/``": "**位置**：``applications/blinky/``",
    "**Location**: ``applications/config_demo/``": "**位置**：``applications/config_demo/``",
    "**Location**: ``applications/freertos_demo/``": "**位置**：``applications/freertos_demo/``",
    "**Location**: ``applications/shell_demo/``": "**位置**：``applications/shell_demo/``",

    # 功能特性
    "HAL system initialization": "HAL 系统初始化",
    "GPIO configuration as output": "GPIO 配置为输出",
    "LED blinking in sequence": "LED 顺序闪烁",
    "Error handling with LED indicators": "使用 LED 指示器的错误处理",

    # 构建和运行
    "Building and Running": "构建和运行",
    "Build and Flash": "构建和烧录",
    "Build the Project": "构建项目",
    "Flash to Hardware": "烧录到硬件",
    "Run the Application": "运行应用程序",

    # 预期行为
    "Expected Behavior": "预期行为",
    "Expected Output": "预期输出",
    "Expected Result": "预期结果",
    "All four LEDs on the STM32F4 Discovery board blink in sequence:": "STM32F4 Discovery 板上的所有四个 LED 按顺序闪烁：",

    # 代码部分
    "Key Code Sections": "关键代码部分",
    "Code Highlights": "代码亮点",
    "Important Code": "重要代码",
    "**LED Initialization**:": "**LED 初始化**：",

    # 学习成果
    "Learning Outcomes": "学习成果",
    "What You Will Learn": "您将学到什么",
    "After studying this example, you will understand:": "学习此示例后，您将理解：",
    "How to initialize the HAL subsystem": "如何初始化 HAL 子系统",
    "How to configure GPIO pins as outputs": "如何将 GPIO 引脚配置为输出",
    "How to control LED states": "如何控制 LED 状态",
    "How to use HAL delay functions": "如何使用 HAL 延迟函数",
    "Basic error handling patterns": "基本错误处理模式",

    # 配置演示
    "Configuration storage and retrieval": "配置存储和检索",
    "Namespace isolation": "命名空间隔离",
    "Configuration query and enumeration": "配置查询和枚举",
    "JSON import/export": "JSON 导入/导出",
    "Binary import/export": "二进制导入/导出",
    "UART output for demonstration": "用于演示的 UART 输出",

    # 硬件要求
    "STM32F4 Discovery board": "STM32F4 Discovery 板",
    "Serial terminal at 115200 baud": "115200 波特率的串口终端",
    "USB cable for power and programming": "用于供电和编程的 USB 线",
    "UART-to-USB adapter": "UART 转 USB 适配器",

    # 演示步骤
    "The application runs through several demonstrations:": "应用程序运行多个演示：",
    "**Basic Configuration**: Stores and retrieves various data types": "**基本配置**：存储和检索各种数据类型",
    "**Query**: Lists all configuration entries": "**查询**：列出所有配置条目",
    "**JSON Export/Import**: Exports config to JSON and reimports": "**JSON 导出/导入**：将配置导出为 JSON 并重新导入",
    "**Binary Export/Import**: Exports config to binary and reimports": "**二进制导出/导入**：将配置导出为二进制并重新导入",
    "**Namespace Demo**: Shows namespace isolation": "**命名空间演示**：显示命名空间隔离",

    # 任务演示
    "Task creation and management": "任务创建和管理",
    "Mutex usage for resource protection": "使用互斥锁保护资源",
    "Semaphore usage for synchronization": "使用信号量进行同步",
    "Queue usage for inter-task communication": "使用队列进行任务间通信",
    "Producer-consumer pattern": "生产者-消费者模式",

    # Shell 演示
    "Command registration": "命令注册",
    "Command execution": "命令执行",
    "Command history": "命令历史",
    "Auto-completion": "自动补全",
    "Line editing": "行编辑",

    # 输出示例
    "Console output": "控制台输出",
    "Serial output": "串口输出",
    "Debug output": "调试输出",
    "Log output": "日志输出",

    # 修改和扩展
    "Modifying the Example": "修改示例",
    "Extending the Example": "扩展示例",
    "Customizing the Example": "自定义示例",
    "Try modifying": "尝试修改",
    "Try adding": "尝试添加",
    "Experiment with": "尝试",

    # 故障排除
    "If LEDs don't blink": "如果 LED 不闪烁",
    "If no output appears": "如果没有输出",
    "If build fails": "如果构建失败",
    "Check connections": "检查连接",
    "Check configuration": "检查配置",
    "Check serial port": "检查串口",

    # 下一步
    "Next Steps": "下一步",
    "Further Exploration": "进一步探索",
    "Try these exercises": "尝试这些练习",
    "Explore other examples": "探索其他示例",
    "Read the documentation": "阅读文档",

    # 参考
    "Related Examples": "相关示例",
    "Related Documentation": "相关文档",
    "API Reference": "API 参考",
    "User Guide": "用户指南",

    # 通用描述
    "This example demonstrates": "此示例演示",
    "This demo shows": "此演示展示",
    "This application illustrates": "此应用程序说明",
    "The purpose of this example": "此示例的目的",

    # 功能列表
    "Features demonstrated": "演示的功能",
    "Key features": "主要功能",
    "Highlights": "亮点",
    "Capabilities shown": "展示的能力",

    # 要求
    "Requirements": "要求",
    "Prerequisites": "前置条件",
    "Dependencies": "依赖",
    "Hardware needed": "所需硬件",
    "Software needed": "所需软件",

    # 设置
    "Setup instructions": "设置说明",
    "Hardware setup": "硬件设置",
    "Software setup": "软件设置",
    "Configuration steps": "配置步骤",

    # 使用
    "Usage instructions": "使用说明",
    "How to use": "如何使用",
    "Running the example": "运行示例",
    "Testing the example": "测试示例",

    # 代码结构
    "Code structure": "代码结构",
    "File organization": "文件组织",
    "Main components": "主要组件",
    "Key functions": "关键函数",

    # 注释
    "Code comments": "代码注释",
    "Inline documentation": "内联文档",
    "Function descriptions": "函数描述",
    "Variable descriptions": "变量描述",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in EXAMPLES_TRANSLATIONS:
        return EXAMPLES_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in EXAMPLES_TRANSLATIONS.items():
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
