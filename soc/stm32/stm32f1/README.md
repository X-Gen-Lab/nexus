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
实际板卡须按 MB1136 修订核对焊桥。F105VC 和 F107VC 各有独立的 25 MHz 晶体
软件装配，名义 PLL2 40 MHz、HCLK 72 MHz、APB1 36 MHz 和 OTG 时钟 48 MHz。
高频档先确认时钟和 Flash 等待周期，再启动时基。关闭先切回 HSI，再撤回 PLL。
F101RB 的独立 8 MHz 晶体软件参考使用 PLL36。F102RB 的对应配置使用 PLL48，
APB1 为 24 MHz，OTG 门控保持关闭。
TIM2 为 16 位计数器。
1 MHz 计数时必须在 65.536 ms 内服务溢出，包含 IRQ 屏蔽与其它会
延迟服务的硬件作用。独占时基与 FreeRTOS CM3 的 SysTick 分开。
Flash-size 签名只用于容量检查。ES096 §2.2.2 的 DBGMCU 寄存器限制
使普通固件无法从零读数证明硅版本。

| 软件装配 | 可用接口与限制 |
| --- | --- |
| NUCLEO-F103RB 裸机和 FreeRTOS | PA5 逻辑输出，PA2／PA3 USART2 8N1 IRQ，字节或事件接收，显式停止和排空 |
| F103RB [USART2 单线半双工](../../../tests/integration/assemblies/stm32f1_uart_half_duplex/README.md) | HSI8、PA2 开漏、移交 TX/RX，外部上拉与产品仲裁 |
| F103RB [USART2 LIN](../../../tests/integration/assemblies/stm32f1_uart_lin/README.md) | HSI8、PA2／PA3、硬件 break 和事件接收、外部收发器 |
| F103RB [USART2 RTS/CTS](../../../tests/integration/assemblies/stm32f1_uart_flow/README.md) | HSI8、PA0／PA1 流控、PA2／PA3 数据，保留阻塞的发送与接收事件 |
| F103RB [USART2 IrDA SIR](../../../tests/integration/assemblies/stm32f1_uart_irda/README.md) | HSI8、PA2／PA3、普通功耗脉冲、10 ms 接收周转 |
| F103RB [USART2 同步主机](../../../tests/integration/assemblies/stm32f1_uart_sync/README.md) | PA2／PA3／PA4，独占 SPI1 资源，按配置控制 CK 极性和相位 |
| F103RB [USART2 字接口](../../../tests/integration/assemblies/stm32f1_uart9n1/README.md) | HSI8、PA2／PA3、独立 9N1 IRQ 和 16 位字容器 |
| C6／ZE／ZG 裸机软件参考 | HSI8、GPIO 输出与 USART2 IRQ。ZE 和 ZG 的独立 Flash 装配另列 |
| F103C8 裸机软件参考 | GPIO 输出、PA0 上拉输入、USART2 IRQ、硬件 CRC32 字与 96 位 UID |
| F103RB 有限 DMA TX | DMA1 通道 7 送出有限 TX，RX 仍走 IRQ。先排空通道与 IRQ，再等 USART 真实 TC 结算 |
| F103RB [有限 DMA RX](../../../tests/integration/assemblies/stm32f1_uart_rx_dma/README.md) | PA3、DMA1 通道 6、调用者持有流块。按已观测 IDLE、满块或显式停止结算 |
| F103C6T6A、F100RBT6B [Flash 装配](../../../tests/integration/assemblies/stm32f1_flash/README.md) | 分别为 32 KiB 和 128 KiB 单 Bank。复用已审查的 1 KiB 页擦除、2 B 编程以及延迟 BSY 处理。用板级配置声明编程电压 |
| F103RB 独立 [Flash 装配](../../../tests/integration/assemblies/stm32f1_flash/README.md) | 1 KiB 页，2 B 编程，显式板级编程电压，有界轮询及失败后的排空与重锁 |
| F103ZE 独立 [Flash 装配](../../../tests/integration/assemblies/stm32f1_flash/README.md) | 512 KiB 单 Bank，2 KiB 页，2 B 编程 |
| F103ZG 独立 [Flash 装配](../../../tests/integration/assemblies/stm32f1_flash/README.md) | 两个 512 KiB Bank，2 KiB 页，单次写入或擦除限于一个物理 Bank |
| F103RB 独立 [IWDG 装配](../../../tests/integration/assemblies/stm32f1_watchdog/README.md) | 先确认 LSI 与寄存器同步。发出启动密钥后保留喂狗与关闭责任，直到复位 |
| F103RB [SPI1／SPI2 双控制器](../../../tests/integration/assemblies/stm32f1_spi_dual/README.md) | 两条默认引脚路线，独立片选与轮询事务 |
| F103RB [组合总线](../../../tests/integration/assemblies/stm32f1_buses/README.md) | HSI8，SPI2 短轮询、8／16 位事务，I²C1 七位 80 kHz 轮询和 repeated START |
| F103RB [I²C1 7 位与 10 位端点](../../../tests/integration/assemblies/stm32f1_i2c_10bit/f103rb-i2c-10bit.toml) | HSI8，PB6/PB7，80 kHz。两种地址共用一个控制器和有界轮询事务 |
| F103RB [ADC1](../../../tests/integration/assemblies/stm32f1_adc/README.md) | PA0、4 MHz ADC 时钟，校准后的规则单次 12 位采样 |
| F103RB [ADC1 定时 DMA 单次采样](../../../tests/integration/assemblies/stm32f1_adc/README.md) | 两个独立装配分别占用 TIM3 TRGO 或 TIM1 CC1。PA0、DMA1 通道 1，每次一个样本，任务确认资源撤回 |
| F103ZE [双 ADC 同步扫描](../../../tests/integration/assemblies/stm32f1_adc/README.md#stm32f103zet6-双-adc-常规同步模式) | PA1／PA2、4 MHz ADC 时钟、两路同时触发，DMA1 通道 1 取得一次打包结果 |
| F103RB [TIM3](../../../tests/integration/assemblies/stm32f1_pwm/README.md) | PA6、CH1、8 MHz 定时器，边沿对齐 PWM 和明确的停止电平 |
| F100RB [TIM15／16／17](../../../tests/integration/assemblies/stm32f1_pwm/README.md) | PA2／PB8／PB9，三路独立固定 PWM，显式停止电平 |
| F100RB [STM32VLDISCOVERY MB913 C.02](../../../boards/stm32vldiscovery_mb913_c02/README.md) | HSI8、PA0 用户按键、PC8 和 PC9 LED |
| F103RB [TIM4 捕获](../../../tests/integration/assemblies/stm32f1_timer_capture/README.md) | PB6、CH1 上升沿，1 MHz 计数，调用者提供事件存储，EXTI6 辅助识别损失 |
| F103RB [TIM3 正交编码器](../../../tests/integration/assemblies/stm32f1_encoder/README.md) | PA6／PA7，x4、16 位模计数，独立相位滤波，停止保留计数 |
| F103RB [TIM1 互补 PWM](../../../tests/integration/assemblies/stm32f1_advanced_pwm/README.md) | PA8／PB13，PB12 低有效硬件刹车，独立恢复和重新启动 |
| F103RB [PWM/ADC 同步采样](../../../tests/integration/assemblies/stm32f1_pwm_adc/README.md) | TIM1 CH1 互补输出与硬件刹车，内部 CC4 触发 ADC1 注入转换，每个窗口采一个样本 |
| F103ZE [TIM8 互补 PWM](../../../tests/integration/assemblies/stm32f1_advanced_pwm/README.md#stm32f103ze-tim8) | PC6／PA7，PA6 低有效硬件刹车，独立恢复和重新启动 |
| F103RB [按键 EXTI](../../../tests/integration/assemblies/stm32f1_exti/README.md) | B1 PC13 的独立软件装配，原始电平与边沿事件 |
| F103RB [STOP 唤醒](../../../tests/integration/assemblies/stm32f1_stop/README.md) | 裸机 HSI8、PC13 下降沿，进入前排空旧期限，唤醒后重建时基 |
| F103RB [PVD](../../../tests/integration/assemblies/stm32f1_pvd/README.md) | 选定阈值编码和轮询电平，启动与关闭管理 PWR 门控 |
| F103RB [ThreadX 软件参考](../../../tests/integration/kernels/os_threadx_runtime/README.md) | Cortex-M3 soft-float，HSI8，1 kHz SysTick 和 BASEPRI |
| F103RB [窗口看门狗](../../../tests/integration/assemblies/stm32f1_window_watchdog/README.md) | HSI8，应用显式激活和刷新，保持不可逆启动责任 |
| F103RB [偏移镜像交接](../../../tests/integration/assemblies/stm32f1_handoff/README.md) | 可选 Boot 目标，实际 Platform 排空、掩码交接及偏移应用的 ARM／SDK 双镜像验证 |
| F103RB [USB FS PMA Device](../../../tests/integration/assemblies/stm32f1_pma_device/README.md) | HSE72 裸机自供电参考，独立 VBUS／D+ 上拉，64 B EP0 IN／OUT，LP20 与 USB／CAN 共用 PMA 排他 |
| F103RB [USB FS PMA EP1 Bulk](../../../tests/integration/assemblies/stm32f1_pma_device/README.md#f103rb-ep1-装配) | 显式启用 EP1 IN／OUT，方向独立各持有一包，64 B，复用 EP0 与共用 PMA 排他 |
| F103RB [USB FS PMA EP1 Interrupt](../../../tests/integration/assemblies/stm32f1_pma_device/README.md#f103rb-ep1-装配) | 显式选择 64 B 中断端点，复用 EP1 单包所有权与 EP0 |
| F102RB [USB FS PMA Device](../../../tests/integration/assemblies/stm32f1_pma_device/README.md) | HSE8 晶体、PLL48、APB1 24 MHz，裸机自供电，独立 LP20 与 USB PMA，64 B EP0 IN／OUT |
| F103RB [CAN1 静默内回环](../../../tests/integration/assemblies/stm32f1_can/README.md) | 裸机、HSI8、名义 500 kbit/s，有限发送与 FIFO0 轮询 |
| F105VC／F107VC [双 bxCAN 内回环](../../../tests/integration/assemblies/stm32f1_can_connectivity/README.md) | HSI8 裸机，CAN1／CAN2 各自中断与 FIFO，共用由 Platform 关闭的过滤域，未选择外部总线 |
| F105VC／F107VC [OTG FS Device](../../../tests/integration/assemblies/stm32f1_connectivity_dwc/README.md) | HSE25／PLL72，OTG 48 MHz，PA9 感知 VBUS，EP0 控制端点软件参考 |
| F105VC／F107VC [USB FS Host](../../../tests/integration/assemblies/stm32f1_connectivity_host/README.md) | 独立外部保护 VBUS、48 MHz USB、单 Full-Speed EP0 控制管道，显式取消与停机 |
| F107VC [Clause 22 PHY 管理](../../../tests/integration/assemblies/stm32f1_mdio/f107vc.toml) | PC1 MDC、PA2 MDIO，HCLK／42，有限任务态读写。独占 MAC 管理时钟，外部 PHY 与上拉由板卡提供 |
| F107VC [MB784 C.1](../../../boards/stm3210c_eval_mb784_c01/README.md) | STM3210C-EVAL 的 DP83848CVV 管理接线、X1 与跳线条件 |
| F107VC [MB784 USART2 重映射](../../../tests/integration/assemblies/stm32f1_uart_remap/README.md) | PD5／PD6，经板载 RS-232 收发器，关闭归还 AFIO 字段 |
| F103RB [RTC 接管](../../../tests/integration/assemblies/stm32f1_rtc/README.md) | 只读取由外部预先配置的 LSE RTC 的备份字，与 PVD 装配互斥 |
| F103RB [私有 RTC 闹钟探针](../../../tests/integration/assemblies/stm32f1_rtc/README.md#私有原始计数闹钟) | 保留 LSE，原始比较器，EXTI17，专用 IRQ 与前台排空 |
| F103RB [CAN1 外部载板软件参考](../../../tests/integration/assemblies/stm32f1_can_normal/README.md) | 裸机、HSE72，SN65HVD230DR，500 kbit/s，TX/RX 轮询和显式收发器停机 |
| STM3210E-EVAL MB672 D06 [FSMC SRAM 装配](../../../tests/integration/assemblies/stm32f1_fsmc/README.md) | F103ZG 的 NE3，独占 1 MiB CPU 窗口与安全回收 |
| F103RB [SPI2 有限 DMA](../../../tests/integration/assemblies/stm32f1_spi_dma/f103rb-spi-dma.toml) | HSI8，8 位模式 0，1 MHz。DMA1 通道 5 TX 和通道 4 RX，处理完成后排空并撤销片选 |
| F103RB [I2C1 Target](../../../tests/integration/assemblies/stm32f1_i2c_target/README.md) | PB6／PB7，HSI8 单地址接收，80 kHz，有限请求和双向量撤回 |
| F100RB／F103ZE／F103ZG [DAC1 直接输出](../../../tests/integration/assemblies/stm32f1_dac/README.md) | 同一 PA4 输出实现，8 位及两种 12 位数字格式，显式启动和空闲码 |
| F103ZE [SDIO SD 卡](../../../tests/integration/assemblies/stm32f1_sdio/README.md) | HSI8，单线轮询 512 B 扇区。默认读取 SDHC／SDXC，可选 SDHC 单扇区写入 |
| F103ZE [I2S2 主发送](../../../tests/integration/assemblies/stm32f1_i2s2/README.md) | HSI8、Philips 双声道 16 位、单块 DMA1 Channel 5，名义帧率可精确查询 |

x6／xB 的 IRQ 槽数为 43，xE／xG 为 60。链接验证单独校验官方向量保留项与 BootRAM 尾部。
具体模式以装配和能力矩阵为准。

USART2 线模式共用一个驱动。CMake 从解析生成的控制器记录读取所选协议并限定
编译范围。普通字节 IRQ 装配无需读取线模式记录。

官方源码准备复用两个已有组织子模块。应用可使用源码或显式导出的 F1
Source SDK，并为每个 Board／后端建立独立构建目录。软件参考与命令见
[独立消费者](../../../tests/integration/assemblies/stm32f1_family/README.md)。
