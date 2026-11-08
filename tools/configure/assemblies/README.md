# F407 软件装配参考

本目录只保留 F4 当前入口的 schema-2 TOML。产品在外部工程持有自己的装配、预算和
策略；这些参考不是默认产品。生产系列为 `stm32f4`，精确实现限定 F407VE／ZG。

| 文件族 | 精确选择与用途 |
| --- | --- |
| `qiming-*` | 启明 V3.1 F407ZGT6，固定 HSE8／PLL168 |
| `sky-*` | 天空星青春版 F407VET6，固定 HSE8／PLL168 |
| `*-empty` | 不选择 I/O，用于选择成本与消费基线 |
| `*-gpio`、`*-uart` | 明确 GPIO 或 IRQ byte/event console |
| `qiming-baremetal-uart-dma` | 受维护的有限 UART `dma-tx` |
| `*-i2s2-slave-tx` | 需要外部 CK／WS 的有限 I2S2 slave TX 软件选择 |

裸机与 FreeRTOS 分文件；实例 ID 来自装配，不持久化为 ABI。核心板和 V4.1 是独立
Board 包，不继承 V3.1 接线或实物资格，见[板卡入口](../../../boards/README.md)。
Native 参考位于 `tests/support/native/assemblies/`，必须显式启用测试支持，不作为
生产家族参考混入本目录。

```sh
python tools/configure/configure.py check \
  --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
cmake --preset stm32-qiming-baremetal
```

从仓库根运行；实际固件还需准备锁定工具和所选 SDK。配置通过、ARM 链接与软件模型
不证明供电、跳线、电平、负载、时序或物理 HIL。每份结果绑定精确源码与配置，旧候选
不随重构迁移。机器目录见 `catalogue.json`，真实能力见
[派生矩阵](../../../docs/delivery/capabilities.md)。
