#!/usr/bin/env python3
"""
批量翻译工具 - 第4部分：参数、配置项和短语
"""

import re
from pathlib import Path
import polib

PARAM_CONFIG_TRANSLATIONS = {
    # 参数名称
    "Param type": "参数 type",
    "Param old_value": "参数 old_value",
    "Param new_value": "参数 new_value",
    "Param key": "参数 key",
    "Param value": "参数 value",
    "Param data": "参数 data",
    "Param size": "参数 size",
    "Param buffer": "参数 buffer",
    "Param length": "参数 length",
    "Param config": "参数 config",
    "Param handle": "参数 handle",
    "Param callback": "参数 callback",
    "Param context": "参数 context",
    "Param timeout": "参数 timeout",
    "Param flags": "参数 flags",
    "Param mode": "参数 mode",
    "Param priority": "参数 priority",
    "Param name": "参数 name",
    "Param id": "参数 id",
    "Param index": "参数 index",
    "Param count": "参数 count",
    "Param offset": "参数 offset",
    "Param address": "参数 address",
    "Param port": "参数 port",
    "Param pin": "参数 pin",
    "Param level": "参数 level",
    "Param state": "参数 state",
    "Param status": "参数 status",
    "Param error": "参数 error",
    "Param result": "参数 result",

    # 后端类型
    "Flash backend": "Flash 后端",
    "RAM backend": "RAM 后端",
    "Thread-safe with internal locking": "使用内部锁实现线程安全",
    "Not thread-safe, requires external mutex": "非线程安全，需要外部互斥锁",

    # API 参考
    "Complete HAL API reference including GPIO, UART, SPI, I2C, Timer, ADC, and more.": "完整的 HAL API 参考，包括 GPIO、UART、SPI、I2C、定时器、ADC 等。",

    # 配置管理器
    "The Config Manager is **thread-safe** when using the Flash backend with proper locking.": "使用 Flash 后端并正确加锁时，配置管理器是**线程安全**的。",

    # API 文档
    "This section provides comprehensive API documentation for all Nexus modules. The documentation is auto-generated from source code comments using Doxygen.": "本节提供所有 Nexus 模块的全面 API 文档。文档使用 Doxygen 从源代码注释自动生成。",

    # 初始化框架
    "The Nexus Init Framework provides a compile-time automatic initialization system for modules and drivers.": "Nexus 初始化框架为模块和驱动程序提供编译时自动初始化系统。",

    # 工具名称
    "Namespaces": "命名空间",
    "nexus_config.py": "nexus_config.py",
    "Jenkins": "Jenkins",

    # 配置工具
    "Compare configuration files": "比较配置文件",
    "Display configuration information": "显示配置信息",

    # Git 提交类型
    "New feature": "新特性",
    "Bug fix": "错误修复",
    "Documentation": "文档",
    "Code refactoring": "代码重构",
    "Performance improvement": "性能改进",
    "Test": "测试",
    "Build system": "构建系统",
    "CI/CD": "CI/CD",
    "Revert": "回退",

    # 平台常量
    "STM32 platform identifier": "STM32 平台标识符",
    "Enable UART peripheral": "启用 UART 外设",
    "STM32 chip name": "STM32 芯片名称",
    "UART1 instance": "UART1 实例",
    "UART1 baud rate": "UART1 波特率",
    "UART1 DMA mode": "UART1 DMA 模式",
    "FreeRTOS backend": "FreeRTOS 后端",
    "OSAL tick rate": "OSAL 时钟频率",
    "OSAL heap size": "OSAL 堆大小",

    # 配置选项描述
    "Enable this feature": "启用此特性",
    "Disable this feature": "禁用此特性",
    "Set the value": "设置值",
    "Configure the option": "配置选项",
    "Select the backend": "选择后端",
    "Choose the platform": "选择平台",
    "Specify the size": "指定大小",
    "Define the timeout": "定义超时",
    "Set the priority": "设置优先级",
    "Configure the mode": "配置模式",

    # 短语
    "by default": "默认情况下",
    "if enabled": "如果启用",
    "if disabled": "如果禁用",
    "when set": "设置时",
    "when cleared": "清除时",
    "at runtime": "运行时",
    "at compile time": "编译时",
    "during initialization": "初始化期间",
    "before use": "使用前",
    "after completion": "完成后",

    # 状态描述
    "enabled by default": "默认启用",
    "disabled by default": "默认禁用",
    "required for": "...所需",
    "optional for": "...可选",
    "recommended for": "推荐用于",
    "not recommended for": "不推荐用于",
    "compatible with": "兼容",
    "incompatible with": "不兼容",

    # 配置依赖
    "depends on": "依赖于",
    "requires": "需要",
    "conflicts with": "与...冲突",
    "implies": "意味着",
    "selects": "选择",

    # 范围和限制
    "valid range": "有效范围",
    "minimum value": "最小值",
    "maximum value": "最大值",
    "default value": "默认值",
    "recommended value": "推荐值",

    # 单位
    "in bytes": "字节",
    "in kilobytes": "千字节",
    "in milliseconds": "毫秒",
    "in seconds": "秒",
    "in hertz": "赫兹",
    "in percentage": "百分比",

    # 操作描述
    "read-only": "只读",
    "write-only": "只写",
    "read-write": "读写",
    "volatile": "易失性",
    "persistent": "持久性",
    "cached": "缓存",
    "buffered": "缓冲",

    # 错误处理
    "on success": "成功时",
    "on failure": "失败时",
    "on error": "出错时",
    "on timeout": "超时时",
    "on completion": "完成时",
    "on interrupt": "中断时",

    # 回调和事件
    "callback function": "回调函数",
    "event handler": "事件处理程序",
    "interrupt handler": "中断处理程序",
    "error handler": "错误处理程序",
    "completion handler": "完成处理程序",

    # 同步原语
    "mutex lock": "互斥锁",
    "semaphore": "信号量",
    "event flag": "事件标志",
    "message queue": "消息队列",
    "mailbox": "邮箱",
    "critical section": "临界区",

    # 内存操作
    "allocate memory": "分配内存",
    "free memory": "释放内存",
    "copy memory": "复制内存",
    "clear memory": "清除内存",
    "set memory": "设置内存",
    "compare memory": "比较内存",

    # 字符串操作
    "copy string": "复制字符串",
    "compare string": "比较字符串",
    "concatenate string": "连接字符串",
    "format string": "格式化字符串",
    "parse string": "解析字符串",

    # 数学操作
    "add": "加",
    "subtract": "减",
    "multiply": "乘",
    "divide": "除",
    "modulo": "取模",
    "increment": "递增",
    "decrement": "递减",

    # 位操作
    "set bit": "设置位",
    "clear bit": "清除位",
    "toggle bit": "切换位",
    "test bit": "测试位",
    "shift left": "左移",
    "shift right": "右移",
    "rotate left": "循环左移",
    "rotate right": "循环右移",

    # 逻辑操作
    "logical AND": "逻辑与",
    "logical OR": "逻辑或",
    "logical NOT": "逻辑非",
    "logical XOR": "逻辑异或",
    "bitwise AND": "按位与",
    "bitwise OR": "按位或",
    "bitwise NOT": "按位非",
    "bitwise XOR": "按位异或",

    # 比较操作
    "equal to": "等于",
    "not equal to": "不等于",
    "greater than": "大于",
    "less than": "小于",
    "greater than or equal": "大于或等于",
    "less than or equal": "小于或等于",

    # 控制流
    "if condition": "如果条件",
    "else clause": "否则子句",
    "while loop": "while 循环",
    "for loop": "for 循环",
    "do-while loop": "do-while 循环",
    "switch statement": "switch 语句",
    "case label": "case 标签",
    "default case": "默认情况",
    "break statement": "break 语句",
    "continue statement": "continue 语句",
    "return statement": "return 语句",
    "goto statement": "goto 语句",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in PARAM_CONFIG_TRANSLATIONS:
        return PARAM_CONFIG_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in PARAM_CONFIG_TRANSLATIONS.items():
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
