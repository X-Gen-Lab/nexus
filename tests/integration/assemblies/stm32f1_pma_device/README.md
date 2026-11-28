# NUCLEO-F103RB 外接 USB Device 软件参考

目标芯片是 STM32F103RBT6。STM32 的 USB 线路从目标板 CN10 引出，通过外接 USB 设备连接器进入主机。NUCLEO 自带 CN1 服务于 ST-LINK。下表选择的外接适配器及线路尚未在实物上观测或测量。

| 信号 | 芯片 | 板上连接 | 外接适配器条件 |
| --- | --- | --- | --- |
| USB D− | PA11 | CN10.14 | USB_DM，成对布线 |
| USB D+ | PA12 | CN10.12 | USB_DP，开关控制的 1.5 kΩ 上拉至 3.0–3.6 V |
| VBUS 检测 | PA0 | CN7.28，亦可用 CN8 A0 | 经适配器转换至 MCU 输入范围，输出高表示 VBUS 存在 |
| 上拉开关 | PA1 | CN7.30，亦可用 CN8 A1 | 高有效，外接硬件默认断开 |
| 信号地 | GND | CN10.20 | 主机信号地与目标信号地按适配器设计连接 |
| 上拉参考 | 3.3 V | CN7.16 | 供电路径与目标板电源方案一起审查 |

PA11 和 PA12 的芯片功能见 DS5319 的引脚表，PMA 与 bxCAN 互斥见 RM0008。PA0 和 PA1 的附加功能由外接适配器定义，平台用现有逻辑 GPIO 输入与输出合同消费。应用确认 VBUS 后启动 D+ 上拉。软件回读验证的是 MCU 数字输出，适配器开关、电气路径、上拉幅值以及实际枚举留给 HIL。选择本软件参考时应避开与这四个脚的复用冲突。

时钟沿用明确选择的 8 MHz MCO 旁路和 PLL72。USBPRE=0 给出名义 48 MHz，APB1 为 36 MHz。晶准、连线和实际 IRQ 间隔未测。物理状态 `not_executed`。

[UM1724 Rev 17 图 13、表 27](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf) · [DS5319 引脚表、表 44／45](https://www.st.com/resource/en/datasheet/stm32f103rb.pdf) · [RM0008 Rev 21 §23](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)

## 固件验证范围

此装配复用 Nexus 的精确 F103RB 启动、时钟、链接器和内存门禁。应用通过
强类型 factory 获取 USB Device、控制 IN 和控制 OUT。64 字节 EP0 的 IN 和 OUT
分别占用 PMA 偏移 64 和 128 字节，前 64 字节预留给硬件描述符表。配置工具校验
缓冲不相交，并为 USB 48 MHz、四个板级引脚、低优先级 IRQ 和 USB／CAN 共用的
PMA 分配唯一所有者。配置不提供 DWC 的 FIFO、SETUP 决策或端点激活接口。产品
通过 EP0 的 submit、cancel、stall 和 Device 的 SET_ADDRESS 驱动控制事务。

首批参考要求启动时已检测到外部 VBUS，D+ 上拉在 USB gate、EP0 和 LP20
向量就绪后才开启。产品通过显式 service 推进传输、控制事件、超时和 VBUS 撤回。
成功停止依次确认 D+ 脱离、LP20 mask／pending／active、EP0 FRES 和保留 CTR 的
排空，再释放设备 gate。Platform 停止时还恢复四个板级引脚。USB 重新接入使用
下一次受控平台启动，外接适配器保持硬件默认不上拉。

本参考为自供电。SUSPEND 保留系统和 USB 48 MHz 时钟。按 RM0008 Rev 21
§23.4.5 与表 172，WKUP 期间观察到 SE0 时清除硬件 FSUSP，让控制器接收后续
RESET。软件继续对外报告挂起，并拒绝新包、地址暂存和端点状态变更。真实 RESET
负责撤回旧请求。SE0 消失而且没有 RESET 或新 K-state WKUP 时，回到 FSUSP。
已有请求仍由真实 CTR、复位或停机排空。

RESUME 表示在 WKUP 时采样到 K 状态，需要及时调用 service。ES096 §2.12.1
说明旧 K 状态也可能触发 WKUP。此自供电配置保留连接，无 FRES 低功耗补救要求。
无需 EXTI18／IRQ42 唤醒。物理时序、远程唤醒和 USB 总线供电低功耗均不在
本软件参考范围。ES096 §2.12.4 要求至少 13 MHz APB1，本配置为名义 36 MHz。

[ES096 Rev 15 §2.12](https://www.st.com/resource/en/errata_sheet/es096-stm32f101x8b-stm32f102x8b-and-stm32f103x8b-mediumdensity-device-limitations-stmicroelectronics.pdf)

## F102RB 自供电载板

F102RBT6 另有一套外部接线参考，沿用 PA0 的高有效 VBUS 检测、PA1 高有效
上拉控制，以及 PA11／PA12 的专用 USB 信号。按 DS5933 Rev7 图 4 和表 4，
LQFP64 上四者分别是 14、15、44、45 脚。板载 8 MHz 晶体选 PLL×6，
SYSCLK 和 USBCLK 名义 48 MHz，APB1 24 MHz，USBPRE 置一。LP20 使用
USB_LP_IRQHandler。该芯片无 bxCAN，配置独占 USB PMA，外部电路与实测
条件仍与 F103 自供电参考相同。

[DS5933 Rev 7 图 2、图 4、表 4、表 8 与表 42](https://www.st.com/resource/en/datasheet/stm32f102rb.pdf)

## F103RB EP1 装配

`f103rb-bulk.toml` 和 `f103rb-interrupt.toml` 复用同一 F103RB 自供电接线、
时钟和 USB／CAN 共用 PMA 的排他约束。分别选择 `device-pma-bulk` 和
`device-pma-interrupt`。强类型 factory 额外提供同名 EP1 IN、EP1 OUT
和端点激活接口。EP0 和两条 EP1 方向各使用 64 字节。EP1 IN 和 OUT
分别位于 PMA 偏移 192 与 256 字节，两条方向各持有至多一个调用者请求。
EP0 装配只选编原有控制端点。

两种装配使用同一个单缓冲驱动，配置明确选择硬件 EP_TYPE。两条 EP1 方向
共享该字段，必须选择相同的传输类型。Interrupt 的轮询周期由产品描述符的
`bInterval` 通告，由 USB 主机调度。代码按同样的 LP20 和硬件 CTR 推进。
[RM0008 Rev 21 §23.3.4、§23.5.2 与表 174／175](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32-bit-mcus-stmicroelectronics.pdf)

产品使用当前控制 epoch 显式开启 `0x81` 和 `0x01`。标准 SET_CONFIGURATION
的 STATUS IN 等待态允许将 EP1 置于 NAK，数据接纳等到该控制事务结算。
USB 描述符、SET_CONFIGURATION 和 Class 归产品负责。发送以
硬件 CTR ACK 结算，接收报告实际字节数。及时调用 service 推进两个方向。
取消和超时仍须等待实际 CTR、总线复位或设备停止确认后才能复用请求和缓冲。
复位撤回两个方向的请求，下一次配置需要重新激活。板级 USB 枚举、吞吐和
实物停止证据仍为 `not_executed`。

## 可选 ST Device Core

`f103rb-st-device.toml` 选择锁定版本的三个官方 ST Device Core 源文件。
ST Core 通过原有 Nexus USB 接口使用 PMA。生成的 profile 明确选择
Packet Memory，FIFO 字段均为零。纯 PMA 装配无需该 profile 和 ST 源码。
自供电夹具明确设置 Core 的 GET_STATUS 电源位。

私有 vendor class 仅实现单包 Bulk 回显。描述符声明 EP1 IN／OUT，class
在 SET_CONFIGURATION 的 STATUS IN 确认后才开始接收。Native 模型让实际
Core 和实际 F103 PMA 驱动一起完成地址提交、配置描述符、短包回显和复位排空。
VID/PID 留空供软件验证。CDC ACM 需要额外 EP2 IN 和专用 class 存储，实物
枚举、吞吐和主机驱动兼容性仍需验证。
