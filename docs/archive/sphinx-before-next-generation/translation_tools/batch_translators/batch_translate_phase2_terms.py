#!/usr/bin/env python3
"""
批量翻译工具 - 第2阶段：技术术语和短语
处理常见的技术术语、短语和单词
"""

import re
from pathlib import Path
import polib

# 技术术语和短语翻译
TERM_TRANSLATIONS = {
    # 通知和消息
    "Notifications": "通知",
    "Caching": "缓存",
    "**Windows**::": "**Windows**：",
    "**Linux**::": "**Linux**：",
    "**macOS**::": "**macOS**：",

    # 硬件相关
    "ESP32-WROOM-32": "ESP32-WROOM-32",
    "USB OTG FS/HS": "USB OTG FS/HS",
    "GD32F407V-START": "GD32F407V-START",

    # Git 相关
    "``feat``: New feature": "``feat``：新功能",
    "``fix``: Bug fix": "``fix``：错误修复",
    "``docs``: Documentation changes": "``docs``：文档更改",
    "``style``: Code style changes": "``style``：代码样式更改",
    "``refactor``: Code refactoring": "``refactor``：代码重构",
    "``test``: Test changes": "``test``：测试更改",
    "``chore``: Build/tooling changes": "``chore``：构建/工具更改",

    # 配置命令
    "``diff`` - Compare configuration files": "``diff`` - 比较配置文件",
    "``info`` - Display configuration information": "``info`` - 显示配置信息",
    "``list`` - List all configuration entries": "``list`` - 列出所有配置条目",
    "``get`` - Get configuration value": "``get`` - 获取配置值",
    "``set`` - Set configuration value": "``set`` - 设置配置值",
    "``delete`` - Delete configuration entry": "``delete`` - 删除配置条目",
    "``export`` - Export configuration": "``export`` - 导出配置",
    "``import`` - Import configuration": "``import`` - 导入配置",

    # 线程安全
    "The Config Manager is **thread-safe** when using the Flash backend with proper locking.": "使用 Flash 后端并进行适当锁定时，配置管理器是**线程安全**的。",

    # 文档描述
    "This section provides comprehensive API documentation for all Nexus modules. The API is organized by functional area.": "本节提供所有 Nexus 模块的完整 API 文档。API 按功能区域组织。",
    "The Nexus Init Framework provides a compile-time automatic initialization system using linker sections.": "Nexus Init 框架使用链接器段提供编译时自动初始化系统。",

    # 常见动作
    "Click": "点击",
    "Select": "选择",
    "Choose": "选择",
    "Enter": "输入",
    "Type": "输入",
    "Press": "按",
    "Download": "下载",
    "Upload": "上传",
    "Extract": "解压",
    "Compile": "编译",
    "Link": "链接",

    # 状态和结果
    "Completed": "已完成",
    "In Progress": "进行中",
    "Pending": "待处理",
    "Cancelled": "已取消",
    "Aborted": "已中止",
    "Timeout": "超时",
    "Retry": "重试",

    # 级别和优先级
    "High Priority": "高优先级",
    "Medium Priority": "中优先级",
    "Low Priority": "低优先级",
    "Critical": "严重",
    "Major": "主要",
    "Minor": "次要",
    "Trivial": "轻微",

    # 文件和目录
    "Source Files": "源文件",
    "Header Files": "头文件",
    "Object Files": "目标文件",
    "Library Files": "库文件",
    "Binary Files": "二进制文件",
    "Configuration Files": "配置文件",
    "Script Files": "脚本文件",
    "Documentation Files": "文档文件",

    # 工具和实用程序
    "Compiler": "编译器",
    "Linker": "链接器",
    "Debugger": "调试器",
    "Profiler": "性能分析器",
    "Analyzer": "分析器",
    "Generator": "生成器",
    "Validator": "验证器",
    "Formatter": "格式化器",

    # 测试相关
    "Unit Test": "单元测试",
    "Integration Test": "集成测试",
    "System Test": "系统测试",
    "Regression Test": "回归测试",
    "Performance Test": "性能测试",
    "Stress Test": "压力测试",
    "Test Case": "测试用例",
    "Test Suite": "测试套件",
    "Test Coverage": "测试覆盖率",

    # 版本控制
    "Commit": "提交",
    "Branch": "分支",
    "Merge": "合并",
    "Rebase": "变基",
    "Cherry-pick": "挑选",
    "Tag": "标签",
    "Release": "发布",
    "Hotfix": "热修复",

    # 构建相关
    "Clean Build": "清理构建",
    "Incremental Build": "增量构建",
    "Debug Build": "调试构建",
    "Release Build": "发布构建",
    "Build Target": "构建目标",
    "Build Configuration": "构建配置",

    # 内存相关
    "Stack": "栈",
    "Heap": "堆",
    "Static Memory": "静态内存",
    "Dynamic Memory": "动态内存",
    "Memory Pool": "内存池",
    "Memory Leak": "内存泄漏",
    "Memory Allocation": "内存分配",
    "Memory Deallocation": "内存释放",

    # 同步原语
    "Mutex": "互斥锁",
    "Semaphore": "信号量",
    "Event": "事件",
    "Condition Variable": "条件变量",
    "Spinlock": "自旋锁",
    "Read-Write Lock": "读写锁",

    # 通信相关
    "Protocol": "协议",
    "Packet": "数据包",
    "Frame": "帧",
    "Message": "消息",
    "Request": "请求",
    "Response": "响应",
    "Acknowledgment": "确认",
    "Timeout": "超时",

    # 外设相关
    "Peripheral": "外设",
    "Interface": "接口",
    "Bus": "总线",
    "Channel": "通道",
    "Port": "端口",
    "Pin": "引脚",
    "Register": "寄存器",
    "Buffer": "缓冲区",

    # 电源相关
    "Power On": "上电",
    "Power Off": "断电",
    "Sleep Mode": "睡眠模式",
    "Deep Sleep": "深度睡眠",
    "Standby Mode": "待机模式",
    "Low Power Mode": "低功耗模式",
    "Wake Up": "唤醒",

    # 中断相关
    "Interrupt": "中断",
    "ISR": "中断服务程序",
    "Interrupt Handler": "中断处理程序",
    "Interrupt Priority": "中断优先级",
    "Interrupt Vector": "中断向量",
    "Nested Interrupt": "嵌套中断",

    # 时钟相关
    "Clock Source": "时钟源",
    "Clock Frequency": "时钟频率",
    "Clock Divider": "时钟分频器",
    "Clock Multiplier": "时钟倍频器",
    "PLL": "锁相环",
    "Oscillator": "振荡器",
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
            if entry.msgid in TERM_TRANSLATIONS:
                entry.msgstr = TERM_TRANSLATIONS[entry.msgid]
                translated_count += 1
            # 去除首尾空白后匹配
            elif entry.msgid.strip() in TERM_TRANSLATIONS:
                entry.msgstr = TERM_TRANSLATIONS[entry.msgid.strip()]
                translated_count += 1

    if translated_count > 0:
        po.save(po_path)

    return translated_count

def main():
    locale_dir = Path('locale/zh_CN/LC_MESSAGES')

    if not locale_dir.exists():
        print(f"错误: {locale_dir} 不存在")
        return 1

    # 处理所有文件
    po_files = sorted(locale_dir.rglob('*.po'))

    print(f"处理 {len(po_files)} 个文件...")

    total_translated = 0
    files_with_translations = 0

    for po_file in po_files:
        count = translate_po_file(po_file)
        if count > 0:
            files_with_translations += 1
            rel_path = po_file.relative_to(locale_dir)
            print(f"  {rel_path}: {count} 条")
        total_translated += count

    print(f"\n总计翻译: {total_translated} 条")
    print(f"涉及文件: {files_with_translations} 个")

    return 0

if __name__ == '__main__':
    import sys
    sys.exit(main())
