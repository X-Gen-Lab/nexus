---
id: guides.app-configuration
type: how-to
audience: consumer
source_language: zh_CN
---

# 配置应用与增加 I/O

应用只维护一份 schema-2 `assembly.toml`。它选择 Board、后端、时钟、组件、实例及
预算；SoC／Board JSON 维护硬件事实，生成的 `resolved.json` 用于检查结果。

## 先查询，再填写

以启明 V3.1 的 UART 为例，设置 `NEXUS_SOURCE` 为当前平台路径，再查询：

```sh
python "$NEXUS_SOURCE/tools/configure/configure.py" list-bindings \
  --board stm32f407_qiming_v31 --kind uart
```

`console` 是应用自定的逻辑名字，`uart1` 是 Board 提供的 binding，USART1 是实际
控制器。查询给出候选模式、资源、来源和限制；后端、时钟及全部实例仍须一起校验。

已有工程从对应模板修改 `assembly.toml`。没有配置时，可创建一份独立 UART 配置：

```sh
python "$NEXUS_SOURCE/tools/configure/configure.py" init \
  --board stm32f407_qiming_v31 --backend baremetal --clock hse8-pll168 \
  --profile uart --binding uart1 --instance console \
  --output /tmp/product-uart.toml
```

`init` 拒绝覆盖已有文件。`empty`、`gpio`、`uart` 是有限起始配置，不自动增加任务、
调度器或产品恢复策略。先阅读生成的 TOML，再采用其中的容量和优先级。

## 检查有效配置

```sh
python "$NEXUS_SOURCE/tools/configure/configure.py" check \
  --assembly /tmp/product-uart.toml
python "$NEXUS_SOURCE/tools/configure/configure.py" explain \
  --assembly /tmp/product-uart.toml
```

`mode = "irq"` 与 `rx.format = "events"` 对应查询中的 `irq-byte-event`。
`rx.capacity = 32` 表示 32 个事件，不是 32 字节。`explain` 显示最终接口、引脚、
时钟、IRQ／DMA、内存域、容量和执行责任；编辑器 schema 仅提供字段提示。

修改后可比较两份有效配置：

```sh
python "$NEXUS_SOURCE/tools/configure/configure.py" diff \
  --assembly /tmp/product-uart.toml --against /tmp/product-uart-next.toml
```

两份输入都须有效。重点看 factory ID、公开接口、传输、执行和存储变化。调整实例
顺序可能改变紧凑 ID，不将它们持久化后跨 bundle 使用。

## 使用所选接口

生成 bundle 自带 `nexus_factory_strong.h`。`uart.console` 使用
`nx_factory_uart_ref(NX_UART_REF_CONSOLE)`；获取只返回既有静态对象，不启动硬件。
先成功启动 Platform，再按该模式提供 service、请求与缓冲存储。

只选择的类别才有 factory getter。可选组合用 `defined(NX_FACTORY_HAS_UART)` 判断
是否存在；`factory.strong` 不是配置字段。`factory.capabilities` 只选择独立能力元数据。

Native 配置查询需显式 `--test-support`。私有 Board 使用 `--board-package` 创建，
其相对路径随 TOML／产品目录移动。完整字段和单位见
[TOML 参考](../reference/configuration/assembly.md)，执行规则见
[I/O 执行](io-execution.md)，错误定位见[排错](troubleshooting.md)。
