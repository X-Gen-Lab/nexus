#!/usr/bin/env python3
"""
批量翻译工具 - 开发指南和脚本文档
"""

import re
from pathlib import Path
import polib

DEVELOPMENT_TRANSLATIONS = {
    # 脚本和工具
    "Script validation": "脚本验证",
    "Validation framework": "验证框架",
    "Script testing": "脚本测试",
    "Test scripts": "测试脚本",
    "Build scripts": "构建脚本",
    "Utility scripts": "实用脚本",
    "Helper scripts": "辅助脚本",

    # 文档贡献
    "Documentation contributing": "文档贡献",
    "Contributing to documentation": "为文档做贡献",
    "Documentation guidelines": "文档指南",
    "Writing documentation": "编写文档",
    "Documentation style": "文档风格",
    "Documentation format": "文档格式",

    # 代码覆盖率
    "Coverage analysis": "覆盖率分析",
    "Code coverage": "代码覆盖率",
    "Test coverage": "测试覆盖率",
    "Coverage report": "覆盖率报告",
    "Coverage metrics": "覆盖率指标",
    "Line coverage": "行覆盖率",
    "Branch coverage": "分支覆盖率",
    "Function coverage": "函数覆盖率",

    # 静态分析
    "Static analysis": "静态分析",
    "Code analysis": "代码分析",
    "Lint checking": "代码检查",
    "Style checking": "风格检查",
    "MISRA checking": "MISRA 检查",
    "Cppcheck": "Cppcheck",
    "Clang-tidy": "Clang-tidy",

    # CI/CD
    "Continuous integration": "持续集成",
    "Continuous deployment": "持续部署",
    "CI pipeline": "CI 流水线",
    "Build pipeline": "构建流水线",
    "Test pipeline": "测试流水线",
    "Deployment pipeline": "部署流水线",

    # GitHub Actions
    "GitHub Actions": "GitHub Actions",
    "Workflow": "工作流",
    "Action": "动作",
    "Job": "作业",
    "Step": "步骤",
    "Runner": "运行器",
    "Trigger": "触发器",

    # 贡献指南
    "Contributing": "贡献",
    "Contribution guidelines": "贡献指南",
    "How to contribute": "如何贡献",
    "Pull request": "拉取请求",
    "Code review": "代码审查",
    "Commit message": "提交消息",
    "Coding style": "编码风格",
    "Code formatting": "代码格式化",

    # Git 工作流
    "Git workflow": "Git 工作流",
    "Branching strategy": "分支策略",
    "Feature branch": "特性分支",
    "Development branch": "开发分支",
    "Release branch": "发布分支",
    "Hotfix branch": "热修复分支",

    # 提交规范
    "Commit conventions": "提交规范",
    "Commit format": "提交格式",
    "Commit type": "提交类型",
    "Commit scope": "提交范围",
    "Commit subject": "提交主题",
    "Commit body": "提交正文",
    "Commit footer": "提交页脚",

    # 提交类型
    "feat": "新特性",
    "fix": "修复",
    "docs": "文档",
    "style": "风格",
    "refactor": "重构",
    "perf": "性能",
    "test": "测试",
    "build": "构建",
    "ci": "CI",
    "chore": "杂项",
    "revert": "回退",

    # 代码审查
    "Code review process": "代码审查流程",
    "Review checklist": "审查清单",
    "Review comments": "审查评论",
    "Approval": "批准",
    "Request changes": "请求更改",
    "Merge": "合并",

    # 测试
    "Unit testing": "单元测试",
    "Integration testing": "集成测试",
    "System testing": "系统测试",
    "Regression testing": "回归测试",
    "Performance testing": "性能测试",
    "Stress testing": "压力测试",

    # 测试框架
    "Test framework": "测试框架",
    "Test suite": "测试套件",
    "Test case": "测试用例",
    "Test fixture": "测试固件",
    "Test runner": "测试运行器",
    "Test report": "测试报告",

    # 测试断言
    "Assertion": "断言",
    "Assert equal": "断言相等",
    "Assert not equal": "断言不相等",
    "Assert true": "断言为真",
    "Assert false": "断言为假",
    "Assert null": "断言为空",
    "Assert not null": "断言非空",

    # 模拟和桩
    "Mocking": "模拟",
    "Mock object": "模拟对象",
    "Stub": "桩",
    "Fake": "伪造",
    "Spy": "间谍",
    "Test double": "测试替身",

    # 调试
    "Debugging": "调试",
    "Debug mode": "调试模式",
    "Debug output": "调试输出",
    "Debug symbols": "调试符号",
    "Breakpoint": "断点",
    "Watchpoint": "观察点",
    "Stack trace": "栈跟踪",

    # 性能分析
    "Profiling": "性能分析",
    "Performance profiling": "性能剖析",
    "CPU profiling": "CPU 剖析",
    "Memory profiling": "内存剖析",
    "Profiler": "剖析器",
    "Profile report": "剖析报告",

    # 内存分析
    "Memory analysis": "内存分析",
    "Memory leak": "内存泄漏",
    "Memory usage": "内存使用",
    "Heap analysis": "堆分析",
    "Stack analysis": "栈分析",
    "Memory sanitizer": "内存消毒器",

    # 工具链
    "Toolchain": "工具链",
    "Compiler": "编译器",
    "Linker": "链接器",
    "Debugger": "调试器",
    "Build system": "构建系统",
    "Package manager": "包管理器",

    # 文档工具
    "Documentation tools": "文档工具",
    "Doxygen": "Doxygen",
    "Sphinx": "Sphinx",
    "Markdown": "Markdown",
    "reStructuredText": "reStructuredText",
    "API documentation": "API 文档",
    "User documentation": "用户文档",

    # 版本控制
    "Version control": "版本控制",
    "Git": "Git",
    "Repository": "仓库",
    "Clone": "克隆",
    "Fork": "分叉",
    "Branch": "分支",
    "Tag": "标签",
    "Release": "发布",

    # 发布管理
    "Release management": "发布管理",
    "Release process": "发布流程",
    "Release notes": "发布说明",
    "Changelog": "更新日志",
    "Version numbering": "版本编号",
    "Semantic versioning": "语义化版本",

    # 质量保证
    "Quality assurance": "质量保证",
    "QA process": "QA 流程",
    "Quality metrics": "质量指标",
    "Code quality": "代码质量",
    "Test quality": "测试质量",
    "Documentation quality": "文档质量",

    # 最佳实践
    "Best practices": "最佳实践",
    "Coding standards": "编码标准",
    "Design patterns": "设计模式",
    "Anti-patterns": "反模式",
    "Code smells": "代码异味",
    "Refactoring": "重构",

    # 项目管理
    "Project management": "项目管理",
    "Issue tracking": "问题跟踪",
    "Bug tracking": "错误跟踪",
    "Feature request": "特性请求",
    "Milestone": "里程碑",
    "Sprint": "冲刺",
    "Backlog": "待办事项",

    # 协作
    "Collaboration": "协作",
    "Team workflow": "团队工作流",
    "Communication": "沟通",
    "Code ownership": "代码所有权",
    "Pair programming": "结对编程",
    "Code sharing": "代码共享",
}

def translate_entry(msgid):
    """翻译单个条目"""
    if not msgid or not msgid.strip():
        return ""

    msgid = msgid.strip()

    # 直接匹配
    if msgid in DEVELOPMENT_TRANSLATIONS:
        return DEVELOPMENT_TRANSLATIONS[msgid]

    # 不区分大小写匹配
    for key, value in DEVELOPMENT_TRANSLATIONS.items():
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

    # 处理 development 目录
    po_files = sorted(locale_dir.glob('development/*.po'))

    print(f"处理 {len(po_files)} 个开发指南文件...")

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
