#!/usr/bin/env python3
"""
批量翻译工具 - 第1部分：教程和入门指南
包含常见教程句子和短语的翻译
"""

import re
from pathlib import Path
import polib

# 教程和指南常用翻译
TUTORIAL_TRANSLATIONS = {
    # 标题
    "Your First Nexus Application": "您的第一个 Nexus 应用程序",
    "Learning Objectives": "学习目标",
    "Prerequisites": "前置条件",
    "Common Issues and Solutions": "常见问题和解决方案",
    "Additional Exercises": "附加练习",
    "Best Practices": "最佳实践",
    "Next Steps": "下一步",

    # 教程引导句
    "This tutorial guides you through": "本教程将指导您",
    "By the end of this tutorial, you will:": "完成本教程后，您将能够：",
    "Before starting, ensure you have:": "开始之前，请确保您已：",
    "First, create a new directory for your project:": "首先，为您的项目创建一个新目录：",
    "Create the following directory structure:": "创建以下目录结构：",
    "You can add Nexus to your project in two ways:": "您可以通过两种方式将 Nexus 添加到项目中：",

    # 学习目标
    "Understand the basic structure of a Nexus application": "理解 Nexus 应用程序的基本结构",
    "Know how to configure CMake for Nexus projects": "了解如何为 Nexus 项目配置 CMake",
    "Be able to initialize the HAL subsystem": "能够初始化 HAL 子系统",
    "Create and control GPIO devices": "创建和控制 GPIO 设备",
    "Build and flash your application to hardware": "构建应用程序并烧录到硬件",

    # 前置条件
    "ARM GCC toolchain installed": "已安装 ARM GCC 工具链",
    "OpenOCD or ST-Link tools for flashing": "用于烧录的 OpenOCD 或 ST-Link 工具",

    # 步骤标题
    "Step 1: Create Project Structure": "步骤 1：创建项目结构",
    "Step 2: Add Nexus as Dependency": "步骤 2：添加 Nexus 依赖",
    "Step 3: Create CMakeLists.txt": "步骤 3：创建 CMakeLists.txt",
    "Step 4: Write Your First Application": "步骤 4：编写您的第一个应用程序",
    "Step 5: Build for Native Platform": "步骤 5：为本地平台构建",
    "Step 6: Build for STM32F4": "步骤 6：为 STM32F4 构建",
    "Step 7: Flash to Hardware": "步骤 7：烧录到硬件",
    "Step 8: Verify Operation": "步骤 8：验证运行",

    # 配置说明
    "GPIO Configuration": "GPIO 配置",
    "GPIO Operations": "GPIO 操作",
    "Platform-Specific Setup": "平台特定设置",

    # 构建相关
    "The native platform is useful for:": "本地平台适用于：",
    "Testing application logic without hardware": "在没有硬件的情况下测试应用程序逻辑",
    "Running unit tests": "运行单元测试",
    "Debugging on your development machine": "在开发机器上调试",
    "Now let's build for the actual target hardware:": "现在让我们为实际目标硬件构建：",

    # 烧录说明
    "Flash with OpenOCD": "使用 OpenOCD 烧录",
    "Connect to the board": "连接到开发板",
    "Click \"Program & Verify\"": "点击\"编程和验证\"",

    # 验证说明
    "The LED should toggle every 500ms": "LED 应该每 500 毫秒切换一次",
    "Press the reset button to restart the application": "按下复位按钮重启应用程序",
    "If the LED doesn't blink:": "如果 LED 不闪烁：",
    "Check that the board is powered": "检查开发板是否通电",
    "Verify the correct LED pin is configured": "验证是否配置了正确的 LED 引脚",
    "Check that the application was flashed successfully": "检查应用程序是否成功烧录",

    # GPIO 配置详细说明
    "Configure as input": "配置为输入",
    "Configure as output": "配置为输出",
    "No pull resistor": "无上拉电阻",
    "Enable pull-up resistor": "启用上拉电阻",
    "Enable pull-down resistor": "启用下拉电阻",

    # 结束语
    "Congratulations! You've created your first Nexus application. Now you can:": "恭喜！您已创建了第一个 Nexus 应用程序。现在您可以：",
    "Try these exercises to reinforce your learning:": "尝试这些练习来巩固您的学习：",

    # 最佳实践
    "Always check return values from HAL functions for errors": "始终检查 HAL 函数的返回值以发现错误",
    "Clearly document which pins are used for what purpose": "清楚地记录哪些引脚用于什么目的",
    "Keep initialization code separate from main loop logic": "将初始化代码与主循环逻辑分开",
    "Use descriptive names for constants and functions": "为常量和函数使用描述性名称",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in TUTORIAL_TRANSLATIONS:
        return TUTORIAL_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in TUTORIAL_TRANSLATIONS.items():
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
