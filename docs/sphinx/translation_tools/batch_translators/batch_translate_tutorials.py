#!/usr/bin/env python3
"""
批量翻译工具 - 教程文档
"""

import re
from pathlib import Path
import polib

TUTORIAL_TRANSLATIONS = {
    # 教程通用
    "Tutorial": "教程",
    "Tutorials": "教程",
    "Example": "示例",
    "Examples": "示例",
    "Walkthrough": "演练",
    "Step-by-step guide": "分步指南",
    "Hands-on tutorial": "实践教程",

    # GPIO 教程
    "GPIO control": "GPIO 控制",
    "GPIO tutorial": "GPIO 教程",
    "LED control": "LED 控制",
    "Button input": "按钮输入",
    "Digital I/O": "数字 I/O",
    "Pin control": "引脚控制",

    # UART 教程
    "UART communication": "UART 通信",
    "Serial communication": "串行通信",
    "UART tutorial": "UART 教程",
    "Send data": "发送数据",
    "Receive data": "接收数据",
    "Serial port": "串口",

    # 任务教程
    "Task creation": "任务创建",
    "RTOS tasks": "RTOS 任务",
    "Task management": "任务管理",
    "Task scheduling": "任务调度",
    "Task synchronization": "任务同步",

    # 中断教程
    "Interrupt handling": "中断处理",
    "Interrupt tutorial": "中断教程",
    "External interrupt": "外部中断",
    "Interrupt service routine": "中断服务程序",
    "ISR": "ISR",

    # 定时器教程
    "Timer tutorial": "定时器教程",
    "Timer configuration": "定时器配置",
    "PWM generation": "PWM 生成",
    "Timer interrupt": "定时器中断",

    # ADC 教程
    "ADC tutorial": "ADC 教程",
    "Analog input": "模拟输入",
    "ADC conversion": "ADC 转换",
    "Read analog value": "读取模拟值",

    # SPI 教程
    "SPI tutorial": "SPI 教程",
    "SPI communication": "SPI 通信",
    "SPI master": "SPI 主机",
    "SPI slave": "SPI 从机",

    # I2C 教程
    "I2C tutorial": "I2C 教程",
    "I2C communication": "I2C 通信",
    "I2C master": "I2C 主机",
    "I2C slave": "I2C 从机",

    # 示例项目
    "Example projects": "示例项目",
    "Sample applications": "示例应用",
    "Demo projects": "演示项目",
    "Reference implementations": "参考实现",

    # 学习目标
    "Learning objectives": "学习目标",
    "What you will learn": "您将学到什么",
    "Prerequisites": "前置条件",
    "Required knowledge": "所需知识",
    "Required hardware": "所需硬件",
    "Required software": "所需软件",

    # 教程步骤
    "Step 1": "步骤 1",
    "Step 2": "步骤 2",
    "Step 3": "步骤 3",
    "Step 4": "步骤 4",
    "Step 5": "步骤 5",
    "Next step": "下一步",
    "Previous step": "上一步",
    "Final step": "最后一步",

    # 教程说明
    "Follow these steps": "按照以下步骤",
    "Let's begin": "让我们开始",
    "Let's start": "让我们开始",
    "First": "首先",
    "Then": "然后",
    "Next": "接下来",
    "Finally": "最后",
    "Now": "现在",

    # 代码示例
    "Code example": "代码示例",
    "Example code": "示例代码",
    "Complete code": "完整代码",
    "Full example": "完整示例",
    "Code listing": "代码清单",

    # 运行和测试
    "Run the example": "运行示例",
    "Test the code": "测试代码",
    "Verify the output": "验证输出",
    "Expected result": "预期结果",
    "Expected output": "预期输出",

    # 故障排除
    "Troubleshooting": "故障排除",
    "Common issues": "常见问题",
    "Common problems": "常见问题",
    "Solutions": "解决方案",
    "If it doesn't work": "如果不工作",
    "Check the following": "检查以下内容",

    # 练习
    "Exercise": "练习",
    "Exercises": "练习",
    "Try it yourself": "自己尝试",
    "Practice": "实践",
    "Challenge": "挑战",
    "Advanced exercise": "高级练习",

    # 总结
    "Summary": "总结",
    "Conclusion": "结论",
    "What we learned": "我们学到了什么",
    "Key points": "要点",
    "Takeaways": "要点",

    # 下一步
    "Next steps": "下一步",
    "Further reading": "延伸阅读",
    "Related tutorials": "相关教程",
    "See also": "另请参阅",
    "More examples": "更多示例",

    # 提示和技巧
    "Tips": "提示",
    "Tips and tricks": "提示和技巧",
    "Best practices": "最佳实践",
    "Recommendations": "建议",
    "Important notes": "重要说明",

    # 注意事项
    "Note": "注意",
    "Important": "重要",
    "Warning": "警告",
    "Caution": "注意",
    "Tip": "提示",
    "Remember": "记住",

    # 硬件连接
    "Hardware setup": "硬件设置",
    "Hardware connections": "硬件连接",
    "Wiring diagram": "接线图",
    "Circuit diagram": "电路图",
    "Pin connections": "引脚连接",
    "Connect the hardware": "连接硬件",

    # 软件设置
    "Software setup": "软件设置",
    "Project setup": "项目设置",
    "Environment setup": "环境设置",
    "Configuration": "配置",
    "Installation": "安装",

    # 构建和烧录
    "Build the project": "构建项目",
    "Compile the code": "编译代码",
    "Flash the firmware": "烧录固件",
    "Upload the code": "上传代码",
    "Program the device": "编程设备",

    # 调试
    "Debug the code": "调试代码",
    "Debugging tips": "调试技巧",
    "Using the debugger": "使用调试器",
    "Set breakpoints": "设置断点",
    "Inspect variables": "检查变量",

    # 修改和扩展
    "Modify the code": "修改代码",
    "Extend the example": "扩展示例",
    "Customize": "自定义",
    "Adapt for your needs": "根据需要调整",
    "Experiment": "实验",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in TUTORIAL_TRANSLATIONS:
        return TUTORIAL_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in TUTORIAL_TRANSLATIONS.items():
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

    # 处理 tutorials 目录
    po_files = sorted(locale_dir.glob('tutorials/*.po'))

    print(f"处理 {len(po_files)} 个教程文件...")

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
