# STM32F1 软件参考

本包以 ST 的 STM32CubeF1 v1.8.7 配对为基础。当前软件目标为
STM32F103RBT6（LQFP64，128 KiB Flash／20 KiB SRAM）和 STM32F103C8T6
（LQFP48，64 KiB Flash／20 KiB SRAM）。两者选择同一 STM32F103xB 官方
器件头和启动向量，容量和可用 pad 分别校验。实板状态为 `not_executed`。

| 目录 | 职责 |
| --- | --- |
| `facts/` | 订货号、封装、IRQ、时钟、板可选路线、手册及勘误 |
| `system/` | 官方启动入口、HSI8、TIM2 单调时基、冷态取得和停止 |
| `drivers/` | 按选定模式编译的 C 驱动、静态实例、只读方法表 |
| `sdk/` | 私有 CMSIS Device 与 LL 头文件、锁定来源检查 |

板卡通过 [NUCLEO-F103RB](../../../boards/stm32f103rb_nucleo/README.md) 的
`board.json` 提供授权接线。配置工具解析 TOML，生成静态连接和强类型
factory。应用用相同的公共 Cortex-M、GPIO、UART 和 OS 接口消费装配。

HSI8 是名义复位输入，当前装配保持此时钟域。TIM2 为 16 位计数器。
1 MHz 计数时必须在 65.536 ms 内服务溢出，包含 IRQ 屏蔽与其它会
延迟服务的硬件作用。独占时基与 FreeRTOS CM3 的 SysTick 分开。
Flash-size 签名只用于容量检查。ES096 §2.2.2 的 DBGMCU 寄存器限制
使普通固件无法从零读数证明硅版本。

| 软件装配 | 可用接口与限制 |
| --- | --- |
| NUCLEO-F103RB 裸机和 FreeRTOS | PA5 逻辑输出，PA2／PA3 USART2 8N1 IRQ，字节或事件接收，显式停止和排空 |
| F103C8 裸机软件参考 | 相同的 GPIO 输出与串口，加 PA0 上拉输入。受精确封装和 Flash 容量约束 |
| F103RB 独立 [Flash 装配](../../../tests/integration/assemblies/stm32f1_flash/README.md) | 1 KiB 页，2 B 编程，显式板级编程电压，有界轮询及失败后的排空与重锁 |

启动器使用 43 个外部 IRQ。链接验证独立校验官方向量保留项和 BootRAM 尾部。
当前只有 HSI8。DMA、串口重映射、其它外设、其它 F1 料号和板卡待逐项加入。

官方源码准备复用两个已有组织子模块。应用可使用源码或显式导出的 F1
Source SDK，并为每个 Board／后端建立独立构建目录。软件参考与命令见
[独立消费者](../../../tests/integration/assemblies/stm32f1_family/README.md)。
