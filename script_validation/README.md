# Script Validation System

脚本交付验证系统 - 一个全面的跨平台脚本验证框架，专为 Nexus 嵌入式系统项目设计。

## 概述

该系统自动化验证所有项目脚本在 Windows、WSL 和 Linux 环境中的功能性、兼容性和可靠性。

## 功能特性

- **跨平台验证**: 支持 Windows、WSL、Linux 三大平台
- **多种验证器**: 功能验证、兼容性验证、性能验证、文档验证
- **多格式报告**: HTML、JSON、Summary、JUnit XML
- **CI/CD 集成**: 支持 GitHub Actions、GitLab CI、Jenkins、Azure DevOps 等
- **灵活配置**: 支持命令行参数和配置文件

## 目录结构

```
script_validation/
├── adapters/           # 平台适配器 (Windows/WSL/Linux)
├── controllers/        # 验证控制器
├── handlers/           # 错误处理和资源管理
├── managers/           # 脚本和平台管理器
├── reporters/          # 报告生成器 (HTML/JSON/JUnit/Summary)
├── validators/         # 验证器 (功能/兼容性/性能/文档)
├── __init__.py         # 模块入口
├── __main__.py         # CLI 入口点
├── ci_integration.py   # CI/CD 集成
├── discovery.py        # 脚本发现
├── integration.py      # 组件集成
├── interfaces.py       # 接口定义
└── models.py           # 数据模型
```

## 快速开始

### 命令行使用

```bash
# 完整验证
python -m script_validation --mode full

# 快速验证
python -m script_validation --mode quick

# 指定平台验证
python -m script_validation --platforms windows wsl

# 生成特定格式报告
python -m script_validation --report-format html json

# CI 模式
python -m script_validation --ci

# 生成 JUnit XML 报告
python -m script_validation --ci --report-format junit

# 列出发现的脚本
python -m script_validation --list-scripts

# 检查平台可用性
python -m script_validation --check-platforms
```

### 编程接口

```python
from script_validation import (
    create_workflow,
    run_validation,
    discover_scripts,
    check_platform_availability,
    Platform,
    ValidationBuilder
)

# 方式1: 使用便捷函数
report = run_validation(mode='full')

# 方式2: 使用工作流程
workflow = create_workflow(
    platforms=[Platform.WINDOWS, Platform.WSL],
    validators=['functional', 'compatibility'],
    report_formats=['html', 'json']
)
report = workflow.run()

# 方式3: 使用构建器模式
workflow = (ValidationBuilder()
    .root_path(Path('./'))
    .platforms(Platform.WINDOWS, Platform.LINUX)
    .validators('functional', 'performance')
    .report_formats('html', 'junit')
    .timeout(600)
    .ci_mode(True)
    .build())
report = workflow.run()
```

## 命令行参数

| 参数 | 简写 | 说明 | 默认值 |
|------|------|------|--------|
| `--root-path` | `-r` | 项目根目录路径 | 当前目录 |
| `--mode` | `-m` | 验证模式: full/quick/platform-specific | full |
| `--platforms` | `-p` | 目标平台: windows/wsl/linux | 所有可用 |
| `--report-format` | `-f` | 报告格式: html/json/summary/junit/all | all |
| `--output-dir` | `-o` | 报告输出目录 | ./validation_reports |
| `--validators` | `-v` | 验证器: functional/compatibility/performance/documentation | 全部 |
| `--timeout` | | 脚本执行超时(秒) | 300 |
| `--max-memory` | | 最大内存限制(MB) | 1024 |
| `--ci` | | CI 模式 | false |
| `--verbose` | | 详细输出 | false |
| `--no-parallel` | | 禁用并行执行 | false |

## CI/CD 集成

系统自动检测 CI 环境并调整输出格式：

- **GitHub Actions**: 使用 `::error::` 和 `::warning::` 注解
- **GitLab CI**: 使用折叠区段
- **Azure DevOps**: 使用 `##vso` 命令
- **Jenkins**: 标准输出格式

### 退出代码

| 代码 | 含义 |
|------|------|
| 0 | 验证成功 |
| 1 | 验证失败 |
| 2 | 执行错误 |

### GitHub Actions 示例

```yaml
- name: Validate Scripts
  run: python -m script_validation --ci --report-format junit
  
- name: Upload Report
  uses: actions/upload-artifact@v3
  with:
    name: validation-report
    path: validation_reports/
```

## 测试

```bash
# 运行所有测试
python -m pytest tests/script_validation/ -v

# 运行属性测试
python -m pytest tests/script_validation/ -v --hypothesis-show-statistics

# 生成覆盖率报告
python -m pytest tests/script_validation/ --cov=script_validation --cov-report=html
```

详细测试文档请参阅 [docs/script_validation_tests.md](../docs/script_validation_tests.md)

## 版本

- 当前版本: 1.0.0
- 作者: Nexus Team
