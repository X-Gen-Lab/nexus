# STM32F103RB 外部 CAN 软件参考

该装配声明 NUCLEO-F103RB MB1136 C-03 的 ST-LINK 8 MHz MCO 和独立的
SN65HVD230DR 载板。产品装配显式选择 can_transceiver。型号和接线未确认时，
装配检查应失败。台架需要第二个会应答的 Classical CAN 节点。

| 信号 | MCU | morpho | 收发器 |
| --- | --- | --- | --- |
| CAN_RX | PA11 | CN10-14 | R |
| CAN_TX | PA12 | CN10-12 | D |
| Standby | PC2 | CN7-35 | RS |
| VCC | 3.3 V | CN7-16 | VCC |
| Reference | GND | CN7-20 | GND |

RS 由板端电阻外拉至 3.3 V 并在固件运行前保持发送禁用。固件通过开漏
输出使能 RS，验证停机状态后才重置 CAN 和归还 GPIO。总线线缆两端配置
与台架匹配的终端。运行模式使用 36 MHz APB1、500 kbit/s、采样点
77.78%、SJW 1 tq；邮箱 0 单次 TX、FIFO0 轮询。RX 事件记录软件观察时刻。
关机、总线错误和 ACK 行为可通过真实 C 模型检查。

参考资料为 [UM1724 Rev17 §7.9.1、表 27](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)、
[RM0008 Rev21 §§9.3.3、24.6、24.7.7](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)、
[ES096 Rev15 §§2.3.1、2.11.1](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)
及 [TI SLOS346O §§8.10、10.4](https://www.ti.com/lit/ds/symlink/sn65hvd230.pdf)。
当前软件证据为装配、ARM 链接与模型，载板实物、时钟、终端及外部总线行为
待实板验证。
