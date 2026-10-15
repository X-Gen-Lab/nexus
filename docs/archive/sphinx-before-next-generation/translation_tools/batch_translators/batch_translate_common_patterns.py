#!/usr/bin/env python3
"""
批量翻译工具 - 常见模式
基于实际未翻译内容中的高频句子
"""

import re
from pathlib import Path
import polib

COMMON_PATTERNS = {
    # IDE 相关
    "Using Visual Studio Code": "使用 Visual Studio Code",
    "Using Eclipse": "使用 Eclipse",
    "Using CLion": "使用 CLion",
    "Create ``.vscode/launch.json``:": "创建 ``.vscode/launch.json``：",
    "Create ``.vscode/settings.json``:": "创建 ``.vscode/settings.json``：",

    # 性能优化
    "Performance Optimization": "性能优化",
    "Memory Optimization": "内存优化",
    "Code Optimization": "代码优化",
    "Speed Optimization": "速度优化",

    # 外设功能
    "Input/Output, alternate functions, interrupts": "输入/输出、复用功能、中断",
    "SD/MMC card interface": "SD/MMC 卡接口",
    "Calendar, alarm, tamper detection": "日历、闹钟、篡改检测",
    "Independent and window watchdog": "独立看门狗和窗口看门狗",

    # 硬件设置
    "Use 3.3V regulated supply": "使用 3.3V 稳压电源",
    "Add bulk capacitor (10µF) near power input": "在电源输入附近添加大容量电容（10µF）",
    "Ensure flash wait states are configured for clock speed": "确保为时钟速度配置了 Flash 等待状态",

    # 故障排除
    "Solution: Install ARM GCC toolchain and add to PATH.": "解决方案：安装 ARM GCC 工具链并添加到 PATH。",
    "If system doesn't start or runs at wrong speed:": "如果系统无法启动或以错误速度运行：",
    "Check PLL configuration": "检查 PLL 配置",
    "Check clock configuration": "检查时钟配置",
    "Check power supply": "检查电源",

    # 优化建议
    "Use DMA for data transfers": "使用 DMA 进行数据传输",
    "Optimize interrupt priorities": "优化中断优先级",
    "Enable instruction and data caches": "启用指令和数据缓存",
    "Use FPU for floating-point operations": "使用 FPU 进行浮点运算",

    # 配置命令
    "cp platforms/native/defconfig .config\npython scripts/kconfig/generate_config.py": "cp platforms/native/defconfig .config\npython scripts/kconfig/generate_config.py",

    # 验证相关
    "Source file existence": "源文件存在性",
    "Circular dependencies": "循环依赖",
    "Undefined symbol references": "未定义的符号引用",
    "Unsatisfiable dependencies": "无法满足的依赖",
    "Missing required symbols": "缺少必需的符号",

    # 代码审查
    "Address all review comments": "处理所有审查评论",
    "Fix review issues": "修复审查问题",
    "Respond to feedback": "回应反馈",

    # 安装命令
    "pip install -r scripts/validation/requirements.txt": "pip install -r scripts/validation/requirements.txt",
    "pip install -r requirements.txt": "pip install -r requirements.txt",

    # 覆盖率标记
    "🟡 **Yellow**: Partially covered branches": "🟡 **黄色**：部分覆盖的分支",
    "🟢 **Green**: Fully covered lines": "🟢 **绿色**：完全覆盖的行",
    "🔴 **Red**: Uncovered lines": "🔴 **红色**：未覆盖的行",

    # 环境变量
    "Environment Variables": "环境变量",
    "System Variables": "系统变量",
    "User Variables": "用户变量",

    # 外设特性
    "Async/Sync TX/RX, DMA, hardware flow control": "异步/同步 TX/RX、DMA、硬件流控制",
    "Master/Slave, DMA, NSS management": "主/从模式、DMA、NSS 管理",
    "Master/Slave, DMA, 10-bit addressing": "主/从模式、DMA、10 位寻址",
    "General-purpose, advanced, basic timers": "通用定时器、高级定时器、基本定时器",
    "12-bit resolution, DMA, multi-channel": "12 位分辨率、DMA、多通道",
    "12-bit resolution, DMA": "12 位分辨率、DMA",
    "CAN 2.0A/B, up to 1 Mbps": "CAN 2.0A/B，最高 1 Mbps",

    # 平台差异
    "Differences from STM32": "与 STM32 的差异",
    "Differences from GD32": "与 GD32 的差异",
    "Compatibility with STM32": "与 STM32 的兼容性",

    # 应用场景
    "Cost-sensitive projects": "成本敏感型项目",
    "High-performance applications": "高性能应用",
    "General embedded applications": "通用嵌入式应用",
    "Cross-platform development": "跨平台开发",

    # 限制
    "Single-precision FPU only": "仅单精度 FPU",
    "**Pin Multiplexing**: Limited pins require careful planning": "**引脚复用**：有限的引脚需要仔细规划",
    "**Temperature Range**: Standard range (-40°C to +85°C)": "**温度范围**：标准范围（-40°C 至 +85°C）",

    # 工具
    "Using ST-Link Utility": "使用 ST-Link 实用程序",
    "Using OpenOCD": "使用 OpenOCD",
    "Using J-Link": "使用 J-Link",

    # 开发板
    "STM32F4 Discovery board with FreeRTOS support": "支持 FreeRTOS 的 STM32F4 Discovery 板",
    "STM32F4 Discovery board or compatible hardware": "STM32F4 Discovery 板或兼容硬件",
    "STM32H7 Nucleo board": "STM32H7 Nucleo 板",
    "GD32 development board": "GD32 开发板",

    # 前置条件
    "Completed the :doc:`../getting_started/installation` guide": "完成了 :doc:`../getting_started/installation` 指南",
    "Completed the installation": "完成了安装",
    "Completed the setup": "完成了设置",

    # 用例建议
    "Use Case Recommendations": "用例建议",
    "Recommended Use Cases": "推荐用例",
    "Best Use Cases": "最佳用例",

    # 后端类型
    "**Flash backend**: Thread-safe with internal locking": "**Flash 后端**：使用内部锁实现线程安全",
    "**RAM backend**: Not thread-safe, requires external mutex": "**RAM 后端**：非线程安全，需要外部互斥锁",

    # 更多常见短语
    "For more information": "更多信息",
    "For details": "详细信息",
    "For examples": "示例",
    "See documentation": "参见文档",
    "Refer to manual": "请参阅手册",
    "Check datasheet": "查看数据手册",

    # 配置步骤
    "Step 1: Configure": "步骤 1：配置",
    "Step 2: Build": "步骤 2：构建",
    "Step 3: Flash": "步骤 3：烧录",
    "Step 4: Test": "步骤 4：测试",
    "Step 5: Debug": "步骤 5：调试",

    # 结果
    "Build successful": "构建成功",
    "Build failed": "构建失败",
    "Test passed": "测试通过",
    "Test failed": "测试失败",
    "Flash successful": "烧录成功",
    "Flash failed": "烧录失败",

    # 状态
    "In progress": "进行中",
    "Completed": "已完成",
    "Failed": "失败",
    "Pending": "待处理",
    "Cancelled": "已取消",

    # 操作
    "Click to expand": "点击展开",
    "Click to collapse": "点击折叠",
    "Click for details": "点击查看详情",
    "Click to copy": "点击复制",

    # 导航
    "Previous page": "上一页",
    "Next page": "下一页",
    "Back to top": "返回顶部",
    "Go to section": "转到章节",

    # 搜索
    "Search results": "搜索结果",
    "No results found": "未找到结果",
    "Search documentation": "搜索文档",
    "Filter results": "筛选结果",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in COMMON_PATTERNS:
        return COMMON_PATTERNS[msgid]

    # 不区分大小写匹配
    for key, value in COMMON_PATTERNS.items():
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

    print(f"处理 {len(po_files)} 个文件...")

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
