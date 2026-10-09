#!/usr/bin/env python3
"""
超大型批量翻译工具 - 包含大量完整句子和表达
这个工具包含了从实际未翻译内容中提取的常见句子
"""

import re
from pathlib import Path
import polib

# 从实际文件中提取的需要翻译的句子
MEGA_TRANSLATIONS = {
    # 完整句子 - 平台相关
    "The STM32H7 platform supports STMicroelectronics STM32H7 series ARM Cortex-M7 microcontrollers.": "STM32H7 平台支持 STMicroelectronics STM32H7 系列 ARM Cortex-M7 微控制器。",
    "The GD32 platform supports GigaDevice GD32 series ARM Cortex-M microcontrollers.": "GD32 平台支持兆易创新 GD32 系列 ARM Cortex-M 微控制器。",
    "Native platform provides a PC-based simulation environment.": "本地平台提供基于 PC 的仿真环境。",

    # 配置说明
    "Configuration is stored in .config file": "配置存储在 .config 文件中",
    "Generated header is nexus_config.h": "生成的头文件是 nexus_config.h",
    "Use menuconfig for interactive configuration": "使用 menuconfig 进行交互式配置",
    "Use defconfig for default configuration": "使用 defconfig 进行默认配置",

    # 构建说明
    "Build the project with cmake": "使用 cmake 构建项目",
    "Clean build artifacts": "清理构建文件",
    "Rebuild from scratch": "从头重新构建",
    "Build for specific platform": "为特定平台构建",

    # 测试说明
    "Run all tests": "运行所有测试",
    "Run specific test": "运行特定测试",
    "Generate test report": "生成测试报告",
    "Check test coverage": "检查测试覆盖率",

    # 文档说明
    "Generate API documentation": "生成 API 文档",
    "Build user guide": "构建用户指南",
    "Update documentation": "更新文档",
    "View documentation": "查看文档",

    # 错误和警告
    "Configuration error": "配置错误",
    "Build error": "构建错误",
    "Runtime error": "运行时错误",
    "Compilation warning": "编译警告",

    # 状态消息
    "Configuration successful": "配置成功",
    "Build successful": "构建成功",
    "Test passed": "测试通过",
    "All tests passed": "所有测试通过",

    # 操作指令
    "Follow these instructions": "按照以下说明",
    "Complete the following steps": "完成以下步骤",
    "Refer to the documentation": "请参阅文档",
    "Check the examples": "查看示例",

    # 要求和前提
    "Before you begin": "开始之前",
    "Make sure you have": "确保您已",
    "You will need": "您将需要",
    "Prerequisites include": "前置条件包括",

    # 结果和输出
    "Expected output": "预期输出",
    "Actual output": "实际输出",
    "Output file": "输出文件",
    "Log file": "日志文件",

    # 配置选项描述
    "This enables": "这将启用",
    "This disables": "这将禁用",
    "This configures": "这将配置",
    "This sets": "这将设置",
    "This option": "此选项",

    # 依赖关系
    "Depends on": "依赖于",
    "Requires": "需要",
    "Optional dependency": "可选依赖",
    "Conflicts with": "与...冲突",

    # 版本信息
    "Minimum version": "最低版本",
    "Maximum version": "最高版本",
    "Current version": "当前版本",
    "Latest version": "最新版本",

    # 兼容性
    "Compatible with": "兼容",
    "Not compatible with": "不兼容",
    "Backward compatible": "向后兼容",
    "Forward compatible": "向前兼容",

    # 性能
    "Performance impact": "性能影响",
    "Memory usage": "内存使用",
    "CPU usage": "CPU 使用",
    "Execution time": "执行时间",

    # 安全性
    "Security consideration": "安全考虑",
    "Thread-safe": "线程安全",
    "Not thread-safe": "非线程安全",
    "Interrupt-safe": "中断安全",

    # 限制
    "Known limitation": "已知限制",
    "Current limitation": "当前限制",
    "Platform limitation": "平台限制",
    "Hardware limitation": "硬件限制",

    # 建议
    "It is recommended": "建议",
    "It is not recommended": "不建议",
    "Best practice": "最佳实践",
    "Common practice": "常见做法",

    # 警告和注意
    "Please note": "请注意",
    "Be aware": "请注意",
    "Keep in mind": "请记住",
    "Take care": "请小心",

    # 更多完整句子
    "This is the default configuration": "这是默认配置",
    "This is an optional feature": "这是可选特性",
    "This is required for": "这是...所必需的",
    "This is not supported": "不支持此功能",
    "This has been deprecated": "此功能已弃用",
    "This will be removed": "此功能将被移除",

    # 操作结果
    "Operation completed successfully": "操作成功完成",
    "Operation failed": "操作失败",
    "Operation in progress": "操作进行中",
    "Operation cancelled": "操作已取消",

    # 文件操作
    "File not found": "文件未找到",
    "File exists": "文件已存在",
    "File created": "文件已创建",
    "File deleted": "文件已删除",
    "File modified": "文件已修改",

    # 网络操作
    "Connection established": "连接已建立",
    "Connection failed": "连接失败",
    "Connection timeout": "连接超时",
    "Connection closed": "连接已关闭",

    # 数据操作
    "Data received": "数据已接收",
    "Data sent": "数据已发送",
    "Data corrupted": "数据损坏",
    "Data valid": "数据有效",
    "Data invalid": "数据无效",

    # 初始化
    "Initialization complete": "初始化完成",
    "Initialization failed": "初始化失败",
    "Initialization in progress": "初始化进行中",
    "Already initialized": "已初始化",
    "Not initialized": "未初始化",

    # 资源管理
    "Resource allocated": "资源已分配",
    "Resource freed": "资源已释放",
    "Resource not available": "资源不可用",
    "Resource busy": "资源忙碌",

    # 队列和缓冲
    "Queue full": "队列已满",
    "Queue empty": "队列为空",
    "Buffer overflow": "缓冲区溢出",
    "Buffer underflow": "缓冲区下溢",

    # 同步
    "Lock acquired": "已获取锁",
    "Lock released": "已释放锁",
    "Waiting for lock": "等待锁",
    "Deadlock detected": "检测到死锁",

    # 任务和线程
    "Task created": "任务已创建",
    "Task deleted": "任务已删除",
    "Task suspended": "任务已挂起",
    "Task resumed": "任务已恢复",
    "Task running": "任务运行中",

    # 定时器
    "Timer started": "定时器已启动",
    "Timer stopped": "定时器已停止",
    "Timer expired": "定时器已过期",
    "Timer reset": "定时器已重置",

    # 中断
    "Interrupt enabled": "中断已启用",
    "Interrupt disabled": "中断已禁用",
    "Interrupt pending": "中断待处理",
    "Interrupt handled": "中断已处理",

    # 电源
    "Power on": "上电",
    "Power off": "断电",
    "Low power mode": "低功耗模式",
    "Normal power mode": "正常功耗模式",

    # 复位
    "System reset": "系统复位",
    "Soft reset": "软复位",
    "Hard reset": "硬复位",
    "Watchdog reset": "看门狗复位",

    # 校准
    "Calibration required": "需要校准",
    "Calibration complete": "校准完成",
    "Calibration failed": "校准失败",
    "Calibration in progress": "校准进行中",

    # 验证
    "Validation passed": "验证通过",
    "Validation failed": "验证失败",
    "Checksum valid": "校验和有效",
    "Checksum invalid": "校验和无效",

    # 更新
    "Update available": "有可用更新",
    "Update required": "需要更新",
    "Update complete": "更新完成",
    "Update failed": "更新失败",

    # 备份和恢复
    "Backup created": "备份已创建",
    "Backup restored": "备份已恢复",
    "Restore failed": "恢复失败",
    "No backup found": "未找到备份",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in MEGA_TRANSLATIONS:
        return MEGA_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in MEGA_TRANSLATIONS.items():
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
