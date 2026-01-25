#!/usr/bin/env python3
"""
批量翻译工具 - 第2阶段：教程和示例
处理教程、示例应用相关的完整句子
"""

import re
from pathlib import Path
import polib

# 教程和示例相关翻译
TUTORIAL_TRANSLATIONS = {
    # 示例概述
    "This page documents the example applications included in the Nexus repository. Each example demonstrates specific features and serves as a reference for building your own applications.": "本页面记录了 Nexus 仓库中包含的示例应用程序。每个示例演示特定功能，并作为构建您自己应用程序的参考。",
    "Purpose": "目的",
    "blinky": "blinky",
    "config_demo": "config_demo",
    "freertos_demo": "freertos_demo",
    "shell_demo": "shell_demo",

    # Blinky 示例
    "**Description**: A simple LED blinky application that demonstrates basic GPIO control and HAL initialization.": "**描述**：一个简单的 LED 闪烁应用程序，演示基本的 GPIO 控制和 HAL 初始化。",
    "Repeat": "重复",
    "**Main Loop**:": "**主循环**：",
    "Green LED turns on when ready, and blinks slowly when complete.": "就绪时绿色 LED 点亮，完成时缓慢闪烁。",

    # Config Demo 示例
    "**Description**: Demonstrates the Config Manager middleware for storing and retrieving configuration data.": "**描述**：演示用于存储和检索配置数据的配置管理器中间件。",
    "**Binary Export/Import**: Exports config to binary format and reimports": "**二进制导出/导入**：将配置导出为二进制格式并重新导入",
    "**Namespace Usage**:": "**命名空间使用**：",
    "**JSON Export**:": "**JSON 导出**：",
    "How to initialize and use the Config Manager": "如何初始化和使用配置管理器",
    "How to store and retrieve different data types": "如何存储和检索不同的数据类型",
    "How to use namespaces for configuration isolation": "如何使用命名空间进行配置隔离",
    "How to export and import configurations": "如何导出和导入配置",
    "How to query configuration entries": "如何查询配置条目",
    "How to use UART for debug output": "如何使用 UART 进行调试输出",

    # FreeRTOS Demo 示例
    "**Description**: Demonstrates multi-tasking using the OSAL with FreeRTOS backend.": "**描述**：演示使用带有 FreeRTOS 后端的 OSAL 进行多任务处理。",
    "Multiple concurrent tasks": "多个并发任务",
    "Task priorities and scheduling": "任务优先级和调度",
    "Mutex for resource protection": "用于资源保护的互斥锁",
    "Semaphore for task synchronization": "用于任务同步的信号量",
    "Message queue for inter-task communication": "用于任务间通信的消息队列",
    "System statistics tracking": "系统统计跟踪",
    "LEDs for visual feedback": "用于视觉反馈的 LED",
    "The application creates four tasks:": "应用程序创建四个任务：",

    # 通用教程短语
    "What You'll Learn": "您将学到什么",
    "What you'll learn": "您将学到什么",
    "In this tutorial, you will learn:": "在本教程中，您将学习：",
    "By the end of this tutorial, you will be able to:": "完成本教程后，您将能够：",
    "This tutorial covers:": "本教程涵盖：",
    "You will learn how to:": "您将学习如何：",

    # 硬件要求
    "Hardware Requirements": "硬件要求",
    "Required Hardware": "所需硬件",
    "You will need:": "您将需要：",
    "For this tutorial, you need:": "对于本教程，您需要：",
    "Supported boards:": "支持的开发板：",
    "Any STM32F4 board": "任何 STM32F4 开发板",
    "USB cable for programming": "用于编程的 USB 电缆",
    "LED (optional)": "LED（可选）",

    # 软件要求
    "Software Requirements": "软件要求",
    "Required Software": "所需软件",
    "Before starting, install:": "开始之前，请安装：",
    "ARM GCC toolchain": "ARM GCC 工具链",
    "CMake 3.15 or later": "CMake 3.15 或更高版本",
    "OpenOCD or ST-Link": "OpenOCD 或 ST-Link",

    # 步骤说明
    "Step-by-Step Instructions": "分步说明",
    "Follow these steps:": "按照以下步骤操作：",
    "Let's get started:": "让我们开始：",
    "First, we need to:": "首先，我们需要：",
    "Next, we will:": "接下来，我们将：",
    "Finally, we can:": "最后，我们可以：",

    # 代码解释
    "Code Explanation": "代码解释",
    "Understanding the Code": "理解代码",
    "Let's break down the code:": "让我们分解代码：",
    "The code does the following:": "代码执行以下操作：",
    "Here's what happens:": "以下是发生的情况：",
    "This function:": "此函数：",
    "This line:": "此行：",

    # 构建和运行
    "Building and Running": "构建和运行",
    "Build and Run": "构建和运行",
    "To build the example:": "要构建示例：",
    "To run the example:": "要运行示例：",
    "Build the project:": "构建项目：",
    "Flash to the board:": "烧录到开发板：",
    "Run the application:": "运行应用程序：",

    # 预期输出
    "Expected Output": "预期输出",
    "Expected Behavior": "预期行为",
    "You should see:": "您应该看到：",
    "The LED should:": "LED 应该：",
    "The output will be:": "输出将是：",
    "On success, you will see:": "成功时，您将看到：",

    # 故障排除
    "Troubleshooting": "故障排除",
    "Common Issues": "常见问题",
    "If something goes wrong:": "如果出现问题：",
    "If you encounter errors:": "如果遇到错误：",
    "Check the following:": "检查以下内容：",
    "Make sure that:": "确保：",
    "Verify that:": "验证：",

    # 下一步
    "Next Steps": "下一步",
    "What's Next": "接下来做什么",
    "Now that you've completed this tutorial:": "现在您已完成本教程：",
    "Try these exercises:": "尝试这些练习：",
    "For more information, see:": "有关更多信息，请参阅：",
    "Continue to the next tutorial:": "继续下一个教程：",

    # 参考
    "References": "参考",
    "Related Topics": "相关主题",
    "See Also": "另请参阅",
    "For more details, refer to:": "有关更多详细信息，请参阅：",
    "Additional resources:": "其他资源：",

    # 功能特性
    "Features": "功能特性",
    "Key Features": "主要功能",
    "This example demonstrates:": "此示例演示：",
    "Main features:": "主要功能：",
    "Highlights:": "亮点：",

    # 学习目标
    "Learning Objectives": "学习目标",
    "Objectives": "目标",
    "Goals": "目标",
    "After completing this tutorial:": "完成本教程后：",
    "You will understand:": "您将理解：",
    "You will be able to:": "您将能够：",
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
            if entry.msgid in TUTORIAL_TRANSLATIONS:
                entry.msgstr = TUTORIAL_TRANSLATIONS[entry.msgid]
                translated_count += 1
            # 去除首尾空白后匹配
            elif entry.msgid.strip() in TUTORIAL_TRANSLATIONS:
                entry.msgstr = TUTORIAL_TRANSLATIONS[entry.msgid.strip()]
                translated_count += 1

    if translated_count > 0:
        po.save(po_path)

    return translated_count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 重点处理教程和示例文件
    target_files = [
        'tutorials/examples.po',
        'tutorials/first_application.po',
        'tutorials/gpio_control.po',
        'tutorials/task_creation.po',
        'tutorials/uart_communication.po',
        'tutorials/index.po',
    ]

    print(f"处理 {len(target_files)} 个教程文件...")

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
