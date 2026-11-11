---
id: delivery.support
type: reference
audience: consumer
source_language: zh_CN
---

# 当前精确支持范围

本页帮助消费者选择当前维护的精确器件、Board、后端与外设模式。当前事实包含
STM32F405ZGT6、F407VET6、F407ZGT6、F415ZGT6 和 F417ZGT6；实际模式仍由
同一 TOML／typed IR resolver 按所选器件、封装和 Board 取交集。新增三料号已有
内部 CRC 软件参考，不继承 F407 专属模式的执行结果，也不授予整系列物理资格。

## 型号、Board 与 OS 边界

| 对象 | 精确边界 |
| --- | --- |
| STM32F407VET6 | LQFP100，512 KiB Flash、128 KiB 主 SRAM、64 KiB CPU-only CCM；只开放该封装实际引出的路线 |
| STM32F407ZGT6 | LQFP144，1 MiB Flash、128 KiB 主 SRAM、64 KiB CPU-only CCM；额外引脚不自动授权 PCB 接线 |
| F405ZG／F415ZG／F417ZG | LQFP144、1 MiB Flash、128 KiB 主 SRAM、64 KiB CCM；[内部 CRC 软件参考](../../tests/contracts/stm32f4_family/README.md)只使用 HSI16，未声明实体 PCB 或外接振荡源 |
| 启明 ZG Board | [V3.1](../../boards/stm32f407_qiming_v31/README.md)、[独立核心板](../../boards/stm32f407zg_qiming_core/README.md)、[V4.1](../../boards/stm32f407zg_qiming_v41/README.md) 是三个独立来源和装配身份 |
| 天空星 VE Board | [天空星青春版](../../boards/stm32f407ve_sky_qingchun/README.md)；不推广到 VG 高配板或其他 PCB 修订 |
| 裸机 | 显式选择 `baremetal`；启动不创建调度器或产品任务，调用者推进 service 与停止 |
| FreeRTOS | 静态任务／队列／等待适配；按所选 assembly、实际 IRQ syscall ceiling 和执行上下文接入 |
| ThreadX | 已维护的 M4 适配子集与明确消费者；不继承所有 FreeRTOS 模式组合 |
| Native／CPU-only Runtime | Native 是显式 host-test scope；CPU-only Runtime 不提供 SoC 外设，二者都不是 PCB 资格 |

引脚、AF、DMA、IRQ、Flash 几何和资源冲突只在
[SoC 事实](../../soc/stm32/stm32f4/soc.json)、
[路线](../../soc/stm32/stm32f4/routes.json)、[Board 包](../../boards/README.md)及
实际 provider 中维护。本页不复制第二份 pin／DMA 表。软件 fixture 只声明测试路线；
原理图、元件实装、跳线、电压和晶振事实仍须由精确 Board 授权。

OS 的静态对象、权限、IRQ 和退出要求见 [OS 合同](../design/os-contracts.md)与
[后端入口](../../os/README.md)。选择一个后端不自动验证每种外设／低功耗组合。

F405／F407 共用 DS8626 Rev12，F415／F417 共用 DS8597 Rev10；精确 package、
IRQ、DMA 和路线仍逐料号检查。67 项共同控制器事实共享，ETH／DCMI 只由 F407／
F417 选择，CRYP／HASH 仍仅为厂商库存。新增三料号已有独立 ARM CRC 链接与开发 SDK
消费，开发包的 `publishable=false` 不等于最终 clean-source 候选通过；全部外设组合
及任何 PCB／HIL 结论仍须各自取得证据。

## 当前模式矩阵

| 能力 | 维护模式与关键边界 | 权威合同 |
| --- | --- | --- |
| 冷态与平台归还 | 静态 factory 纯查询；手写 C 执行准入、取得、逆序撤回和回读。部分效果、未退出 IRQ／DMA 与失败引脚恢复继续保留原 owner 和收条 | [所有权](../explanation/ownership.md)、[F4 实现](../../soc/stm32/stm32f4/README.md) |
| System／时钟 | 默认 `hse8-pll168`；显式 custom HSE／HSI、直接输入或 PLL、AHB／APB／ADC 分频与 Flash latency。频率来自同一静态 ClockPlanIR，不提供运行期 DVFS | [System 与时钟](../design/f407-system-power.md) |
| Sleep／STOP | 浅 Sleep 保留时间域；显式排空后进入 STOP，恢复建立新 Core epoch，旧 deadline 不继承。唤醒源、独立 elapsed bounds 和 OS 恢复由调用者承担 | [System 低功耗](../design/f407-system-power.md)、[ClockDomain 屏障](../design/clock-domain-sleep.md) |
| Standby | 显式 WKUP／RTC 唤醒与同一 RTC owner 的准备收条；按勘误禁源、处理 WUF，RTC 事件只按明确授权丢弃。真实进入经复位重建启动；虚假返回建立新 Core epoch，restore 失败保留责任 | [System 低功耗](../design/f407-system-power.md)、[RTC 合同](../design/f407-rtc.md) |
| 监督与故障 | IWDG／WWDG 显式启用；可选 WWDG EWI、HSE CSS／NMI、复位原因、首次故障记录与 DWT trace。无自动喂狗、任务重启或旧时间域降频续跑 | [监督与故障](../design/f407-system-power.md) |
| GPIO／EXTI／PVD | 精确封装全部 GPIO 的 input／output、四档显式输出速度与 EXTI0–15；PVD 原始电平／事件。EXTI 的 `hardware-event` 只产生 EMR 硬件事件，不取得 IRQ 或软件队列。省略速度保留已有配置，PC13–15 输出限制为 LOW；晶振 LSE 占用时 PC13 保持静默，晶振和调试引脚仍受资源约束。软件事件保留 COALESCED／LOSS，不提供自动消抖、精确边沿时间或 BOR 资格 | [F4 GPIO／PVD](../../soc/stm32/stm32f4/README.md)、[I/O 观察](../design/io-observation-recovery.md) |
| UART 数据传输 | 全部六实例：IRQ byte／event、独立 9N1 words、调用者存储的 blocks、有限 HAL TX DMA、有限／连续 RX DMA 和 duplex；各模式分别选择 | [UART DMA 与 RS485](../design/uart-dma-rs485.md) |
| UART 线模式 | 全部六实例支持半双工、LIN、IrDA；同步 master、Smartcard、CTS／RTS 只用于 USART1／2／3／6。RS485 显式组合 DE 与既有传输 owner | [UART 线模式](../design/uart-wire-modes.md)、[RS485](../design/uart-dma-rs485.md) |
| SPI | SPI1／2／3 polling、8／16-bit 分段事务、有限 DMA；`irq-frames` 提供 master／slave、单双线、Motorola／TI 与双线 CRC。单线 CRC 拒绝；主接收精确帧数以声明时序预算成立为条件 | [SPI 帧模式](../design/spi-frame-modes.md)、[公共 I/O](../reference/api/io.md) |
| I2C master | I2C1／2／3 polling／IRQ／DMA、显式 7／10-bit endpoint；实际时序与 repeated START 勘误共同限制准入；与 target personality 分选 | [总线恢复](../design/i2c-bus-clear.md)、[F4 I2C](../../soc/stm32/stm32f4/README.md) |
| I2C target／SMBus | I2C1／2／3 的 `target-irq` 提供 7／10-bit 自身地址与 7-bit 双地址；`smbus-device` 为 7-bit、至多 100 kHz，可选 PEC／SMBA ARA。每方向至多 256 B、WRITE_READ 合计至多 512 B、最多两个地址阶段；IRQ、无 DMA，响应预计算。PEC 受声明 IRQ 预算约束；ARP／Host Notify 与命令策略由产品提供 | [I2C target 合同](../design/f407-i2c-target.md) |
| 普通 PWM／capture／encoder | 已审 TIM3／4／5／9–14 PWM；TIM1／3／4／5／8／9–14 原始输入捕获，按实例开放 1／2／4 通道，TIM5 保留 32 位；TIM1／3／4／5／8 支持 x2／x4 正交计数。TIM2 保留给 Core，TIM6／7 用作专用触发；timer personality 独占，IRQ 只取得所选模式实际使用的向量 | [定时器合同](../design/timer-advanced.md)、[F4 实现](../../soc/stm32/stm32f4/README.md) |
| 互补 PWM | TIM1／TIM8 的 CH1–3／CH1N–3N、deadtime、break 和显式 rearm；没有自动故障重启或隐式跨 timer 同步 | [高级定时器](../design/timer-advanced.md) |
| PWM 组／有限波形 | 单 timer 多通道 CPU group、硬件单脉冲与有限 UPDATE-DMA sequence；实例、TRGO／ITR、break 和 burst 预算显式选择。sequence 成功统计 staged 存储字节，不承诺精确电脉冲数 | [有限波形合同](../design/f407-timer-waveform.md) |
| 独立 ADC | ADC1／2／3 的已审外部输入，ADC1 内部输入；6／8／10／12-bit、regular shot／scan、软件 injected、有限／连续 timer DMA。内部输入与模拟路径有独立启动／退出责任 | [F4 ADC](../../soc/stm32/stm32f4/README.md) |
| 多 ADC／注入采集 | `adc_group` 独占 ADC123 公共域；双／三同步、交错的有限 DMA，以及单／双／三注入组。显式触发、格式、采样时间与原始码看门狗；没有循环多 ADC 或 regular／injected 并行 | [多 ADC 合同](../design/f407-adc-groups.md) |
| DAC | 单／双通道 direct、独占 timer trigger、noise／triangle、有限／循环 DMA 与显式 idle code。数字完成不承诺精确 N 次模拟转换、settling 或输出精度 | [F4 DAC](../../soc/stm32/stm32f4/README.md) |
| DCMI | RAW／JPEG snapshot 与连续 RAW storage blocks；VE 最高 12-bit，ZG 可选 14-bit。JPEG 返回含硬件 padding 的 word 前缀，RAW blocks 不等于图像或精确帧数 | [DCMI／I2S 合同](../design/f407-capture-audio.md) |
| I2S2／3 | master／slave × TX／RX／duplex 的有限 DMA 与连续 `*-blocks`；Philips、MSB／LSB justified、PCM short／long，16／24／32-bit 数据。master 显式 PLLI2S，slave 实际外部 rate 为 unknown；连续模式显式声明 rate／service 上界，使用调用者 block 存储，已发布 RX loan 单独归还。不转换音频格式 | [音频合同](../design/f407-capture-audio.md)、[有限 slave TX 路径](../design/f407-i2s2-slave-tx.md) |
| RTC／备份域 | `adopt-retained`／`provision-empty`、Alarm A request、Alarm B、wakeup、timestamp、tamper、校准、20 个 backup registers、4 KiB backup SRAM CPU lease 与显式 handoff。PC13 timestamp／tamper 需要 Board 明确授权 LSE bypass；四个现有 Board 的晶体配置不能据 API 存在推定支持。不提供世纪／时区或保持性证明 | [RTC 合同](../design/f407-rtc.md) |
| 内部 Flash／制造维护 | 精确 VE／ZG sector 几何；显式维护 face 提供 OTP、锁定、option／RDP 及保护降级的整阵列擦除效果。失败后的 `recover` 只排空、撤回并重锁，确认成功后才归还 owner；不可逆效果逐次声明，不自动回滚、重放或把软件复位当作所需 POR | [物理存储与维护](../design/f407-storage.md) |
| SDIO／MMC／I/O functions | SD memory／MMC polling、有限 IRQ／DMA；SDIO functions 使用独立 face 的 CMD5／52／53。单卡、3.3 V，SD／SDIO 为 1／4 位，MMC 可显式选择经版本和 SWITCH 检查的 8 位十引脚软件参考。memory 单块 CMD17／24，多块以一次 CMD18／25、CMD12 和 CMD13 READY 结束；combo 不同时维护 memory portion。静态 ClockPlan 要求精确 PLL48 与经 RM／勘误联合验证的分频；不提供 HS／DDR／HS200 | [SDIO 合同](../../soc/stm32/stm32f4/private/sdio_hal/README.md) |
| FSMC | 运行期 CPU window、永久 startup SRAM、parallel16，以及显式 PSRAM、有限 NOR program／erase、raw NAND／OOB／ECC 码；bank 与 mux／burst／WAIT 按封装和所选模式授权，无 FTL 或自动纠错 | [FSMC 合同](../design/f407-fsmc.md)、[存储边界](../design/f407-storage.md) |
| CRC／芯片身份 | 固定 CRC-32 数值字序列；实际 UID、device／revision 与 Flash 容量。数据不证明完整订货号、封装身份或认证秘密 | [F4 CRC／身份](../../soc/stm32/stm32f4/README.md) |
| RNG | 唯一 typed RNG factory 与 Platform 生命周期；精确 PLL48 和活跃 System epoch，保留官方 seed／clock／连续值错误检查；不建立 DRBG、熵池或质量认证 | [RNG 时钟合同](../design/f407-system-power.md)、[公共 I/O](../reference/api/io.md) |
| CAN | CAN1／2 classic IRQ、mailbox 与共享 filter domain；F407 不提供 CAN FD | [Classic CAN](../design/classic-can.md) |
| Ethernet | VE／ZG MII 与 ZG RMII、独立 MDIO；raw `eth` 与增强 `eth_frames` 分选。已选 HCLK 为 25–168 MHz，MDC 与 PTP 20／40／80 ns 增量由计划推导。增强 face 提供整帧 SG、单 C-tag VLAN、显式 checksum offload 和 MAC 硬件事件／PTP 时间戳；这不建立同步或物理精度资格，PHY 接线、协议栈、同步与网络政策由外部提供 | [Ethernet 帧](../design/ethernet-frames.md)、[MAC／MDIO](../design/ethernet.md) |
| USB | FS／HS 引擎的 Device／Host、PIO 与 HS AHB DMA；Device 支持 control、bulk、interrupt 和单 transaction ISO packet。Device ISO 使用实际速度的帧窗口，接纳时剩余期限至多 1 秒，并满足受审 FIFO 分区；无 ACK／自动重试或周期调度。内部 PHY 仍为 FS，外部 ULPI 提供 HS／FS 回退；显式 managed roles 与按需 ST 官方 Core 桥接。同 epoch 提前零长 STATUS OUT 与 DATA IN 可并存，线上闭合后仍须双方向内存 join。不维护高带宽 ISO、remote wake、自动 HNP／SRP 或 Host split | [USB 所有权](../design/usb-device.md) |
| 可选器件组件 | ILI9341 parallel16、XPT2046、W25 等通过公共端口访问；板级供电、存储分区和恢复政策由外部工程提供 | [组件入口](../../components/README.md)、[I/O 所有权](../design/io-evolution.md) |

模式矩阵不穷举所有芯片功能的笛卡尔组合。未列出的格式、外部触发、共享资源和
后端组合必须由相应 provider 明确接入；不能从一种模式或公共 API 的存在推定。

## 选择、证据与来源

先确定精确型号和 Board 授权，再选择模式并通过 resolver 检查资源、内存域和时钟。
[派生能力表](capabilities.md)用于追踪声明、路线、装配和生产 ops；它不是第二份
配置输入，也不证明相关运行、排空或物理时序已通过。

源码维护、分组模型回归、ARM ELF、Source SDK、完整软件候选和物理 station 分别
建立证据，并绑定各自源码与配置。代理的 GREEN 或单个 ELF 不构成全候选通过。
当前 physical HIL 为 `not_executed`；未知 PCB、晶振精度、跳线、负载和波形保持未知。

所有 vendor、子模块、锁与许可证保留。未选 SDK 不进入默认生产构建闭包；
[器件目录](../../soc/stm32/stm32f4/devices.json)只描述官方宏、启动和 IRQ 编译事实。
历史范围见[固定重构前源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)，
旧候选、计数和实板结果不重标为当前源码资格。
