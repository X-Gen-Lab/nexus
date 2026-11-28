# STM32F1 软件参考

本包以 ST 的 STM32CubeF1 v1.8.7 配对为基础，按精确料号选取 CMSIS Device
头文件、启动向量、封装 pad 和链接内存。所有参考的实板状态为 `not_executed`。

| 料号 | 厂商分组 | Flash／SRAM | CPU 能力 |
| --- | --- | --- | --- |
| STM32F103C6T6A | x6 | 32／10 KiB | Cortex-M3 |
| STM32F103C8T6 | xB | 64／20 KiB | Cortex-M3 |
| STM32F103RBT6 | xB | 128／20 KiB | Cortex-M3 |
| STM32F103ZET6 | xE | 512／64 KiB | Cortex-M3 |
| STM32F103ZGT6 | xG | 1024／96 KiB | Cortex-M3、MPU v7 八区域 |

F100、F101、F102、F105 和 F107 的基础软件参考共用 Cortex-M3、HSI8、TIM2、
GPIO 和 USART2 IRQ。精确料号、外部 IRQ、内存、编译宏与授权路线分别取自
[器件事实](facts/soc.json)、[CMSIS 视图](facts/devices.json)和
[软件装配](../../../tests/integration/assemblies/stm32f1_family/README.md)。
额外外设能力按精确器件和模式单独接入。

| 目录 | 职责 |
| --- | --- |
| `facts/` | 订货号、封装、IRQ、时钟、板可选路线、手册及勘误 |
| `system/` | 官方启动入口、时钟切换、TIM2 单调时基、冷态取得和停止 |
| `drivers/` | 按选定模式编译的 C 驱动、静态实例、只读方法表 |
| `sdk/` | 私有 CMSIS Device 与 LL 头文件、锁定来源检查 |

板卡通过 [NUCLEO-F103RB](../../../boards/stm32f103rb_nucleo/README.md) 的
`board.json` 提供授权接线。配置工具解析 TOML，生成静态连接和强类型
factory。应用用相同的公共 Cortex-M、GPIO、UART 和 OS 接口消费装配。

HSI8 是名义复位输入。RB 的独立 MCO 软件参考允许 8 MHz 旁路输入、PLL72 和 CSS。
实际板卡须按 MB1136 修订核对焊桥。TIM2 为 16 位计数器。
1 MHz 计数时必须在 65.536 ms 内服务溢出，包含 IRQ 屏蔽与其它会
延迟服务的硬件作用。独占时基与 FreeRTOS CM3 的 SysTick 分开。
Flash-size 签名只用于容量检查。ES096 §2.2.2 的 DBGMCU 寄存器限制
使普通固件无法从零读数证明硅版本。

| 软件装配 | 可用接口与限制 |
| --- | --- |
| NUCLEO-F103RB 裸机和 FreeRTOS | PA5 逻辑输出，PA2／PA3 USART2 8N1 IRQ，字节或事件接收，显式停止和排空 |
| C6／ZE／ZG 裸机软件参考 | HSI8、GPIO 输出与 USART2 IRQ。Flash 写擦和高阶外设尚未在这些料号开放 |
| F103C8 裸机软件参考 | GPIO 输出、PA0 上拉输入、USART2 IRQ、硬件 CRC32 字与 96 位 UID |
| F103RB 有限 DMA TX | DMA1 通道 7 送出有限 TX，RX 仍走 IRQ。先排空通道与 IRQ，再等 USART 真实 TC 结算 |
| F103RB 独立 [Flash 装配](../../../tests/integration/assemblies/stm32f1_flash/README.md) | 1 KiB 页，2 B 编程，显式板级编程电压，有界轮询及失败后的排空与重锁 |
| F103RB 独立 [IWDG 装配](../../../tests/integration/assemblies/stm32f1_watchdog/README.md) | 先确认 LSI 与寄存器同步。发出启动密钥后保留喂狗与关闭责任，直到复位 |
| F103RB [组合总线](../../../tests/integration/assemblies/stm32f1_buses/README.md) | HSI8，SPI2 短轮询、8／16 位事务，I²C1 七位 80 kHz 轮询和 repeated START |

x6／xB 的 IRQ 槽数为 43，xE／xG 为 60。链接验证单独校验官方向量保留项与 BootRAM 尾部。
具体模式以装配和能力矩阵为准。

官方源码准备复用两个已有组织子模块。应用可使用源码或显式导出的 F1
Source SDK，并为每个 Board／后端建立独立构建目录。软件参考与命令见
[独立消费者](../../../tests/integration/assemblies/stm32f1_family/README.md)。
