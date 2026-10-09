#!/usr/bin/env python3
"""
高级翻译工具 - 处理完整句子和段落
使用扩展的短语词典和智能翻译规则
"""

import re
from pathlib import Path
import polib

# 完整句子和段落翻译词典
PHRASE_DICT = {
    # === 文档导航和结构 ===
    "This section describes": "本节描述",
    "This chapter covers": "本章涵盖",
    "This guide explains": "本指南说明",
    "This tutorial shows": "本教程展示",
    "This document provides": "本文档提供",
    "The following sections": "以下各节",
    "The next section": "下一节",
    "The previous section": "上一节",
    "For more information": "更多信息请参阅",
    "For details": "详细信息请参阅",
    "See also": "另请参阅",
    "Refer to": "请参阅",
    "As shown in": "如图所示",
    "As described in": "如所述",
    "As mentioned": "如前所述",

    # === 操作说明 ===
    "To get started": "开始使用",
    "To begin": "首先",
    "To install": "安装方法",
    "To configure": "配置方法",
    "To build": "构建方法",
    "To run": "运行方法",
    "To use": "使用方法",
    "To enable": "启用方法",
    "To disable": "禁用方法",
    "Follow these steps": "按照以下步骤",
    "Perform the following": "执行以下操作",
    "You can": "您可以",
    "You should": "您应该",
    "You must": "您必须",
    "You need to": "您需要",
    "It is recommended": "建议",
    "It is required": "必须",
    "Make sure": "确保",
    "Ensure that": "确保",

    # === 条件和逻辑 ===
    "If you want to": "如果您想要",
    "If you need to": "如果您需要",
    "When you": "当您",
    "Before you": "在您...之前",
    "After you": "在您...之后",
    "Once you": "一旦您",
    "In order to": "为了",
    "In case of": "如果出现",
    "Depending on": "取决于",
    "According to": "根据",
    "Based on": "基于",

    # === 描述和说明 ===
    "This is": "这是",
    "This allows": "这允许",
    "This enables": "这使得",
    "This provides": "这提供",
    "This includes": "这包括",
    "This contains": "这包含",
    "This supports": "这支持",
    "This requires": "这需要",
    "The system": "系统",
    "The framework": "框架",
    "The library": "库",
    "The module": "模块",
    "The function": "函数",
    "The API": "API",
    "The interface": "接口",
    "The driver": "驱动程序",
    "The application": "应用程序",

    # === 特性和功能 ===
    "supports the following": "支持以下",
    "provides the following": "提供以下",
    "includes the following": "包括以下",
    "offers the following": "提供以下",
    "Key features": "主要特性",
    "Main features": "主要特性",
    "Available options": "可用选项",
    "Supported platforms": "支持的平台",
    "Supported devices": "支持的设备",
    "Compatible with": "兼容",

    # === 注意事项 ===
    "Note that": "请注意",
    "Please note": "请注意",
    "Keep in mind": "请记住",
    "Be aware": "请注意",
    "Important": "重要",
    "Warning": "警告",
    "Caution": "注意",
    "Tip": "提示",
    "Do not": "不要",
    "Never": "切勿",
    "Always": "始终",
    "Make sure to": "务必",

    # === 结果和状态 ===
    "will be": "将会",
    "will result in": "将导致",
    "can be": "可以",
    "can be used": "可以使用",
    "should be": "应该",
    "must be": "必须",
    "may be": "可能",
    "is used to": "用于",
    "is required": "是必需的",
    "is optional": "是可选的",
    "is available": "可用",
    "is supported": "受支持",
    "is enabled": "已启用",
    "is disabled": "已禁用",

    # === 比较和关系 ===
    "similar to": "类似于",
    "different from": "不同于",
    "same as": "与...相同",
    "compared to": "与...相比",
    "in addition to": "除了...之外",
    "as well as": "以及",
    "instead of": "而不是",
    "rather than": "而不是",
    "more than": "超过",
    "less than": "少于",
    "at least": "至少",
    "at most": "最多",
    "up to": "最多",

    # === 时间和顺序 ===
    "First": "首先",
    "Second": "其次",
    "Third": "第三",
    "Finally": "最后",
    "Then": "然后",
    "Next": "接下来",
    "After that": "之后",
    "Before that": "之前",
    "Meanwhile": "同时",
    "At the same time": "同时",
    "During": "在...期间",
    "While": "当...时",
    "Until": "直到",
    "Once": "一旦",
    "When": "当",

    # === 示例和引用 ===
    "For example": "例如",
    "For instance": "例如",
    "Such as": "例如",
    "Including": "包括",
    "Like": "如",
    "In this example": "在此示例中",
    "In the example": "在示例中",
    "The example shows": "示例展示",
    "Consider the following": "考虑以下",
    "Take a look at": "查看",

    # === 问题和解决方案 ===
    "If you encounter": "如果遇到",
    "If you see": "如果看到",
    "If this happens": "如果发生这种情况",
    "To fix this": "要解决此问题",
    "To resolve": "要解决",
    "To solve": "要解决",
    "The solution is": "解决方案是",
    "The workaround is": "变通方法是",
    "Try the following": "尝试以下方法",

    # === 完整常用句子 ===
    "This is a simple example": "这是一个简单示例",
    "This is the default behavior": "这是默认行为",
    "This is not supported": "不支持此功能",
    "This is optional": "这是可选的",
    "This is required": "这是必需的",
    "This is recommended": "建议这样做",
    "This is deprecated": "此功能已弃用",
    "This will be removed": "此功能将被移除",
    "This has been added": "已添加此功能",
    "This has been removed": "已移除此功能",
    "This has been changed": "此功能已更改",
    "This has been fixed": "已修复此问题",
    "This has been improved": "已改进此功能",
    "This has been updated": "已更新此功能",

    # === 配置和设置 ===
    "The default value is": "默认值为",
    "The valid range is": "有效范围是",
    "The possible values are": "可能的值为",
    "The supported values are": "支持的值为",
    "Set this to": "将此设置为",
    "Change this to": "将此更改为",
    "Configure this option": "配置此选项",
    "Enable this option": "启用此选项",
    "Disable this option": "禁用此选项",

    # === 文件和路径 ===
    "The file is located": "文件位于",
    "The directory contains": "目录包含",
    "Create a new file": "创建新文件",
    "Open the file": "打开文件",
    "Edit the file": "编辑文件",
    "Save the file": "保存文件",
    "Add the following": "添加以下内容",
    "Replace with": "替换为",
    "Modify as follows": "按如下方式修改",

    # === 构建和编译 ===
    "Build the project": "构建项目",
    "Compile the code": "编译代码",
    "Run the build": "运行构建",
    "Clean the build": "清理构建",
    "The build will": "构建将",
    "The compilation": "编译",
    "Build output": "构建输出",
    "Build directory": "构建目录",

    # === 测试和调试 ===
    "Run the test": "运行测试",
    "Execute the test": "执行测试",
    "The test will": "测试将",
    "Test output": "测试输出",
    "Debug output": "调试输出",
    "Enable debugging": "启用调试",
    "Disable debugging": "禁用调试",
    "Set a breakpoint": "设置断点",

    # === API 和函数 ===
    "Call this function": "调用此函数",
    "This function returns": "此函数返回",
    "This function takes": "此函数接受",
    "Pass the following": "传递以下",
    "The parameter": "参数",
    "The return value": "返回值",
    "On success": "成功时",
    "On failure": "失败时",
    "On error": "出错时",
    "Returns zero": "返回零",
    "Returns non-zero": "返回非零值",
    "Returns NULL": "返回 NULL",

    # === 硬件和外设 ===
    "Initialize the peripheral": "初始化外设",
    "Configure the peripheral": "配置外设",
    "Enable the peripheral": "启用外设",
    "Disable the peripheral": "禁用外设",
    "The peripheral": "外设",
    "The hardware": "硬件",
    "The device": "设备",
    "The pin": "引脚",
    "The port": "端口",
    "The register": "寄存器",
    "Set the pin": "设置引脚",
    "Clear the pin": "清除引脚",
    "Toggle the pin": "切换引脚",
    "Read the pin": "读取引脚",
    "Write to the pin": "写入引脚",

    # === 中断和事件 ===
    "Enable interrupts": "启用中断",
    "Disable interrupts": "禁用中断",
    "Register a handler": "注册处理程序",
    "The interrupt handler": "中断处理程序",
    "The callback function": "回调函数",
    "When the interrupt": "当中断",
    "Trigger an interrupt": "触发中断",
    "Handle the interrupt": "处理中断",

    # === 任务和线程 ===
    "Create a task": "创建任务",
    "Delete a task": "删除任务",
    "Start the task": "启动任务",
    "Stop the task": "停止任务",
    "The task will": "任务将",
    "Task priority": "任务优先级",
    "Task stack": "任务栈",
    "Thread-safe": "线程安全",

    # === 内存和资源 ===
    "Allocate memory": "分配内存",
    "Free memory": "释放内存",
    "Memory allocation": "内存分配",
    "Memory usage": "内存使用",
    "Resource usage": "资源使用",
    "Available memory": "可用内存",
    "Used memory": "已用内存",

    # === 错误处理 ===
    "Error handling": "错误处理",
    "Error code": "错误代码",
    "Error message": "错误消息",
    "Check for errors": "检查错误",
    "Handle errors": "处理错误",
    "Return an error": "返回错误",
    "Report an error": "报告错误",
    "If an error occurs": "如果发生错误",
    "In case of error": "如果出错",

    # === 版本和兼容性 ===
    "Version information": "版本信息",
    "Compatibility": "兼容性",
    "Backward compatible": "向后兼容",
    "Not compatible": "不兼容",
    "Requires version": "需要版本",
    "Minimum version": "最低版本",
    "Latest version": "最新版本",
    "Current version": "当前版本",

    # === 许可和版权 ===
    "Licensed under": "根据...许可",
    "Copyright": "版权",
    "All rights reserved": "保留所有权利",
    "Open source": "开源",
    "Free software": "自由软件",

    # === 贡献和社区 ===
    "Contributions are welcome": "欢迎贡献",
    "Report bugs": "报告错误",
    "Submit a pull request": "提交拉取请求",
    "Open an issue": "提交问题",
    "Contact us": "联系我们",
    "Join the community": "加入社区",
}

# 技术术语词典（保持英文或中英混用）
TECH_TERMS = {
    "GPIO": "GPIO",
    "UART": "UART",
    "SPI": "SPI",
    "I2C": "I2C",
    "PWM": "PWM",
    "ADC": "ADC",
    "DAC": "DAC",
    "DMA": "DMA",
    "RTC": "RTC",
    "WDT": "WDT",
    "CAN": "CAN",
    "USB": "USB",
    "Ethernet": "以太网",
    "WiFi": "WiFi",
    "Bluetooth": "蓝牙",
    "HAL": "HAL",
    "OSAL": "OSAL",
    "RTOS": "RTOS",
    "FreeRTOS": "FreeRTOS",
    "Cortex-M": "Cortex-M",
    "ARM": "ARM",
    "STM32": "STM32",
    "GD32": "GD32",
    "NXP": "NXP",
    "Microchip": "Microchip",
    "Nordic": "Nordic",
    "Espressif": "Espressif",
    "MCU": "MCU",
    "CPU": "CPU",
    "Flash": "Flash",
    "RAM": "RAM",
    "ROM": "ROM",
    "EEPROM": "EEPROM",
    "Bootloader": "引导加载程序",
    "Firmware": "固件",
    "SDK": "SDK",
    "API": "API",
    "IDE": "IDE",
    "GCC": "GCC",
    "Clang": "Clang",
    "CMake": "CMake",
    "Make": "Make",
    "Ninja": "Ninja",
    "GDB": "GDB",
    "OpenOCD": "OpenOCD",
    "J-Link": "J-Link",
    "ST-Link": "ST-Link",
    "JTAG": "JTAG",
    "SWD": "SWD",
    "Linux": "Linux",
    "Windows": "Windows",
    "macOS": "macOS",
    "Python": "Python",
    "C": "C",
    "C++": "C++",
    "Shell": "Shell",
    "Bash": "Bash",
    "Git": "Git",
    "GitHub": "GitHub",
    "Doxygen": "Doxygen",
    "Sphinx": "Sphinx",
    "Markdown": "Markdown",
    "RST": "RST",
    "JSON": "JSON",
    "YAML": "YAML",
    "XML": "XML",
    "HTML": "HTML",
    "CSS": "CSS",
}

def should_not_translate(msgid):
    """判断是否不需要翻译"""
    if not msgid or not msgid.strip():
        return True

    msgid = msgid.strip()

    # 代码块
    if (msgid.startswith('``') and msgid.endswith('``')) or \
       (msgid.startswith('`') and msgid.endswith('`')):
        return True

    # 纯数字
    if msgid.isdigit():
        return True

    # 全大写常量
    if re.match(r'^[A-Z_][A-Z0-9_]*$', msgid) and len(msgid) > 1:
        return True

    # Shell 命令
    if msgid.startswith('$') or msgid.startswith('./') or \
       msgid.startswith('python ') or msgid.startswith('cmake ') or \
       msgid.startswith('make ') or msgid.startswith('git '):
        return True

    # URL
    if msgid.startswith('http://') or msgid.startswith('https://') or \
       msgid.startswith('www.'):
        return True

    # RST 指令
    if msgid.startswith('..') or (msgid.startswith(':') and msgid.endswith(':')):
        return True

    # 文件路径（简单判断）
    if '/' in msgid and ' ' not in msgid and len(msgid.split('/')) > 2:
        return True

    # 包含大量代码符号
    code_chars = sum(1 for c in msgid if c in '(){}[]<>=;')
    if code_chars > len(msgid) * 0.3:
        return True

    # 单个字符或很短的技术缩写
    if len(msgid) <= 2 and msgid.isupper():
        return True

    return False

def translate_with_phrases(msgid):
    """使用短语词典翻译"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 不需要翻译
    if should_not_translate(msgid):
        return msgid

    # 完全匹配
    if msgid in PHRASE_DICT:
        return PHRASE_DICT[msgid]

    # 不区分大小写匹配
    msgid_lower = msgid.lower()
    for key, value in PHRASE_DICT.items():
        if key.lower() == msgid_lower:
            # 保持原始大小写风格
            if msgid.isupper():
                return value.upper()
            elif msgid[0].isupper():
                return value[0].upper() + value[1:] if len(value) > 1 else value.upper()
            return value

    # 尝试部分匹配（从最长的开始）
    sorted_phrases = sorted(PHRASE_DICT.items(), key=lambda x: len(x[0]), reverse=True)
    for phrase, translation in sorted_phrases:
        if phrase.lower() in msgid_lower:
            # 找到匹配的短语，尝试替换
            # 这里需要更智能的处理，暂时返回空
            pass

    return ""

def process_po_file(po_path, auto_translate=True, verbose=False):
    """处理 .po 文件"""
    try:
        po = polib.pofile(po_path)
    except Exception as e:
        if verbose:
            print(f"错误: 无法加载 {po_path}: {e}")
        return 0, 0, 0

    total = 0
    already_translated = 0
    newly_translated = 0

    for entry in po:
        if entry.msgid and not entry.obsolete:
            total += 1

            if entry.msgstr and entry.msgstr.strip():
                already_translated += 1
                continue

            if auto_translate:
                translation = translate_with_phrases(entry.msgid)
                if translation and translation != entry.msgid:
                    entry.msgstr = translation
                    newly_translated += 1
                    if verbose:
                        print(f"  翻译: {entry.msgid[:50]}... -> {translation[:50]}...")

    if newly_translated > 0:
        po.save(po_path)

    return total, already_translated, newly_translated

def main():
    import argparse

    parser = argparse.ArgumentParser(description='高级翻译 Nexus 文档')
    parser.add_argument('--dry-run', action='store_true', help='只显示统计，不实际翻译')
    parser.add_argument('--verbose', '-v', action='store_true', help='显示详细信息')
    parser.add_argument('--file', help='只处理指定文件')
    parser.add_argument('--stats', action='store_true', help='显示翻译统计')

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

    total_entries = 0
    total_already = 0
    total_newly = 0

    for po_file in po_files:
        entries, already, newly = process_po_file(
            po_file,
            auto_translate=not args.dry_run,
            verbose=args.verbose
        )

        total_entries += entries
        total_already += already
        total_newly += newly

        if newly > 0 or args.verbose or args.stats:
            rel_path = po_file.relative_to(locale_dir)
            if args.dry_run:
                untranslated = entries - already
                print(f"{rel_path}: {untranslated} 个待翻译条目")
            elif newly > 0:
                print(f"✓ {rel_path}: 新翻译 {newly} 个条目")

    print("=" * 80)
    print(f"总计:")
    print(f"  总条目: {total_entries}")
    print(f"  已翻译: {total_already} ({total_already/total_entries*100:.1f}%)")
    if not args.dry_run:
        print(f"  新翻译: {total_newly}")
        print(f"  当前完成度: {(total_already + total_newly)/total_entries*100:.1f}%")
    print(f"  待翻译: {total_entries - total_already - total_newly}")

    if args.dry_run:
        print("\n提示: 移除 --dry-run 参数以执行实际翻译")
    elif total_newly > 0:
        print("\n✓ 翻译完成！运行 'python build_docs.py --lang zh_CN' 构建文档")
    else:
        print("\n提示: 没有找到可以自动翻译的内容")
        print("提示: 剩余内容需要人工翻译或使用专业翻译服务")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
