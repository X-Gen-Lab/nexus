# NUCLEO-F103RB 板级参考

该包对应 MB1136 上的 STM32F103RBT6，LQFP64，128 KiB Flash 与 20 KiB SRAM。
`board.json` 的出处是 ST UM1724 Rev17。实际 PCB 修订、焊桥和实板行为尚未观察。

| 绑定 | 连接与初始化 |
| --- | --- |
| `led0` | PA5/D13 经 SB21 接 LD2，高有效；初始写低 |
| `uart2` | USART2 默认复用 PA2 TX、PA3 RX；资料中的 ST-LINK VCP 路线 |
| `flash0` | 片内 Flash，1 KiB 擦除页、2 字节编程单位；依据 UM1724 §7.5 声明名义 3.3 V，运行时不测量供电 |

VCP 路线要求 SB13、SB14 接通且 SB62、SB63 断开。烧录和串口由 ST-LINK
提供。该 USB 接口并未连接目标 MCU 的 USB Device 控制器。
片内 Flash 由配置显式选入，绑定本身不会擦写。应用为数据选择已留出的地址范围。

## 时钟与资源

本参考选择名义 `hsi8`，`board.json` 不声明外部振荡器。UM1724 §7.9 记录
MB1136 C-01 默认不使用 HSE，C-02 及后续版本默认使用 ST-LINK 的 8 MHz MCO。
这些事实用于板级核对。当前软件配置没有将外部时钟或 LSE 加入已审档位。
PA13/PA14 和 PA15/PB3/PB4 保留给 SWJ。PD0/PD1 和 PC14/PC15 保留给时钟接线。

baremetal 与 FreeRTOS 的参考构建使用相同绑定。TIM2 属于系统单调时基。FreeRTOS
的调度 tick 由已选内核端口提供。平台初始化完成后，应用通过生成的强类型 factory
取得 LED 与串口接口并自己管理任务、缓冲区和关闭顺序。

实板验证前核对丝印、MB1136 修订、焊桥、3.3 V 供电、串口实际波特率和 LED 电平。
软件配置、主机模型和 ARM 链接报告分别记录，物理状态保持 `not_executed`。

[ST 板级手册 UM1724](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
