# Kconfig 命名规范系统

Nexus 项目的 Kconfig 配置文件命名规范和自动化工具系统。

## 概述

本系统提供统一的 Kconfig 命名规范定义、自动化生成工具和验证工具，确保所有 Kconfig 配置文件遵循一致的命名约定，提高代码可维护性和可读性。

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                  Kconfig 命名规范系统                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │  命名规范库   │  │  生成器模块   │  │  验证器模块   │ │
│  │              │  │              │  │              │ │
│  │ - 规则定义   │  │ - 模板引擎   │  │ - 规则检查   │ │
│  │ - 模式匹配   │  │ - 参数配置   │  │ - 错误报告   │ │
│  │ - 格式化     │  │ - 文件生成   │  │ - 修复建议   │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
│         │                  │                  │        │
│         └──────────────────┴──────────────────┘        │
│                            │                           │
└────────────────────────────┼───────────────────────────┘
                             │
                    ┌────────┴────────┐
                    │  Kconfig 文件    │
                    └─────────────────┘
```

## 核心组件

### 1. 命名规范库 (NamingRules)

定义所有 Kconfig 配置符号的命名规则和模式：

- **平台级命名**: `{PLATFORM}_ENABLE`, `{PLATFORM}_PLATFORM_NAME`
- **外设级命名**: `{PLATFORM}_{PERIPHERAL}_ENABLE`, `{PLATFORM}_{PERIPHERAL}_MAX_INSTANCES`
- **实例级命名**: `INSTANCE_NX_{PERIPHERAL}_{N}`, `{PERIPHERAL}{N}_{PARAMETER}`
- **选择项命名**: `NX_{PERIPHERAL}{N}_{CATEGORY}_{OPTION}`, `{PERIPHERAL}{N}_{CATEGORY}_VALUE`

### 2. 生成器模块 (KconfigGenerator)

基于外设模板自动生成符合规范的 Kconfig 文件：

- 支持多种外设类型（UART、GPIO、SPI、I2C、ADC、DAC、CRC、Watchdog）
- 可配置参数和实例数量
- 自动生成完整的注释和帮助文本
- 支持批量生成

### 3. 验证器模块 (KconfigValidator)

检查现有 Kconfig 文件是否符合命名规范：

- 检查配置符号命名格式
- 检查选择项命名格式
- 检查文件结构完整性
- 生成详细的验证报告
- 提供修复建议

## 安装

本工具是 Nexus 项目的一部分，无需单独安装。确保已安装项目依赖：

```bash
pip install -r requirements.txt
```

## 使用说明

### 命令行工具

系统提供命令行接口 `cli.py`，支持以下命令：

#### 1. 生成 Kconfig 文件

```bash
# 生成单个外设的 Kconfig 文件
python scripts/kconfig_tools/cli.py generate \
    --peripheral UART \
    --platform NATIVE \
    --instances 4 \
    --output platforms/native/src/uart/Kconfig

# 使用简短参数
python scripts/kconfig_tools/cli.py generate -p SPI -P NATIVE -n 2 -o output.kconfig
```

**参数说明**:
- `--peripheral, -p`: 外设类型（UART, GPIO, SPI, I2C, ADC, DAC, CRC, WATCHDOG）
- `--platform, -P`: 平台名称（NATIVE, STM32, GD32 等）
- `--instances, -n`: 实例数量
- `--output, -o`: 输出文件路径

#### 2. 批量生成 Kconfig 文件

```bash
# 从配置文件批量生成
python scripts/kconfig_tools/cli.py batch-generate \
    --config examples/batch_config.yaml \
    --output-dir platforms/native/src/

# 使用简短参数
python scripts/kconfig_tools/cli.py batch-generate -c config.yaml -o output/
```

**配置文件格式** (YAML):
```yaml
peripherals:
  - type: UART
    platform: NATIVE
    instances: 4
    output: uart/Kconfig
  - type: GPIO
    platform: NATIVE
    instances: 3
    output: gpio/Kconfig
```

#### 3. 验证 Kconfig 文件

```bash
# 验证单个文件
python scripts/kconfig_tools/cli.py validate \
    --file platforms/native/src/uart/Kconfig

# 验证目录下所有文件
python scripts/kconfig_tools/cli.py validate \
    --directory platforms/native/src/

# 使用简短参数
python scripts/kconfig_tools/cli.py validate -f file.kconfig
python scripts/kconfig_tools/cli.py validate -d platforms/
```

**验证报告示例**:
```
Validation Report
=================

File: platforms/native/src/uart/Kconfig
----------------------------------------
✓ No issues found

File: platforms/native/src/gpio/Kconfig
----------------------------------------
✗ 2 issues found:

[ERROR] Line 15: Invalid naming format
  Symbol: GPIO_A_ENABLE
  Expected: INSTANCE_NX_GPIOA
  Suggestion: Rename to follow instance naming pattern

[WARNING] Line 42: Missing help text
  Symbol: GPIOA_PIN0_MODE
  Suggestion: Add help text to describe the configuration option
```

#### 4. 批量验证 Kconfig 文件

```bash
# 递归验证目录下所有 Kconfig 文件
python scripts/kconfig_tools/cli.py batch-validate \
    --directory platforms/ \
    --report validation_report.txt

# 使用简短参数
python scripts/kconfig_tools/cli.py batch-validate -d platforms/ -r report.txt
```

### Python API

#### 生成 Kconfig 文件

```python
from kconfig_tools import KconfigGenerator, PeripheralTemplate, ParameterConfig, ChoiceConfig

# 定义外设模板
template = PeripheralTemplate(
    name="UART",
    platform="NATIVE",
    max_instances=4,
    instance_type="numeric",
    parameters=[
        ParameterConfig("BAUDRATE", "int", 115200, help="波特率"),
        ParameterConfig("TX_BUFFER_SIZE", "int", 256, (16, 4096), "发送缓冲区大小"),
    ],
    choices=[
        ChoiceConfig("PARITY", ["NONE", "ODD", "EVEN"], "NONE", 
                    "校验位", {"NONE": 0, "ODD": 1, "EVEN": 2}),
    ],
    help_text="UART 串口外设配置"
)

# 创建生成器并生成文件
generator = KconfigGenerator(template)
generator.generate_file("output/uart_kconfig")
```

#### 验证 Kconfig 文件

```python
from kconfig_tools import KconfigValidator, NamingRules

# 创建验证器
validator = KconfigValidator(NamingRules())

# 验证单个文件
issues = validator.validate_file("platforms/native/src/uart/Kconfig")
for issue in issues:
    print(f"[{issue.severity.upper()}] Line {issue.line}: {issue.message}")
    if issue.suggestion:
        print(f"  Suggestion: {issue.suggestion}")

# 验证目录
all_issues = validator.validate_directory("platforms/native/src/")
report = validator.generate_report(all_issues)
print(report)
```

## 命名规范详解

### 平台级配置符号

| 类型 | 格式 | 示例 |
|------|------|------|
| 平台使能 | `{PLATFORM}_ENABLE` | `NATIVE_ENABLE` |
| 平台特性 | `{PLATFORM}_{FEATURE}_ENABLE` | `NATIVE_UART_ENABLE` |
| 平台名称 | `{PLATFORM}_PLATFORM_NAME` | `NATIVE_PLATFORM_NAME` |
| 平台版本 | `{PLATFORM}_PLATFORM_VERSION` | `NATIVE_PLATFORM_VERSION` |

### 外设级配置符号

| 类型 | 格式 | 示例 |
|------|------|------|
| 外设使能 | `{PLATFORM}_{PERIPHERAL}_ENABLE` | `NATIVE_UART_ENABLE` |
| 最大实例数 | `{PLATFORM}_{PERIPHERAL}_MAX_INSTANCES` | `NATIVE_UART_MAX_INSTANCES` |
| 全局配置 | `{PLATFORM}_{PERIPHERAL}_{PARAMETER}` | `NATIVE_UART_CLOCK_SOURCE` |

### 实例级配置符号

| 类型 | 格式 | 示例 |
|------|------|------|
| 实例使能 | `INSTANCE_NX_{PERIPHERAL}_{N}` | `INSTANCE_NX_UART_0` |
| 实例参数 | `{PERIPHERAL}{N}_{PARAMETER}` | `UART0_BAUDRATE` |
| 选择项选项 | `NX_{PERIPHERAL}{N}_{CATEGORY}_{OPTION}` | `NX_UART0_PARITY_NONE` |
| 选择项值 | `{PERIPHERAL}{N}_{CATEGORY}_VALUE` | `UART0_PARITY_VALUE` |

### 特殊规则

#### GPIO 命名

GPIO 端口使用字母标识，引脚使用 PIN 前缀：

```
INSTANCE_NX_GPIOA          # GPIO A 端口使能
GPIOA_PIN0_MODE            # GPIO A 端口 PIN0 模式
NX_GPIOA_PIN0_MODE_INPUT   # GPIO A 端口 PIN0 输入模式选项
```

## 支持的外设类型

系统预定义了以下外设模板：

| 外设 | 实例类型 | 主要参数 | 主要选择项 |
|------|----------|----------|------------|
| UART | 数字 (0-3) | 波特率、数据位、停止位、缓冲区大小 | 校验位、传输模式 |
| GPIO | 字母 (A-C) | 输出值 | 模式、上下拉、速度 |
| SPI | 数字 (0-1) | 最大速度、缓冲区大小 | 时钟极性、时钟相位、位顺序 |
| I2C | 数字 (0-1) | 缓冲区大小 | 速度模式、寻址模式 |
| ADC | 数字 (0-1) | 通道数、分辨率、采样时间 | 触发模式、对齐方式 |
| DAC | 数字 (0-1) | 通道数、分辨率 | 触发模式、输出缓冲 |
| CRC | 数字 (0) | 多项式、初始值 | 算法类型 |
| WATCHDOG | 数字 (0) | 超时时间 | 窗口模式 |

## 文件结构规范

生成的 Kconfig 文件遵循以下结构：

```kconfig
# ============================================================================
# {Peripheral} Configuration for {Platform}
# ============================================================================

# {Peripheral} Enable
config {PLATFORM}_{PERIPHERAL}_ENABLE
    bool "Enable {Peripheral}"
    default n
    help
      Enable {Peripheral} peripheral support

if {PLATFORM}_{PERIPHERAL}_ENABLE

# Global Configuration
config {PLATFORM}_{PERIPHERAL}_MAX_INSTANCES
    int "Maximum number of {Peripheral} instances"
    default {max_instances}
    help
      Maximum number of {Peripheral} instances

# Instance 0 Configuration
menuconfig INSTANCE_NX_{PERIPHERAL}_0
    bool "Enable {Peripheral} Instance 0"
    default n
    help
      Enable {Peripheral} instance 0

if INSTANCE_NX_{PERIPHERAL}_0
    # Parameters and choices...
endif

# Instance 1 Configuration
# ...

endif
```

## 示例

查看 `examples/` 目录获取更多使用示例：

- `generate_all_peripherals.py` - 生成所有外设类型的示例
- `validate_project.py` - 验证项目所有 Kconfig 文件的示例
- `batch_config.yaml` - 批量生成配置文件示例
- `custom_peripheral.json` - 自定义外设模板示例

## 测试

运行测试套件：

```bash
# 运行所有测试
pytest tests/kconfig/

# 运行特定测试
pytest tests/kconfig/test_naming_rules_platform_properties.py
pytest tests/kconfig/test_generator_file_properties.py
pytest tests/kconfig/test_validator_naming_properties.py

# 运行属性测试
pytest tests/kconfig/ -k "properties"

# 查看测试覆盖率
pytest tests/kconfig/ --cov=scripts/kconfig_tools --cov-report=html
```

## 故障排除

### 常见问题

**Q: 生成的文件无法被 Kconfig 工具解析**

A: 确保使用的外设模板参数有效，检查生成的文件语法。可以使用验证器检查：
```bash
python scripts/kconfig_tools/cli.py validate -f generated_file.kconfig
```

**Q: 验证器报告大量错误**

A: 这可能是因为现有文件使用了旧的命名规范。查看验证报告中的修复建议，逐步更新文件。

**Q: 如何添加自定义外设类型**

A: 创建自定义 `PeripheralTemplate` 并使用 Python API 生成：
```python
from kconfig_tools import KconfigGenerator, PeripheralTemplate, ParameterConfig

custom_template = PeripheralTemplate(
    name="CUSTOM",
    platform="NATIVE",
    max_instances=2,
    instance_type="numeric",
    parameters=[...],
    choices=[...],
    help_text="Custom peripheral"
)

generator = KconfigGenerator(custom_template)
generator.generate_file("output/custom.kconfig")
```

## 贡献

欢迎贡献！请遵循以下步骤：

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

## 许可证

Copyright (c) 2026 Nexus Team

## 联系方式

- 项目主页: [Nexus Project](https://github.com/nexus-project)
- 问题反馈: [GitHub Issues](https://github.com/nexus-project/issues)
- 文档: [Nexus Documentation](https://nexus-project.readthedocs.io)

## 版本历史

### v1.0.0 (2026-01-20)
- 初始版本发布
- 支持 8 种外设类型
- 命名规范库、生成器和验证器模块
- 命令行工具和 Python API
- 完整的测试套件和文档
