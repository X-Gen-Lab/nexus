#!/usr/bin/env python3
"""
批量翻译工具 - 第3部分：API 文档和配置说明
"""

import re
from pathlib import Path
import polib

API_CONFIG_TRANSLATIONS = {
    # API 文档标题
    "API Reference": "API 参考",
    "Function Reference": "函数参考",
    "Data Structures": "数据结构",
    "Enumerations": "枚举",
    "Type Definitions": "类型定义",
    "Macros": "宏",
    "Constants": "常量",

    # 函数说明
    "Function Description": "函数描述",
    "Parameters": "参数",
    "Return Value": "返回值",
    "Return Values": "返回值",
    "Returns": "返回",
    "Example Usage": "使用示例",
    "Usage Example": "使用示例",
    "Notes": "注意事项",
    "Remarks": "备注",
    "See Also": "另请参阅",

    # 参数说明
    "Input parameter": "输入参数",
    "Output parameter": "输出参数",
    "Input/output parameter": "输入/输出参数",
    "Pointer to": "指向...的指针",
    "Pointer to the": "指向...的指针",
    "Address of": "...的地址",
    "Handle to": "...的句柄",
    "Configuration structure": "配置结构",
    "Configuration pointer": "配置指针",

    # 返回值说明
    "On success": "成功时",
    "On failure": "失败时",
    "On error": "出错时",
    "Returns zero on success": "成功时返回零",
    "Returns non-zero on error": "出错时返回非零值",
    "Returns NULL on error": "出错时返回 NULL",
    "Returns pointer on success": "成功时返回指针",

    # 配置选项
    "Configuration Options": "配置选项",
    "Configuration": "配置",
    "Default Configuration": "默认配置",
    "Custom Configuration": "自定义配置",
    "Platform Configuration": "平台配置",
    "Build Configuration": "构建配置",

    # Kconfig 相关
    "Kconfig Options": "Kconfig 选项",
    "Kconfig Reference": "Kconfig 参考",
    "Configuration System": "配置系统",
    "Menu Configuration": "菜单配置",

    # 配置说明
    "This option enables": "此选项启用",
    "This option disables": "此选项禁用",
    "This option controls": "此选项控制",
    "This option sets": "此选项设置",
    "Enable this option to": "启用此选项以",
    "Disable this option to": "禁用此选项以",
    "Set this option to": "将此选项设置为",

    # 配置类型
    "Type: bool": "类型：布尔",
    "Type: int": "类型：整数",
    "Type: string": "类型：字符串",
    "Type: hex": "类型：十六进制",
    "Default: y": "默认：是",
    "Default: n": "默认：否",
    "Default value": "默认值",
    "Valid range": "有效范围",
    "Possible values": "可能的值",

    # 依赖关系
    "Depends on": "依赖于",
    "Requires": "需要",
    "Optional": "可选",
    "Required": "必需",
    "Recommended": "推荐",
    "Not recommended": "不推荐",

    # 平台相关
    "Platform Support": "平台支持",
    "Supported Platforms": "支持的平台",
    "Platform-Specific": "平台特定",
    "All platforms": "所有平台",
    "ARM Cortex-M only": "仅 ARM Cortex-M",
    "STM32 only": "仅 STM32",

    # 特性说明
    "Features": "特性",
    "Key Features": "主要特性",
    "Main Features": "主要特性",
    "Supported Features": "支持的特性",
    "Available Features": "可用特性",
    "Feature List": "特性列表",

    # 限制说明
    "Limitations": "限制",
    "Known Limitations": "已知限制",
    "Restrictions": "限制",
    "Constraints": "约束",

    # 兼容性
    "Compatibility": "兼容性",
    "Backward Compatibility": "向后兼容性",
    "Forward Compatibility": "向前兼容性",
    "Compatible with": "兼容",
    "Not compatible with": "不兼容",

    # 性能
    "Performance": "性能",
    "Performance Considerations": "性能考虑",
    "Optimization": "优化",
    "Memory Usage": "内存使用",
    "CPU Usage": "CPU 使用",
    "Execution Time": "执行时间",

    # 安全性
    "Security": "安全性",
    "Security Considerations": "安全考虑",
    "Thread Safety": "线程安全",
    "Interrupt Safety": "中断安全",
    "Reentrant": "可重入",
    "Not reentrant": "不可重入",

    # 初始化和配置
    "Initialization": "初始化",
    "Initialization Sequence": "初始化序列",
    "Configuration Steps": "配置步骤",
    "Setup": "设置",
    "Cleanup": "清理",
    "Deinitialization": "反初始化",

    # 错误处理
    "Error Handling": "错误处理",
    "Error Codes": "错误代码",
    "Error Messages": "错误消息",
    "Error Recovery": "错误恢复",
    "Failure Modes": "失败模式",

    # 调试
    "Debugging": "调试",
    "Debug Output": "调试输出",
    "Debug Mode": "调试模式",
    "Verbose Mode": "详细模式",
    "Logging": "日志记录",
    "Log Levels": "日志级别",

    # 示例
    "Example": "示例",
    "Examples": "示例",
    "Code Example": "代码示例",
    "Usage Examples": "使用示例",
    "Sample Code": "示例代码",
    "Demo Application": "演示应用程序",

    # 文档链接
    "For more information, see": "更多信息请参阅",
    "Refer to the": "请参阅",
    "See the": "参见",
    "Consult the": "查阅",
    "Check the": "检查",

    # 版本信息
    "Version": "版本",
    "Since version": "自版本",
    "Added in version": "在版本中添加",
    "Deprecated in version": "在版本中弃用",
    "Removed in version": "在版本中移除",
    "Changed in version": "在版本中更改",

    # 作者和许可
    "Author": "作者",
    "Authors": "作者",
    "Contributors": "贡献者",
    "License": "许可证",
    "Copyright": "版权",

    # 通用描述
    "Description": "描述",
    "Overview": "概述",
    "Summary": "摘要",
    "Details": "详细信息",
    "Implementation": "实现",
    "Behavior": "行为",
    "Operation": "操作",

    # 状态和标志
    "Status": "状态",
    "State": "状态",
    "Flags": "标志",
    "Options": "选项",
    "Settings": "设置",
    "Properties": "属性",
    "Attributes": "属性",

    # 数据类型
    "Data Type": "数据类型",
    "Data Types": "数据类型",
    "Structure": "结构体",
    "Union": "联合体",
    "Enumeration": "枚举",
    "Typedef": "类型定义",

    # 内存相关
    "Memory": "内存",
    "Memory Allocation": "内存分配",
    "Memory Management": "内存管理",
    "Buffer": "缓冲区",
    "Buffer Size": "缓冲区大小",
    "Stack": "栈",
    "Heap": "堆",

    # 时间相关
    "Timeout": "超时",
    "Delay": "延迟",
    "Duration": "持续时间",
    "Period": "周期",
    "Frequency": "频率",
    "Interval": "间隔",

    # 优先级
    "Priority": "优先级",
    "High Priority": "高优先级",
    "Low Priority": "低优先级",
    "Normal Priority": "正常优先级",

    # 模式
    "Mode": "模式",
    "Operating Mode": "操作模式",
    "Power Mode": "电源模式",
    "Sleep Mode": "睡眠模式",
    "Active Mode": "活动模式",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in API_CONFIG_TRANSLATIONS:
        return API_CONFIG_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in API_CONFIG_TRANSLATIONS.items():
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
