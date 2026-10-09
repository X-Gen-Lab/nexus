#!/usr/bin/env python3
"""
批量翻译工具 - 第2部分：构建系统和开发指南
"""

import re
from pathlib import Path
import polib

BUILD_TRANSLATIONS = {
    # 构建系统标题
    "Build System": "构建系统",
    "Project Organization": "项目组织",
    "Building the Project": "构建项目",
    "Cross-Compilation": "交叉编译",
    "ARM Cortex-M Targets": "ARM Cortex-M 目标",
    "Kconfig Integration": "Kconfig 集成",
    "Generated Header": "生成的头文件",
    "Output Artifacts": "输出文件",
    "Build Options": "构建选项",
    "Troubleshooting": "故障排除",

    # 构建系统描述
    "The Nexus build system is designed to support:": "Nexus 构建系统旨在支持：",
    "Cross-compilation for ARM targets": "ARM 目标的交叉编译",
    "Automated configuration generation via Kconfig": "通过 Kconfig 自动生成配置",
    "Unit testing with GoogleTest": "使用 GoogleTest 进行单元测试",
    "Code coverage analysis": "代码覆盖率分析",
    "Documentation generation": "文档生成",

    # 项目结构
    "The project follows a hierarchical CMake structure:": "项目遵循分层的 CMake 结构：",
    "Root CMakeLists.txt": "根 CMakeLists.txt",

    # CMake 配置
    "The root ``CMakeLists.txt`` file:": "根 ``CMakeLists.txt`` 文件：",
    "Prevents in-source builds": "防止源码内构建",
    "Declares build options": "声明构建选项",
    "Configures compiler flags": "配置编译器标志",
    "Integrates Kconfig configuration system": "集成 Kconfig 配置系统",
    "Includes subdirectories": "包含子目录",

    # 构建选项
    "The following CMake options control the build configuration:": "以下 CMake 选项控制构建配置：",
    "Enable code coverage instrumentation": "启用代码覆盖率检测",
    "Set platform using:": "设置平台：",
    "Set OSAL backend using:": "设置 OSAL 后端：",
    "Set build type using:": "设置构建类型：",

    # OSAL 后端
    "FreeRTOS integration": "FreeRTOS 集成",
    "RT-Thread integration": "RT-Thread 集成",
    "Zephyr RTOS integration": "Zephyr RTOS 集成",

    # 构建类型
    "CMake supports standard build types:": "CMake 支持标准构建类型：",

    # 构建步骤
    "Create build directory": "创建构建目录",
    "Configure": "配置",
    "Build": "构建",
    "With specific options:": "使用特定选项：",

    # 工具链
    "Install ARM GNU Toolchain:": "安装 ARM GNU 工具链：",
    "Verify installation:": "验证安装：",
    "Compiler": "编译器",
    "Linker": "链接器",
    "Archiver": "归档器",
    "Objcopy": "对象复制",
    "Size": "大小",

    # 编译器标志
    "Compiler flags for ARM:": "ARM 编译器标志：",
    "If the toolchain is not in PATH:": "如果工具链不在 PATH 中：",

    # Kconfig 集成
    "The build system integrates with Kconfig for configuration management:": "构建系统与 Kconfig 集成以进行配置管理：",

    # 生成的头文件
    "The ``nexus_config.h`` header contains:": "``nexus_config.h`` 头文件包含：",

    # 构建输出
    "After building, the directory structure:": "构建后的目录结构：",
    "For embedded targets:": "对于嵌入式目标：",

    # 构建技巧
    "Use multiple cores for faster builds:": "使用多核加快构建速度：",
    "Show full compiler commands:": "显示完整的编译器命令：",

    # 清理
    "Clean build artifacts": "清理构建文件",
    "Or delete build directory": "或删除构建目录",

    # 代码覆盖率
    "Enable coverage and generate reports:": "启用覆盖率并生成报告：",

    # 故障排除
    "Solution: Upgrade CMake to version 3.16 or higher.": "解决方案：将 CMake 升级到 3.16 或更高版本。",
    "Solution: Install Python 3.7 or higher.": "解决方案：安装 Python 3.7 或更高版本。",

    # 测试相关
    "Testing": "测试",
    "Unit Testing": "单元测试",
    "Integration Testing": "集成测试",
    "Coverage Reporting": "覆盖率报告",
    "Static Analysis": "静态分析",
    "MISRA C Compliance": "MISRA C 合规性",

    # 测试描述
    "Configure and build with tests enabled::": "启用测试进行配置和构建：",
    "Run with Verbose Output": "使用详细输出运行",
    "Generate Coverage Report": "生成覆盖率报告",
    "Address Sanitizer": "地址消毒器",

    # 测试类型
    "Positive Tests": "正向测试",
    "Negative Tests": "负向测试",
    "Boundary Tests": "边界测试",
    "Integration Tests": "集成测试",
    "Verify correct behavior with valid inputs": "验证有效输入的正确行为",
    "Verify error handling with invalid inputs": "验证无效输入的错误处理",
    "Test edge cases and limits": "测试边界情况和限制",
    "Test component interactions": "测试组件交互",

    # 测试要求
    "Minimum code coverage: **90%**": "最低代码覆盖率：**90%**",
    "All public APIs must have tests": "所有公共 API 必须有测试",
    "All error paths must be tested": "所有错误路径必须经过测试",

    # 测试最佳实践
    "Test Independence": "测试独立性",
    "Clear Assertions": "清晰的断言",
    "Setup/Teardown": "设置/清理",
    "Mock External Dependencies": "模拟外部依赖",
    "Test Edge Cases": "测试边界情况",
    "Keep Tests Fast": "保持测试快速",
    "Avoid Test Duplication": "避免测试重复",
    "Each test should be independent and not rely on other tests": "每个测试应该独立，不依赖其他测试",
    "Use descriptive assertion messages": "使用描述性断言消息",
    "Use fixtures for common setup and cleanup": "使用固件进行通用设置和清理",
    "Use mocks for hardware and OS dependencies": "使用模拟来处理硬件和操作系统依赖",
    "Include boundary conditions and error cases": "包括边界条件和错误情况",
    "Unit tests should run quickly": "单元测试应该快速运行",
    "Use parameterized tests for similar cases": "对类似情况使用参数化测试",

    # CI/CD
    "Parameterized Tests": "参数化测试",
    "Tests run automatically on every PR via GitHub Actions:": "测试通过 GitHub Actions 在每个 PR 上自动运行：",
    "Coverage reporting": "覆盖率报告",
    "Memory sanitizer checks": "内存消毒器检查",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in BUILD_TRANSLATIONS:
        return BUILD_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in BUILD_TRANSLATIONS.items():
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
