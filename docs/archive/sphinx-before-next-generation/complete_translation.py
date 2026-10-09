#!/usr/bin/env python3
"""
完整翻译工具 - 使用扩展词典和智能翻译规则
"""

import re
from pathlib import Path
import polib

# 扩展翻译词典 - 涵盖所有常见技术术语和短语
TRANSLATION_DICT = {
    # === 文档结构 ===
    "Overview": "概述",
    "Introduction": "简介",
    "Prerequisites": "前置条件",
    "Requirements": "要求",
    "Installation": "安装",
    "Configuration": "配置",
    "Usage": "用法",
    "Examples": "示例",
    "Example": "示例",
    "Tutorial": "教程",
    "Tutorials": "教程",
    "Guide": "指南",
    "Reference": "参考",
    "API Reference": "API 参考",
    "User Guide": "用户指南",
    "Developer Guide": "开发者指南",
    "Getting Started": "快速开始",
    "Quick Start": "快速开始",
    "Features": "特性",
    "Architecture": "架构",
    "Design": "设计",
    "Implementation": "实现",
    "Testing": "测试",
    "Debugging": "调试",
    "Troubleshooting": "故障排除",
    "FAQ": "常见问题",
    "Frequently Asked Questions": "常见问题",
    "Glossary": "术语表",
    "Index": "索引",
    "Contents": "目录",
    "Table of Contents": "目录",
    "Summary": "总结",
    "Conclusion": "结论",
    "Next Steps": "下一步",
    "Related Topics": "相关主题",
    "Further Reading": "延伸阅读",

    # === 通用术语 ===
    "Note": "注意",
    "Warning": "警告",
    "Important": "重要",
    "Tip": "提示",
    "Caution": "警告",
    "Danger": "危险",
    "See also": "另请参阅",
    "See": "参见",
    "Description": "描述",
    "Parameters": "参数",
    "Parameter": "参数",
    "Returns": "返回值",
    "Return value": "返回值",
    "Return": "返回",
    "Error": "错误",
    "Errors": "错误",
    "Success": "成功",
    "Failed": "失败",
    "Failure": "失败",
    "Optional": "可选",
    "Required": "必需",
    "Default": "默认",
    "Type": "类型",
    "Value": "值",
    "Values": "值",
    "Name": "名称",
    "Version": "版本",
    "Author": "作者",
    "License": "许可证",
    "Copyright": "版权",
    "Status": "状态",
    "State": "状态",

    # === 操作动词 ===
    "Build": "构建",
    "Compile": "编译",
    "Run": "运行",
    "Execute": "执行",
    "Install": "安装",
    "Configure": "配置",
    "Initialize": "初始化",
    "Init": "初始化",
    "Create": "创建",
    "Delete": "删除",
    "Remove": "移除",
    "Update": "更新",
    "Modify": "修改",
    "Change": "更改",
    "Add": "添加",
    "Enable": "启用",
    "Disable": "禁用",
    "Start": "启动",
    "Stop": "停止",
    "Restart": "重启",
    "Reset": "重置",
    "Clear": "清除",
    "Clean": "清理",
    "Save": "保存",
    "Load": "加载",
    "Read": "读取",
    "Write": "写入",
    "Open": "打开",
    "Close": "关闭",
    "Connect": "连接",
    "Disconnect": "断开",
    "Send": "发送",
    "Receive": "接收",
    "Transmit": "传输",
    "Transfer": "传输",
    "Set": "设置",
    "Get": "获取",
    "Check": "检查",
    "Verify": "验证",
    "Validate": "验证",
    "Test": "测试",
    "Debug": "调试",

    # === 状态描述 ===
    "Ready": "就绪",
    "Busy": "忙碌",
    "Idle": "空闲",
    "Active": "活动",
    "Inactive": "非活动",
    "Enabled": "已启用",
    "Disabled": "已禁用",
    "Running": "运行中",
    "Stopped": "已停止",
    "Pending": "待处理",
    "Complete": "完成",
    "Completed": "已完成",
    "Incomplete": "未完成",
    "Valid": "有效",
    "Invalid": "无效",
    "Available": "可用",
    "Unavailable": "不可用",
    "Online": "在线",
    "Offline": "离线",
    "Connected": "已连接",
    "Disconnected": "已断开",

    # === 硬件和平台 ===
    "Hardware": "硬件",
    "Software": "软件",
    "Firmware": "固件",
    "Platform": "平台",
    "Device": "设备",
    "Peripheral": "外设",
    "Peripherals": "外设",
    "Microcontroller": "微控制器",
    "MCU": "MCU",
    "Processor": "处理器",
    "CPU": "CPU",
    "Memory": "内存",
    "Flash": "Flash",
    "RAM": "RAM",
    "ROM": "ROM",
    "Register": "寄存器",
    "Registers": "寄存器",
    "Pin": "引脚",
    "Pins": "引脚",
    "Port": "端口",
    "Ports": "端口",
    "Bus": "总线",
    "Clock": "时钟",
    "Interrupt": "中断",
    "Interrupts": "中断",

    # === 编程概念 ===
    "Function": "函数",
    "Functions": "函数",
    "Method": "方法",
    "Methods": "方法",
    "Class": "类",
    "Classes": "类",
    "Object": "对象",
    "Objects": "对象",
    "Interface": "接口",
    "Interfaces": "接口",
    "Structure": "结构体",
    "Struct": "结构体",
    "Union": "联合体",
    "Enum": "枚举",
    "Enumeration": "枚举",
    "Typedef": "类型定义",
    "Macro": "宏",
    "Macros": "宏",
    "Constant": "常量",
    "Constants": "常量",
    "Variable": "变量",
    "Variables": "变量",
    "Pointer": "指针",
    "Pointers": "指针",
    "Array": "数组",
    "Arrays": "数组",
    "Buffer": "缓冲区",
    "Buffers": "缓冲区",
    "String": "字符串",
    "Strings": "字符串",
    "Callback": "回调",
    "Callbacks": "回调",
    "Handler": "处理程序",
    "Handlers": "处理程序",
    "Event": "事件",
    "Events": "事件",
    "Task": "任务",
    "Tasks": "任务",
    "Thread": "线程",
    "Threads": "线程",
    "Process": "进程",
    "Processes": "进程",
    "Mutex": "互斥锁",
    "Semaphore": "信号量",
    "Queue": "队列",
    "Queues": "队列",
    "Stack": "栈",
    "Heap": "堆",

    # === 工具和构建 ===
    "Build System": "构建系统",
    "Toolchain": "工具链",
    "Compiler": "编译器",
    "Linker": "链接器",
    "Debugger": "调试器",
    "IDE": "集成开发环境",
    "Editor": "编辑器",
    "Terminal": "终端",
    "Console": "控制台",
    "Shell": "Shell",
    "Command": "命令",
    "Commands": "命令",
    "Script": "脚本",
    "Scripts": "脚本",
    "Tool": "工具",
    "Tools": "工具",
    "Utility": "实用工具",
    "Utilities": "实用工具",

    # === 文件和目录 ===
    "File": "文件",
    "Files": "文件",
    "Directory": "目录",
    "Directories": "目录",
    "Folder": "文件夹",
    "Folders": "文件夹",
    "Path": "路径",
    "Paths": "路径",
    "Source": "源代码",
    "Source code": "源代码",
    "Header": "头文件",
    "Headers": "头文件",
    "Library": "库",
    "Libraries": "库",
    "Module": "模块",
    "Modules": "模块",
    "Package": "包",
    "Packages": "包",
    "Component": "组件",
    "Components": "组件",

    # === 配置和选项 ===
    "Option": "选项",
    "Options": "选项",
    "Setting": "设置",
    "Settings": "设置",
    "Config": "配置",
    "Property": "属性",
    "Properties": "属性",
    "Attribute": "属性",
    "Attributes": "属性",
    "Flag": "标志",
    "Flags": "标志",
    "Mode": "模式",
    "Modes": "模式",
    "Level": "级别",
    "Levels": "级别",
    "Priority": "优先级",
    "Priorities": "优先级",

    # === 数据和通信 ===
    "Data": "数据",
    "Information": "信息",
    "Message": "消息",
    "Messages": "消息",
    "Packet": "数据包",
    "Packets": "数据包",
    "Frame": "帧",
    "Frames": "帧",
    "Byte": "字节",
    "Bytes": "字节",
    "Bit": "位",
    "Bits": "位",
    "Signal": "信号",
    "Signals": "信号",
    "Protocol": "协议",
    "Protocols": "协议",
    "Format": "格式",
    "Formats": "格式",
    "Encoding": "编码",
    "Decoding": "解码",

    # === 时间和定时 ===
    "Time": "时间",
    "Timeout": "超时",
    "Delay": "延迟",
    "Duration": "持续时间",
    "Period": "周期",
    "Frequency": "频率",
    "Rate": "速率",
    "Speed": "速度",
    "Timer": "定时器",
    "Timers": "定时器",
    "Timestamp": "时间戳",

    # === 错误和调试 ===
    "Bug": "错误",
    "Bugs": "错误",
    "Issue": "问题",
    "Issues": "问题",
    "Problem": "问题",
    "Problems": "问题",
    "Exception": "异常",
    "Exceptions": "异常",
    "Assert": "断言",
    "Assertion": "断言",
    "Log": "日志",
    "Logs": "日志",
    "Logging": "日志记录",
    "Trace": "跟踪",
    "Tracing": "跟踪",

    # === 性能和优化 ===
    "Performance": "性能",
    "Optimization": "优化",
    "Optimizations": "优化",
    "Efficiency": "效率",
    "Overhead": "开销",
    "Latency": "延迟",
    "Throughput": "吞吐量",
    "Bandwidth": "带宽",
    "Resource": "资源",
    "Resources": "资源",
    "Usage": "使用情况",

    # === 版本和发布 ===
    "Release": "发布",
    "Releases": "发布",
    "Stable": "稳定版",
    "Beta": "测试版",
    "Alpha": "Alpha 版",
    "Development": "开发版",
    "Deprecated": "已弃用",
    "Legacy": "旧版",
    "Migration": "迁移",
    "Upgrade": "升级",
    "Downgrade": "降级",

    # === 文档特定 ===
    "This section": "本节",
    "This chapter": "本章",
    "This guide": "本指南",
    "This tutorial": "本教程",
    "This document": "本文档",
    "For more information": "更多信息",
    "For details": "详细信息",
    "As shown": "如图所示",
    "As follows": "如下所示",
    "The following": "以下",
    "In this example": "在此示例中",
    "For example": "例如",
    "Such as": "例如",
    "Including": "包括",
    "Excluding": "不包括",
    "Before": "之前",
    "After": "之后",
    "During": "期间",
    "When": "当",
    "If": "如果",
    "Unless": "除非",
    "Until": "直到",
    "While": "当",
    "First": "首先",
    "Second": "其次",
    "Third": "第三",
    "Finally": "最后",
    "Additionally": "此外",
    "However": "然而",
    "Therefore": "因此",
    "Otherwise": "否则",
    "Meanwhile": "同时",
    "Currently": "当前",
    "Previously": "之前",
    "Now": "现在",
    "Later": "稍后",
    "Soon": "即将",
    "Always": "始终",
    "Never": "从不",
    "Sometimes": "有时",
    "Usually": "通常",
    "Often": "经常",
    "Rarely": "很少",
    "Automatically": "自动",
    "Manually": "手动",
    "Optionally": "可选地",
    "Typically": "通常",
    "Generally": "一般",
    "Specifically": "具体地",
    "Especially": "特别是",
    "Particularly": "尤其是",
    "Mainly": "主要",
    "Primarily": "主要",
    "Mostly": "大多数",
    "Partially": "部分",
    "Completely": "完全",
    "Fully": "完全",
    "Entirely": "完全",
    "Exactly": "确切地",
    "Approximately": "大约",
    "About": "关于",
    "Roughly": "大致",
    "Nearly": "几乎",
    "Almost": "几乎",
    "Quite": "相当",
    "Very": "非常",
    "Extremely": "极其",
    "Highly": "高度",
    "Significantly": "显著",
    "Slightly": "略微",
    "Somewhat": "有点",
    "Rather": "相当",
    "Fairly": "相当",
    "Relatively": "相对",
    "Absolutely": "绝对",
    "Definitely": "肯定",
    "Certainly": "当然",
    "Probably": "可能",
    "Possibly": "可能",
    "Perhaps": "也许",
    "Maybe": "也许",
    "Likely": "可能",
    "Unlikely": "不太可能",

    # === 常见短语 ===
    "by default": "默认情况下",
    "in general": "一般来说",
    "in particular": "特别是",
    "in addition": "此外",
    "in other words": "换句话说",
    "for instance": "例如",
    "as well": "也",
    "as well as": "以及",
    "such as": "例如",
    "and so on": "等等",
    "etc.": "等",
    "i.e.": "即",
    "e.g.": "例如",
    "vs.": "对比",
    "versus": "对比",
    "compared to": "与...相比",
    "according to": "根据",
    "depending on": "取决于",
    "based on": "基于",
    "related to": "与...相关",
    "similar to": "类似于",
    "different from": "不同于",
    "same as": "与...相同",
    "equal to": "等于",
    "greater than": "大于",
    "less than": "小于",
    "at least": "至少",
    "at most": "最多",
    "up to": "最多",
    "more than": "超过",
    "no more than": "不超过",
    "no less than": "不少于",
    "between": "之间",
    "within": "在...之内",
    "outside": "在...之外",
    "inside": "在...内部",
    "above": "在...之上",
    "below": "在...之下",
    "over": "超过",
    "under": "在...之下",
    "through": "通过",
    "via": "通过",
    "using": "使用",
    "with": "使用",
    "without": "不使用",
    "from": "从",
    "to": "到",
    "into": "进入",
    "onto": "到...上",
    "off": "关闭",
    "on": "开启",
    "out": "输出",
    "in": "输入",
}

# 句子翻译模式
SENTENCE_PATTERNS = [
    # 简单句式
    (r'^This (.*) provides (.*)$', r'此\1提供\2'),
    (r'^The (.*) is (.*)$', r'\1是\2'),
    (r'^You can (.*)$', r'您可以\1'),
    (r'^To (.*), (.*)$', r'要\1，\2'),
    (r'^When (.*), (.*)$', r'当\1时，\2'),
    (r'^If (.*), (.*)$', r'如果\1，\2'),
    (r'^Before (.*), (.*)$', r'在\1之前，\2'),
    (r'^After (.*), (.*)$', r'在\1之后，\2'),
]

def should_not_translate(msgid):
    """判断是否不需要翻译"""
    if not msgid or not msgid.strip():
        return True

    msgid = msgid.strip()

    # 代码块
    if msgid.startswith('``') and msgid.endswith('``'):
        return True

    # 纯数字
    if msgid.isdigit():
        return True

    # 全大写常量
    if re.match(r'^[A-Z_][A-Z0-9_]*$', msgid):
        return True

    # Shell 命令
    if msgid.startswith('$') or msgid.startswith('./') or msgid.startswith('python '):
        return True

    # URL
    if msgid.startswith('http://') or msgid.startswith('https://'):
        return True

    # RST 指令
    if msgid.startswith('..') or msgid.startswith(':'):
        return True

    # 文件路径
    if '/' in msgid and not ' ' in msgid:
        return True

    # 包含大量代码符号
    code_chars = sum(1 for c in msgid if c in '(){}[]<>=;,.')
    if code_chars > len(msgid) * 0.3:
        return True

    return False

def translate_text(msgid):
    """翻译文本"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 不需要翻译
    if should_not_translate(msgid):
        return msgid

    # 直接匹配词典
    if msgid in TRANSLATION_DICT:
        return TRANSLATION_DICT[msgid]

    # 不区分大小写匹配
    msgid_lower = msgid.lower()
    for key, value in TRANSLATION_DICT.items():
        if key.lower() == msgid_lower:
            # 保持原始大小写风格
            if msgid.isupper():
                return value.upper()
            elif msgid[0].isupper():
                return value[0].upper() + value[1:] if len(value) > 1 else value.upper()
            return value

    # 尝试句子模式匹配
    for pattern, replacement in SENTENCE_PATTERNS:
        match = re.match(pattern, msgid, re.IGNORECASE)
        if match:
            # 这里需要更复杂的处理，暂时跳过
            pass

    # 无法翻译，返回空字符串
    return ""

def process_po_file(po_path, auto_translate=True):
    """处理 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        print(f"错误: 无法加载 {po_path}: {e}")
        return 0, 0

    translated_count = 0
    total_empty = 0

    for entry in po:
        if entry.msgid and not entry.obsolete and not entry.msgstr:
            total_empty += 1

            if auto_translate:
                translation = translate_text(entry.msgid)
                if translation:
                    entry.msgstr = translation
                    translated_count += 1

    if translated_count > 0:
        po.save(po_path)

    return total_empty, translated_count

def main():
    import argparse

    parser = argparse.ArgumentParser(description='完整翻译 Nexus 文档')
    parser.add_argument('--dry-run', action='store_true', help='只显示统计，不实际翻译')
    parser.add_argument('--file', help='只处理指定文件')

    args = parser.parse_args()

    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 查找所有 .po 文件
    if args.file:
        po_files = [Path(args.file)]
    else:
        po_files = sorted(locale_dir.rglob('*.po'))

    print(f"处理 {len(po_files)} 个 .po 文件...")
    print("=" * 80)

    total_empty = 0
    total_translated = 0

    for po_file in po_files:
        empty, translated = process_po_file(po_file, auto_translate=not args.dry_run)

        total_empty += empty
        total_translated += translated

        if translated > 0 or args.dry_run:
            rel_path = po_file.relative_to(locale_dir)
            if args.dry_run:
                print(f"{rel_path}: {empty} 个空条目")
            else:
                print(f"[OK] {rel_path}: 翻译了 {translated}/{empty} 个条目")

    print("=" * 80)
    print(f"总计:")
    print(f"  空条目: {total_empty}")
    print(f"  已翻译: {total_translated}")
    print(f"  剩余: {total_empty - total_translated}")

    if args.dry_run:
        print("\n提示: 移除 --dry-run 参数以执行实际翻译")
    elif total_translated > 0:
        print("\n[OK] 翻译完成！运行 'python build_docs.py --lang zh_CN' 构建文档")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
