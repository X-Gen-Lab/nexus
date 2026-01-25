# Kconfig 工具示例配置文件

本目录包含 Kconfig 命名规范系统的示例配置文件，演示如何使用批量生成和自定义外设模板功能。

## 文件说明

### batch_config.yaml

完整的批量生成配置示例，包含所有支持的外设类型（UART、GPIO、SPI、I2C、ADC、DAC、CRC、WATCHDOG）。

**使用方法**:
```bash
python scripts/kconfig_tools/cli.py batch-generate \
    -c scripts/kconfig_tools/examples/batch_config.yaml \
    -o platforms/native/src/
```

**特点**:
- 包含所有 8 种预定义外设类型
- 为 NATIVE 平台配置
- 包含详细的描述和注释
- 配置了全局选项（覆盖、验证、详细日志）

### stm32_config.yaml

STM32 平台的批量生成配置示例，展示如何为不同平台生成配置。

**使用方法**:
```bash
python scripts/kconfig_tools/cli.py batch-generate \
    -c scripts/kconfig_tools/examples/stm32_config.yaml \
    -o platforms/stm32/src/
```

**特点**:
- 针对 STM32 平台
- 更多的实例数量（如 6 个 UART，11 个 GPIO 端口）
- 适用于资源丰富的 MCU

### minimal_config.yaml

最小化配置示例，适用于快速原型开发或测试。

**使用方法**:
```bash
python scripts/kconfig_tools/cli.py batch-generate \
    -c scripts/kconfig_tools/examples/minimal_config.yaml \
    -o test_output/
```

**特点**:
- 只包含 UART 和 GPIO 两种外设
- 实例数量较少
- 启用覆盖模式，方便测试
- 简洁的配置，易于理解

### custom_peripheral.json

自定义外设模板示例（JSON 格式），展示如何定义新的外设类型。

**使用方法**:
```python
import json
from kconfig_tools import KconfigGenerator, PeripheralTemplate, ParameterConfig, ChoiceConfig

# 加载自定义模板
with open('scripts/kconfig_tools/examples/custom_peripheral.json', 'r') as f:
    template_data = json.load(f)

# 转换为 PeripheralTemplate 对象
template = PeripheralTemplate(
    name=template_data['name'],
    platform=template_data['platform'],
    max_instances=template_data['max_instances'],
    instance_type=template_data['instance_type'],
    parameters=[ParameterConfig(**p) for p in template_data['parameters']],
    choices=[ChoiceConfig(**c) for c in template_data['choices']],
    help_text=template_data['help_text']
)

# 生成 Kconfig 文件
generator = KconfigGenerator(template)
generator.generate_file('output/timer_kconfig')
```

**特点**:
- 定义了 TIMER 外设
- 包含 4 个参数配置
- 包含 3 个选择项配置
- JSON 格式，易于编辑和扩展

## 配置文件格式说明

### YAML 格式

```yaml
peripherals:
  - type: <外设类型>           # UART, GPIO, SPI, I2C, ADC, DAC, CRC, WATCHDOG
    platform: <平台名称>        # NATIVE, STM32, GD32 等
    instances: <实例数量>       # 整数
    output: <输出路径>          # 相对于输出目录的路径
    description: <描述>         # 可选，用于文档

options:
  overwrite: <true/false>      # 是否覆盖已存在的文件
  validate: <true/false>       # 是否在生成后自动验证
  verbose: <true/false>        # 是否输出详细日志
```

### JSON 格式（自定义外设模板）

```json
{
  "name": "外设名称",
  "platform": "平台名称",
  "max_instances": 实例数量,
  "instance_type": "numeric 或 alpha",
  "help_text": "外设帮助文本",
  "parameters": [
    {
      "name": "参数名称",
      "type": "int, hex, bool, 或 string",
      "default": 默认值,
      "range": [最小值, 最大值],  // 可选，仅用于 int/hex
      "help": "参数帮助文本"
    }
  ],
  "choices": [
    {
      "name": "选择项名称",
      "options": ["选项1", "选项2", "选项3"],
      "default": "默认选项",
      "help": "选择项帮助文本",
      "values": {
        "选项1": 0,
        "选项2": 1,
        "选项3": 2
      }
    }
  ]
}
```

## 快速开始

### 1. 生成单个外设

```bash
# 生成 UART 外设配置
python scripts/kconfig_tools/cli.py generate \
    -p UART -P NATIVE -n 4 \
    -o platforms/native/src/uart/Kconfig
```

### 2. 批量生成（使用示例配置）

```bash
# 使用完整配置
python scripts/kconfig_tools/cli.py batch-generate \
    -c scripts/kconfig_tools/examples/batch_config.yaml \
    -o platforms/native/src/

# 使用最小配置（测试）
python scripts/kconfig_tools/cli.py batch-generate \
    -c scripts/kconfig_tools/examples/minimal_config.yaml \
    -o test_output/
```

### 3. 验证生成的文件

```bash
# 验证单个文件
python scripts/kconfig_tools/cli.py validate \
    -f platforms/native/src/uart/Kconfig

# 批量验证目录
python scripts/kconfig_tools/cli.py batch-validate \
    -d platforms/native/src/ \
    -r validation_report.txt
```

## 自定义配置

### 创建自己的批量配置

1. 复制 `minimal_config.yaml` 作为起点
2. 根据需要添加或修改外设配置
3. 调整实例数量和输出路径
4. 运行批量生成命令

### 创建自定义外设模板

1. 复制 `custom_peripheral.json` 作为起点
2. 修改外设名称、参数和选择项
3. 使用 Python API 加载并生成

## 注意事项

1. **输出路径**: 配置文件中的 `output` 路径是相对于 `-o` 参数指定的输出目录的
2. **覆盖模式**: 设置 `overwrite: true` 会覆盖已存在的文件，请谨慎使用
3. **验证模式**: 建议始终启用 `validate: true`，确保生成的文件符合规范
4. **实例数量**: GPIO 使用字母标识（A-Z），最多支持 26 个实例；其他外设使用数字标识
5. **平台名称**: 平台名称会自动转换为大写，确保命名一致性

## 故障排除

**问题**: 批量生成失败，提示文件已存在

**解决**: 设置 `overwrite: true` 或删除已存在的文件

---

**问题**: 自定义外设模板加载失败

**解决**: 检查 JSON 格式是否正确，确保所有必需字段都已填写

---

**问题**: 生成的文件验证失败

**解决**: 检查模板配置是否符合命名规范，查看验证报告中的具体错误信息

## 更多示例

查看主 README 文档获取更多使用示例和 API 文档：
- `scripts/kconfig_tools/README.md`
