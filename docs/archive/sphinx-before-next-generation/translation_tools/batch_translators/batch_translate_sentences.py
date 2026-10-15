#!/usr/bin/env python3
"""
批量翻译工具 - 完整句子翻译
处理常见的完整句子和段落
"""

import re
from pathlib import Path
import polib

SENTENCE_TRANSLATIONS = {
    # 完整的教程句子
    "This tutorial guides you through creating your first Nexus application from scratch. You'll learn how to set up a project, configure the build system, and write a simple LED blinky application.": "本教程将指导您从头开始创建第一个 Nexus 应用程序。您将学习如何设置项目、配置构建系统以及编写一个简单的 LED 闪烁应用程序。",

    "The Nexus Embedded Platform uses CMake as its build system, providing a flexible and powerful way to configure and build projects for multiple platforms.": "Nexus 嵌入式平台使用 CMake 作为其构建系统，为多个平台的项目配置和构建提供了灵活而强大的方式。",

    "Nexus uses Google Test for unit testing and provides comprehensive testing infrastructure including coverage analysis and static analysis tools.": "Nexus 使用 Google Test 进行单元测试，并提供全面的测试基础设施，包括覆盖率分析和静态分析工具。",

    "The following diagram illustrates the complete build workflow from configuration to final binary:": "下图说明了从配置到最终二进制文件的完整构建工作流程：",

    # 配置和构建相关句子
    "Create ``CMakeLists.txt`` with the following content:": "创建包含以下内容的 ``CMakeLists.txt``：",
    "Create ``main.c`` with a simple LED blinky:": "创建一个简单的 LED 闪烁程序 ``main.c``：",
    "This creates ``build-stm32f4/app.elf`` and ``build-stm32f4/app.bin`` files.": "这将创建 ``build-stm32f4/app.elf`` 和 ``build-stm32f4/app.bin`` 文件。",

    # 配置结构说明
    "The ``hal_gpio_config_t`` structure defines GPIO behavior:": "``hal_gpio_config_t`` 结构定义了 GPIO 行为：",
    "The ``hal_gpio_config_t`` structure provides fine-grained control:": "``hal_gpio_config_t`` 结构提供了细粒度控制：",

    # 配置项说明
    "Input or output mode": "输入或输出模式",
    "Pull-up, pull-down, or none": "上拉、下拉或无",
    "Push-pull or open-drain": "推挽或开漏",

    # 平台说明
    "Initializes platform-specific hardware": "初始化平台特定硬件",
    "For LED control, low speed is sufficient. Use higher speeds for high-frequency signals.": "对于 LED 控制，低速就足够了。对于高频信号使用更高的速度。",

    # 故障排除
    "Solution: Install ARM toolchain and ensure it's in PATH, or specify ``TOOLCHAIN_PREFIX``.": "解决方案：安装 ARM 工具链并确保它在 PATH 中，或指定 ``TOOLCHAIN_PREFIX``。",
    "Solution: Use Visual Studio Developer Command Prompt or install MinGW-w64.": "解决方案：使用 Visual Studio 开发人员命令提示符或安装 MinGW-w64。",
    "Solution: Validate ``.config`` file or regenerate from defconfig:": "解决方案：验证 ``.config`` 文件或从 defconfig 重新生成：",

    # Kconfig 说明
    "During CMake configuration, the ``generate_config.py`` script processes Kconfig files and generates ``nexus_config.h``.": "在 CMake 配置期间，``generate_config.py`` 脚本处理 Kconfig 文件并生成 ``nexus_config.h``。",
    "CMake tracks all Kconfig files and reconfigures when they change.": "CMake 跟踪所有 Kconfig 文件，并在它们更改时重新配置。",
    "If no ``.config`` exists, a default configuration is generated from the platform's defconfig.": "如果不存在 ``.config``，则从平台的 defconfig 生成默认配置。",

    # 输出文件说明
    "Executable with debug symbols": "带有调试符号的可执行文件",
    "Raw binary for flashing": "用于烧录的原始二进制文件",
    "Intel HEX format": "Intel HEX 格式",
    "Memory map file": "内存映射文件",

    # 构建技巧
    "Clean build artifacts": "清理构建文件",
    "Or delete build directory": "或删除构建目录",

    # 测试相关句子
    "Place test files in the ``tests/`` directory with the following structure::": "将测试文件放在 ``tests/`` 目录中，结构如下：",

    # 测试类型说明
    "Verify correct behavior with valid inputs": "验证有效输入的正确行为",
    "Verify error handling with invalid inputs": "验证无效输入的错误处理",
    "Test edge cases and limits": "测试边界情况和限制",
    "Test component interactions": "测试组件交互",

    # 测试最佳实践
    "Each test should be independent and not rely on other tests": "每个测试应该独立，不依赖其他测试",
    "Use descriptive assertion messages": "使用描述性断言消息",
    "Use fixtures for common setup and cleanup": "使用固件进行通用设置和清理",
    "Use mocks for hardware and OS dependencies": "使用模拟来处理硬件和操作系统依赖",
    "Include boundary conditions and error cases": "包括边界条件和错误情况",
    "Unit tests should run quickly": "单元测试应该快速运行",
    "Use parameterized tests for similar cases": "对类似情况使用参数化测试",

    # 构建系统说明
    "The project follows a hierarchical CMake structure:": "项目遵循分层的 CMake 结构：",
    "Prevents in-source builds": "防止源码内构建",
    "Declares build options": "声明构建选项",
    "Configures compiler flags": "配置编译器标志",
    "Integrates Kconfig configuration system": "集成 Kconfig 配置系统",
    "Includes subdirectories": "包含子目录",

    # 工具链说明
    "Verify installation:": "验证安装：",
    "If the toolchain is not in PATH:": "如果工具链不在 PATH 中：",

    # 平台构建
    "Open ST-Link Utility": "打开 ST-Link 实用程序",
    "Connect to the board": "连接到开发板",
    "Load ``build-stm32f4/app.bin`` at address ``0x08000000``": "在地址 ``0x08000000`` 加载 ``build-stm32f4/app.bin``",

    # 验证操作
    "The LED should toggle every 500ms": "LED 应该每 500 毫秒切换一次",
    "Press the reset button to restart the application": "按下复位按钮重启应用程序",
    "If the LED doesn't blink:": "如果 LED 不闪烁：",
    "Check that the board is powered": "检查开发板是否通电",
    "Verify the correct LED pin is configured": "验证是否配置了正确的 LED 引脚",
    "Check that the application was flashed successfully": "检查应用程序是否成功烧录",

    # 练习说明
    "Try these exercises to reinforce your learning:": "尝试这些练习来巩固您的学习：",
    "Modify the code to blink all four LEDs on the STM32F4 Discovery board": "修改代码以使 STM32F4 Discovery 板上的所有四个 LED 闪烁",
    "Add a button to change the blink speed when pressed": "添加一个按钮，按下时改变闪烁速度",
    "Add more robust error handling with LED error indicators": "添加更强大的错误处理和 LED 错误指示器",

    # 最佳实践
    "Always check return values from HAL functions for errors": "始终检查 HAL 函数的返回值以发现错误",
    "Clearly document which pins are used for what purpose": "清楚地记录哪些引脚用于什么目的",
    "Keep initialization code separate from main loop logic": "将初始化代码与主循环逻辑分开",
    "Use descriptive names for constants and functions": "为常量和函数使用描述性名称",

    # 结束语
    "Congratulations! You've created your first Nexus application. Now you can:": "恭喜！您已创建了第一个 Nexus 应用程序。现在您可以：",
    "Explore the :doc:`../user_guide/hal` for more HAL features": "探索 :doc:`../user_guide/hal` 了解更多 HAL 特性",

    # 其他常见句子
    "Check :doc:`../development/contributing` for contribution guidelines": "查看 :doc:`../development/contributing` 了解贡献指南",
    "See :doc:`ide_integration` for detailed IDE setup guides.": "查看 :doc:`ide_integration` 了解详细的 IDE 设置指南。",

    # 构建输出
    "After building, the directory structure:": "构建后的目录结构：",
    "For embedded targets:": "对于嵌入式目标：",

    # 覆盖率
    "Enable coverage and generate reports:": "启用覆盖率并生成报告：",
    "Minimum code coverage: **90%**": "最低代码覆盖率：**90%**",
    "All public APIs must have tests": "所有公共 API 必须有测试",
    "All error paths must be tested": "所有错误路径必须经过测试",

    # CI/CD
    "Tests run automatically on every PR via GitHub Actions:": "测试通过 GitHub Actions 在每个 PR 上自动运行：",
    "Coverage reporting": "覆盖率报告",
    "Memory sanitizer checks": "内存消毒器检查",

    # 平台特定
    "Build with sanitizers for memory error detection::": "使用消毒器构建以检测内存错误：",

    # 短句子
    "Multiple LEDs": "多个 LED",
    "Variable Speed": "可变速度",
    "Error Handling": "错误处理",
    "Example: Blinking Multiple LEDs": "示例：闪烁多个 LED",
    "Check Return Values": "检查返回值",
    "Document Pin Assignments": "记录引脚分配",
    "Code Organization": "代码组织",
    "Use Meaningful Names": "使用有意义的名称",

    # 平台支持
    "RTOS Support": "RTOS 支持",
    "FreeRTOS, Bare": "FreeRTOS，裸机",

    # 其他
    "Using Git Submodule": "使用 Git 子模块",
    "Using Direct Copy": "使用直接复制",
    "Links the Hardware Abstraction Layer": "链接硬件抽象层",
    "Links platform-specific code": "链接平台特定代码",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in SENTENCE_TRANSLATIONS:
        return SENTENCE_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in SENTENCE_TRANSLATIONS.items():
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
