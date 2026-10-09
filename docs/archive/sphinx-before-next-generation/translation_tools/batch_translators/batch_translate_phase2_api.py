#!/usr/bin/env python3
"""
批量翻译工具 - 第2阶段：API 文档
处理 API 参考文档相关的完整句子
"""

import re
from pathlib import Path
import polib

# API 文档相关翻译
API_TRANSLATIONS = {
    # API 概述
    "This section provides comprehensive API documentation for all Nexus modules. The API documentation is automatically generated from source code using Doxygen and integrated via Breathe.": "本节提供所有 Nexus 模块的完整 API 文档。API 文档使用 Doxygen 从源代码自动生成，并通过 Breathe 集成。",
    "Core Layers": "核心层",

    # OSAL
    "Complete OSAL API reference including tasks, mutexes, semaphores, queues, and timers.": "完整的 OSAL API 参考，包括任务、互斥锁、信号量、队列和定时器。",

    # Init Framework
    "Automatic initialization system using linker sections.": "使用链接器段的自动初始化系统。",
    "Init Framework API reference with initialization levels and export macros.": "Init 框架 API 参考，包含初始化级别和导出宏。",

    # Log Framework
    "Flexible logging system with multiple backends and async support.": "灵活的日志系统，支持多个后端和异步支持。",
    "Log Framework API reference including log levels, backends, and configuration.": "Log 框架 API 参考，包括日志级别、后端和配置。",

    # Shell Framework
    "Interactive command-line interface with autocomplete and history.": "交互式命令行界面，支持自动完成和历史记录。",
    "Shell Framework API reference including command registration and line editing.": "Shell 框架 API 参考，包括命令注册和行编辑。",

    # Config Manager
    "Config Manager": "配置管理器",
    "Configuration management system with multiple storage backends.": "配置管理系统，支持多个存储后端。",
    "Config Manager API reference including data types and backends.": "配置管理器 API 参考，包括数据类型和后端。",

    # Kconfig Tools
    "Kconfig Tools": "Kconfig 工具",
    "Tools for managing Kconfig-based configuration.": "用于管理基于 Kconfig 的配置的工具。",
    "Kconfig tools API reference for configuration generation and validation.": "Kconfig 工具 API 参考，用于配置生成和验证。",

    # API 分类
    "The Nexus API is organized into the following categories:": "Nexus API 组织为以下类别：",
    "**HAL** - Hardware abstraction for peripherals": "**HAL** - 外设的硬件抽象",
    "**OSAL** - Operating system abstraction": "**OSAL** - 操作系统抽象",
    "**Init** - Automatic initialization system": "**Init** - 自动初始化系统",
    "**Log** - Logging framework": "**Log** - 日志框架",
    "**Shell** - Command-line interface": "**Shell** - 命令行界面",
    "**Config** - Configuration management": "**Config** - 配置管理",
    "**Kconfig Tools** - Configuration management tools": "**Kconfig Tools** - 配置管理工具",

    # API 文档标准
    "API Documentation Standards": "API 文档标准",
    "All API functions follow these documentation standards:": "所有 API 函数都遵循以下文档标准：",
    "**Function signatures** with parameter types and return values": "**函数签名**，包含参数类型和返回值",
    "**Parameter descriptions** for all inputs and outputs": "所有输入和输出的**参数描述**",
    "**Return value descriptions** including error codes": "**返回值描述**，包括错误代码",
    "**Usage examples** for common patterns": "常见模式的**使用示例**",
    "**Thread-safety information** for concurrent usage": "并发使用的**线程安全信息**",

    # 通用 API 术语
    "Function Reference": "函数参考",
    "Type Definitions": "类型定义",
    "Enumerations": "枚举",
    "Structures": "结构体",
    "Macros": "宏",
    "Constants": "常量",
    "Global Variables": "全局变量",

    # 参数相关
    "Parameters": "参数",
    "Param user_data": "参数 user_data",
    "Param info": "参数 info",
    "Param config": "参数 config",
    "Param handle": "参数 handle",
    "Param callback": "参数 callback",
    "Param context": "参数 context",
    "Input Parameters": "输入参数",
    "Output Parameters": "输出参数",
    "In/Out Parameters": "输入/输出参数",

    # 返回值
    "Return Values": "返回值",
    "Return Value": "返回值",
    "Returns": "返回",
    "On success": "成功时",
    "On failure": "失败时",
    "On error": "错误时",
    "Returns 0 on success": "成功时返回 0",
    "Returns negative error code on failure": "失败时返回负错误代码",

    # 成员和字段
    "Public Members": "公共成员",
    "Private Members": "私有成员",
    "Member Functions": "成员函数",
    "Member Variables": "成员变量",
    "Fields": "字段",

    # 函数类型
    "Initialization Functions": "初始化函数",
    "Configuration Functions": "配置函数",
    "Control Functions": "控制函数",
    "Data Transfer Functions": "数据传输函数",
    "Utility Functions": "实用函数",
    "Callback Functions": "回调函数",
    "Helper Functions": "辅助函数",

    # 线程安全
    "Thread Safety": "线程安全",
    "Thread-safe": "线程安全",
    "Not thread-safe": "非线程安全",
    "Reentrant": "可重入",
    "Non-reentrant": "不可重入",
    "Requires locking": "需要锁定",
    "Atomic operation": "原子操作",

    # 错误处理
    "Error Handling": "错误处理",
    "Error Codes": "错误代码",
    "Error Messages": "错误消息",
    "Error Conditions": "错误条件",
    "Error Recovery": "错误恢复",

    # 使用说明
    "Usage": "用法",
    "Usage Example": "使用示例",
    "Example Usage": "示例用法",
    "Basic Usage": "基本用法",
    "Advanced Usage": "高级用法",
    "Common Use Cases": "常见用例",

    # 注意事项
    "Notes": "注意事项",
    "Remarks": "备注",
    "Warnings": "警告",
    "Preconditions": "前置条件",
    "Postconditions": "后置条件",
    "Side Effects": "副作用",

    # 版本信息
    "Since": "起始版本",
    "Deprecated": "已弃用",
    "Version": "版本",
    "Added in version": "在版本中添加",
    "Removed in version": "在版本中移除",

    # 相关内容
    "See Also": "另请参阅",
    "Related Functions": "相关函数",
    "Related Types": "相关类型",
    "Related Modules": "相关模块",
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
            if entry.msgid in API_TRANSLATIONS:
                entry.msgstr = API_TRANSLATIONS[entry.msgid]
                translated_count += 1
            # 去除首尾空白后匹配
            elif entry.msgid.strip() in API_TRANSLATIONS:
                entry.msgstr = API_TRANSLATIONS[entry.msgid.strip()]
                translated_count += 1

    if translated_count > 0:
        po.save(po_path)

    return translated_count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 重点处理 API 和参考文档文件
    target_files = [
        'api/index.po',
        'api/hal.po',
        'api/osal.po',
        'api/init.po',
        'api/log.po',
        'api/shell.po',
        'api/config.po',
        'api/kconfig_tools.po',
        'reference/api_index.po',
        'reference/error_codes.po',
    ]

    print(f"处理 {len(target_files)} 个 API 文档文件...")

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
