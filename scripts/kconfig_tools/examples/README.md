# Kconfig 示例

这些示例调用现有 Python 模板 API，生成供审核的外设 schema 草稿。模板生成、产品配置解析、驱动实现和板卡验证分别提供对应证据；生成一个 SPI 或 TIMER 文件不会自动增加硬件支持。输出放在专用目录，已有文件会明确拒绝覆盖。

从仓库根执行：

```sh
# 生成八种内置模板的草稿。
python3 scripts/kconfig_tools/examples/generate_all_peripherals.py NATIVE build/kconfig-examples/all

# 只生成两个 UART 实例，实例数超出模板上限时失败。
python3 scripts/kconfig_tools/examples/generate_all_peripherals.py NATIVE build/kconfig-examples/uart --peripheral UART --instances 2

# 使用 Python 或本目录的 JSON 数据生成自定义 TIMER 草稿。
python3 scripts/kconfig_tools/examples/custom_peripheral_example.py code NATIVE build/kconfig-examples/timer-code
python3 scripts/kconfig_tools/examples/custom_peripheral_example.py json scripts/kconfig_tools/examples/custom_peripheral.json NATIVE build/kconfig-examples/timer-json

# 对草稿执行命名 lint，读取失败、空目录或 error 均返回非零。
python3 scripts/kconfig_tools/examples/validate_project.py build/kconfig-examples/all --report build/kconfig-examples/naming-report.txt

# 交互菜单：0 返回/退出；执行失败传播为进程失败。
python3 scripts/kconfig_tools/examples/quick_start.py
```

所有脚本支持 `--help`。`quick_start.py` 将参数列表交给 `subprocess.run()`；文件路径中的空格或 shell 字符保持为普通参数。外设/文件选择检查正整数范围，实例数检查正整数，非法输入不会索引到列表末项。

八种内置模板的生成物通过命名 lint。每实例 choice 的隐藏整数 VALUE 必须以完整选项符号建立条件默认值；错误实例、类别名称或缺失映射均导致失败。静态枚举 choice 无需生成每实例 VALUE。

命名 lint 只检查 schema 的命名结构。产品 fragment 使用维护中的 Kconfig 解析器验证，未知符号、冲突或依赖问题导致失败：

```sh
python3 scripts/nexus_config.py validate --config configs/stm32f407_baremetal_defconfig
python3 scripts/nexus_config.py generate --config configs/stm32f407_baremetal_defconfig --build-dir build/profile-inspection
cmake --preset stm32-armgcc-debug
```

维护中的 STM32 reference 由明确的 SoC、板卡和构建 profile 选择。`stm32_config.yaml`、`batch_config.yaml` 和 `minimal_config.yaml` 保留为模板数据示例，不要将其输出直接覆盖已经审核的 `platforms/*` schema。JSON 模式使用 `custom_peripheral.json`；参数的 `range` 数组在传入 API 前转换为 tuple。
