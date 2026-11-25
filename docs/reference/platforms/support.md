---
id: reference.platforms.support
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.platforms.support)=
# 精确平台支持

生产系列入口是STM32F4。精确料号、器件视图、四个F407 Board及模式分别说明；厂商SDK库存不参与默认支持。芯片存在、公开接口和配置通过不证明实板已运行。

## 型号、Board 与 OS 边界

| 对象 | 精确边界 |
| --- | --- |
| STM32F407VET6 | LQFP100，512 KiB Flash、128 KiB 主 SRAM、64 KiB CPU-only CCM；只开放该封装实际引出的路线 |
| STM32F407ZGT6 | LQFP144，1 MiB Flash、128 KiB 主 SRAM、64 KiB CPU-only CCM；额外引脚不自动授权 PCB 接线 |
| F405ZG／F415ZG／F417ZG | LQFP144、1 MiB Flash、128 KiB 主 SRAM、64 KiB CCM；[内部 CRC 软件参考](../../../tests/contracts/stm32f4_family/README.md)只使用 HSI16，未声明实体 PCB 或外接振荡源 |
| STM32F411RET6 | LQFP64、512 KiB Flash、128 KiB 主 SRAM、无 CCM；显式 HSI96 RUN，并保留 direct HSI16；所选 TIM2／CRC 与精确高频配置下 RTC／Power 参考分别校验，不扩展到其它外设模式 |
| STM32F446ZET6 | LQFP144、512 KiB Flash、128 KiB 主 SRAM、无 CCM；显式 HSI168 RUN，并保留 direct HSI16；SAI1／2 消费独立 M 的 PLLSAI Q，不开放 LTDC R |
| STM32F479NIH6 | TFBGA216、2 MiB Flash、320 KiB 主 SRAM、64 KiB CPU-only CCM；显式 HSI168 RUN，并保留 direct HSI16；按精确模式开放 FMC／SAI／LTDC／DMA2D／QuadSPI，PLLSAI 共享 M。GPIO 软件参考仅开放 PJ0／PK3 的 input／output、baremetal，不推广到其它 J／K pad 或 PCB。普通 Flash 写擦覆盖两个物理 bank，OPTCR1 维护不受支持 |
| 启明 ZG Board | [V3.1](../../../boards/stm32f407_qiming_v31/README.md)、[独立核心板](../../../boards/stm32f407zg_qiming_core/README.md)、[V4.1](../../../boards/stm32f407zg_qiming_v41/README.md) 是三个独立来源和装配身份 |
| 天空星 VE Board | [天空星青春版](../../../boards/stm32f407ve_sky_qingchun/README.md)；不推广到 VG 高配板或其他 PCB 修订 |
| 裸机 | 显式选择 `baremetal`；启动不创建调度器或产品任务，调用者推进 service 与停止 |
| FreeRTOS | 静态任务／队列／等待适配；按所选 assembly、实际 IRQ syscall ceiling 和执行上下文接入 |
| ThreadX | 已维护的 M4 适配子集与明确消费者；不继承所有 FreeRTOS 模式组合 |
| Native／CPU-only Runtime | Native 是显式 host-test scope；CPU-only Runtime 不提供 SoC 外设，二者都不是 PCB 资格 |

引脚、AF、DMA、IRQ、Flash 几何和资源冲突只在
[SoC 事实](../../../soc/stm32/stm32f4/facts/soc.json)、
[路线](../../../soc/stm32/stm32f4/facts/routes.json)、[Board 包](../../../boards/README.md)及
实际 provider 中维护。本页不复制第二份 pin／DMA 表。软件 fixture 只声明测试路线；
原理图、元件实装、跳线、电压和晶振事实仍须由精确 Board 授权。

OS 的静态对象、权限、IRQ 和退出要求见 [OS 合同](../contracts/os/execution.md)与
[后端入口](../../../os/README.md)。选择一个后端不自动验证每种外设／低功耗组合。

F405／F407 共用 DS8626 Rev12，F415／F417 共用 DS8597 Rev10；经典四款器件复用
67 项共同控制器事实，ETH／DCMI 只由 F407／F417 选择，基础 CRYP／HASH 限定
F415／F417。F411、F446、F479 分别绑定独立 RM／ES profile；F446 的 PLLSAI M
独立，F479 与主 PLL 共用 M，同一 System owner 管理所选 Q／R 输出。

精确 package、IRQ、DMA 和路线仍逐料号检查。F479NI 当前显式 GPIO 名单为 159 项，
与资料摘要 161 项的差异保持未知，不补猜 pad。新料号与 IP 的软件参考不声明 PCB
接线；开发包 `publishable=false`、ARM 对象或单个 ELF 不能代替最终 clean-source
候选及全部消费者。原四板范围不变，全部物理 HIL 保持未执行。

## 当前模式矩阵

| 能力 | 维护模式与关键边界 | 权威合同 |
| --- | --- | --- |
| 冷态与平台归还 | 静态 factory 纯查询；手写 C 执行准入、取得、逆序撤回和回读。部分效果、未退出 IRQ／DMA 与失败引脚恢复继续保留原 owner 和收条 | [所有权](../../explanation/ownership.md)、[F4 实现](../../../soc/stm32/stm32f4/README.md) |
| System／时钟 | F407 默认 `hse8-pll168`，其余已审 custom HSE／HSI、PLL、总线和 Flash latency 共用 ClockPlanIR。F411 显式 HSI96；F446／F479 显式 HSI168，均保留 direct HSI16。F479 可选 M10／N210 以同时使用共享 M 的 PLLSAI 音频／图形输出；System 先证明 PLLRDY／VOSRDY、prefetch 与 latency，再切换并发布时间。HSI 名义频率不证明 USB 或音频精度。无 180 MHz／overdrive 或运行期 DVFS；新增器件 STOP 只按独立已审高频 profile 与唤醒合同开放 | [System 与时钟](../../../soc/stm32/stm32f4/system/README.md) |
| Sleep／STOP | 浅 Sleep 保留时间域；F407 既有配置与 F411／F446／F479 精确高频软件参考分别支持受约束 STOP。实际 traits 校验 NVIC bank、备份／CCM 和唤醒源，拒绝外来低功耗／debug 状态；排空后进入 STOP，真实 System 恢复才建立新 Core epoch，旧 deadline 不继承。仅 baremetal，elapsed bounds 由调用者提供 | [System 低功耗](../../../soc/stm32/stm32f4/system/README.md)、[ClockDomain 屏障](../contracts/platform/clock-domain.md) |
| Standby | 显式 WKUP／RTC 唤醒与同一 RTC owner 的准备收条；F446 另校验 EWUP2／PC13 与晶体 LSE 的冲突。F479 SBF／sticky WUF 阻止 RTC 重入；显式 reset-prepare 只按授权清 SBF，之后产品必须复位，平台不自动复位或清 RTC。真实进入经复位重建启动；虚假返回建立新 Core epoch，restore 失败保留责任 | [System 低功耗](../../../soc/stm32/stm32f4/system/README.md)、[RTC 合同](../contracts/io/f407-rtc.md) |
| 监督与故障 | IWDG／WWDG 显式启用；可选 WWDG EWI、HSE CSS／NMI、复位原因、首次故障记录与 DWT trace。无自动喂狗、任务重启或旧时间域降频续跑 | [监督与故障](../../../soc/stm32/stm32f4/system/README.md) |
| GPIO／EXTI／PVD | 按精确器件和已审路线选择 GPIO input／output、四档显式输出速度与 EXTI0–15；PVD 原始电平／事件。F479NI 的 J／K 当前只提供上表两条 GPIO 软件路线，不授予 EXTI。EXTI 的 `hardware-event` 只产生 EMR 硬件事件，不取得 IRQ 或软件队列。省略速度保留已有配置，PC13–15 输出限制为 LOW；晶振 LSE 占用时 PC13 保持静默，晶振和调试引脚仍受资源约束。单 pin 可显式声明 `active_high`，生成复用授权 port 的只读逻辑 signal；省略时无 signal，不从初始电平推导极性。软件事件保留 COALESCED／LOSS，不提供自动消抖、精确边沿时间或 BOR 资格 | [F4 GPIO／PVD](../../../soc/stm32/stm32f4/README.md)、[I/O 观察](../contracts/io/observation-recovery.md) |
| UART 数据传输 | 全部六实例：IRQ byte／event、独立 9N1 words、调用者存储的 blocks、有限 HAL TX DMA、有限／连续 RX DMA 和 duplex；各模式分别选择 | [UART DMA 与 RS485](../contracts/io/uart-dma-rs485.md) |
| UART 线模式 | 全部六实例支持半双工、LIN、IrDA；同步 master、Smartcard、CTS／RTS 只用于 USART1／2／3／6。RS485 显式组合 DE 与既有传输 owner | [UART 线模式](../contracts/io/f407-uart-wire-modes.md)、[RS485](../contracts/io/uart-dma-rs485.md) |
| SPI | SPI1／2／3 polling、8／16-bit 分段事务、有限 DMA；`irq-frames` 提供 master／slave、单双线、Motorola／TI 与双线 CRC。F4 polling／recover 只在特权、未屏蔽 Thread 执行；preflight 拒绝不写 progress 或控制存储，精确原地收发与部分重叠分别处理。单线 CRC 拒绝；主接收精确帧数以声明时序预算成立为条件 | [SPI 帧模式](../contracts/io/f407-spi-frames.md)、[公共 I/O](../api/io/spi.md) |
| I2C master | I2C1／2／3 polling／IRQ／DMA、显式 7／10-bit endpoint；实际时序与 repeated START 勘误共同限制准入；与 target personality 分选 | [总线恢复](../contracts/io/i2c-bus-clear.md)、[F4 I2C](../../../soc/stm32/stm32f4/README.md) |
| SMBus packet PEC | 可选 `Nexus::SMBus` 复用 7-bit 异步 I2C endpoint 和原强类型 ID；独立 companion 返回原静态 faces 的只读绑定。有限 write／read／write-read；32 B 包含 command／count，外层贷款直到内部 I2C drain 和 PEC 结清 | [SMBus 组件](../../../components/buses/smbus/README.md) |
| I2C target／SMBus | I2C1／2／3 的 `target-irq` 提供 7／10-bit 自身地址与 7-bit 双地址；`smbus-device` 为 7-bit、至多 100 kHz，可选 PEC／SMBA ARA。每方向至多 256 B、WRITE_READ 合计至多 512 B、最多两个地址阶段；IRQ、无 DMA，响应预计算。PEC 受声明 IRQ 预算约束；ARP／Host Notify 与命令策略由产品提供 | [I2C target 合同](../../../soc/stm32/stm32f4/drivers/i2c/README-target.md) |
| 普通 PWM／capture／encoder | 已审 TIM3／4／5／9–14 PWM；TIM1／3／4／5／8／9–14 原始输入捕获，按实例开放 1／2／4 通道，TIM5 保留 32 位；TIM1／3／4／5／8 支持 x2／x4 正交计数。TIM2 保留给 Core，TIM6／7 用作专用触发；timer personality 独占，IRQ 只取得所选模式实际使用的向量 | [定时器合同](../contracts/io/timer-advanced.md)、[F4 实现](../../../soc/stm32/stm32f4/README.md) |
| 互补 PWM | TIM1／TIM8 的 CH1–3／CH1N–3N、deadtime、break 和显式 rearm；没有自动故障重启或隐式跨 timer 同步 | [高级定时器](../contracts/io/timer-advanced.md) |
| PWM 组／有限波形 | 单 timer 多通道 CPU group、硬件单脉冲与有限 UPDATE-DMA sequence；实例、TRGO／ITR、break 和 burst 预算显式选择。sequence 成功统计 staged 存储字节，不承诺精确电脉冲数 | [有限波形合同](../../../soc/stm32/stm32f4/drivers/timer/README-waveform.md) |
| 独立 ADC | ADC1／2／3 的已审外部输入，ADC1 内部输入；6／8／10／12-bit、regular shot／scan、软件 injected、有限／连续 timer DMA。内部输入与模拟路径有独立启动／退出责任 | [F4 ADC](../../../soc/stm32/stm32f4/README.md) |
| 多 ADC／注入采集 | `adc_group` 独占 ADC123 域；保留有限 regular／injected 与双／三 ADC 同步或交错，新增对应 regular DBM blocks。同一同步 owner 可接一次软件或独占 Table69 timer 注入；有限 `regular-auto-injected` 用具名 descriptor 联合借用 regular／injected 两个 span，TC＋全部 JEOC＋真实 join 才共同终止。交错 blocks 按原生 CDR 几何、最快吞吐和 service gap 准入，不承诺均匀 aperture。未建立连续 JAUTO 逐次 JDR、交错＋注入或未知外来 trigger 合同。 | [多 ADC 合同](../contracts/io/f407-adc-groups.md) |
| DAC | 单／双通道 direct、独占 timer、noise／triangle、有限／循环 DMA 与 idle code；可选 EXTI9 event-only 外部触发 CPU／循环 DMA，实际外部 rate 为 unknown，有限外部尾部拒绝。ES0182 恢复可内部转换声明的 idle，随后短时禁输出，不计 payload，不保证请求间模拟连续。 | [F4 DAC](../../../soc/stm32/stm32f4/README.md) |
| DCMI | RAW／JPEG snapshot、连续 RAW storage blocks，及有限 frames-raw／frames-jpeg CM1 会话。每帧完整 join 后发布并重启，允许间隙和缺 FREE 时 source-off 等待；会话 transferred 统计完整帧前缀。JPEG word padding 不等于码流长度，无 CM0 无缝完整帧。VE 最高 12-bit，ZG 可选 14-bit。 | [DCMI／音频合同](../../../soc/stm32/stm32f4/drivers/ip/av/README.md) |
| I2S2／3 | master／slave × TX／RX／duplex 的有限 DMA 与连续 `*-blocks`；Philips、MSB／LSB justified、PCM short／long，16／24／32-bit 数据。master 显式 PLLI2S，slave 实际外部 rate 为 unknown；连续模式显式声明 rate／service 上界，使用调用者 block 存储，已发布 RX loan 单独归还。不转换音频格式 | [音频合同](../../../soc/stm32/stm32f4/drivers/ip/av/README.md)、[有限 slave TX 路径](../contracts/io/f407-i2s2-slave-tx.md) |
| SAI1／SAI2 | F446 的 SAI1／2、F479 的 SAI1：A／B master／slave × TX／RX／同步 duplex，有限与连续 `*-blocks`，8／10／16／20／24／32 有效位、至多 16 wire slots。显式 active mask 只为有效 slot 分配 FIFO 容器，保留原 wire 位置，不做重排。连续模式使用 caller block 存储、声明 SCK／service 上界，撤回目标后才发布 RX loan；缺块撤源、失败排空保留责任。master 消费唯一 System PLLSAI Q；slave 实际外部速率 unknown。无跨 SAI 同步、AC97／SPDIF 或物理时序保证 | [SAI 与布局](../../../soc/stm32/stm32f4/drivers/ip/av/README.md) |
| RTC／备份域 | `adopt-retained`／`provision-empty`、Alarm A request、Alarm B、wakeup、timestamp、tamper、校准、20 个 backup registers、4 KiB backup SRAM CPU lease 与显式 handoff。PC13 timestamp／tamper 需要 Board 明确授权 LSE bypass；四个现有 Board 的晶体配置不能据 API 存在推定支持。新增 F411／F446／F479 的晶体 LSE primary retained／provision 精确高频参考；F411 有 20 个 backup registers、无 backup SRAM，其 SRAM ops 为 NULL；F446／F479 保留 4 KiB lease。未增加这些料号的 bypass、periodic 或 RTOS 组合。不提供世纪／时区或保持性证明 | [RTC 合同](../contracts/io/f407-rtc.md) |
| 内部 Flash／制造维护 | F407 VE／ZG 的精确 sector 几何及显式 OTP／option／RDP 维护；F479NI 的 2 MiB／24 sectors 只提供 poll-word 读写擦，bank2 逻辑编号映射 SNB16–27，拒绝交换映射、BFB2／PCROP 和未审 OPTCR1 维护。F479 按 ES0321 在 pulse 前关闭 DCEN，真实 idle 后 reset／restore；失败保留同一 owner。Flash pulse 不可中止，同 bank 执行可停顿；deadline 不撤销借用。不自动回滚、重放不可逆维护或以软件 reset 代替 POR | [物理存储与维护](../contracts/io/f4-flash-storage.md) |
| SDIO／MMC／I/O functions | SD memory／MMC polling、有限 IRQ／DMA；SDIO functions 使用独立 face 的 CMD5／52／53。单卡、3.3 V，SD／SDIO 为 1／4 位，MMC 可显式选择经版本和 SWITCH 检查的 8 位十引脚软件参考。memory 单块 CMD17／24，多块以一次 CMD18／25、CMD12 和 CMD13 READY 结束；combo 不同时维护 memory portion。默认速度使用精确 PLL48 与经 RM／勘误联合验证的分频；另选精确 F407 `hse8-pll150-sdio375`、裸机四位 SD／MMC 37.5 MHz 高速 SDR，须完成 SD CMD6 或 MMC HS52 协商。该 profile 不授予 USB／RNG 或低功耗；不提供其他高速组合、DDR／HS200 | [SDIO 合同](../../../soc/stm32/stm32f4/sdk/hal_config/sdio/README.md) |
| FSMC | 运行期 CPU window、永久 startup SRAM、parallel16，以及显式 PSRAM、有限 NOR program／erase、raw NAND／OOB／ECC 码；bank 与 mux／burst／WAIT 按封装和所选模式授权，无 FTL 或自动纠错 | [FSMC 合同](../contracts/io/f4-external-memory.md)、[存储边界](../contracts/io/f4-flash-storage.md) |
| FMC SDRAM | F479NI Bank1、8 MiB、16-bit、row12／column8；默认 CPU 数据窗口不授予 DMA。显式 `graphics-owned` 通过 `memory_window` 引用，仅给所选 LTDC 只读、DMA2D 读写区域，共同保留原始 bank lease 直至冷退出。两种模式均检查七组时序、上电／刷新／自刷新等待与原 System epoch；WFDIS 共享约束在取得 gate 后实际观察。不授予启动 RAM、XIP、自动 MPU 或实装 RAM 时序资格 | [FMC／外部存储](../contracts/io/f4-external-memory.md) |
| QuadSPI／串行 NOR | F479NI＋W25Q128JV Flash1、24-bit SDR 1-1-1；PIO 读／program／4 KiB erase。可选 `qspi-indirect-dma` 仅把 page-program 的 payload 交给 DMA2 Stream7 Channel3，读／status／verify 保持 PIO；PIO 不分配 DMA 可变扩展。memory TC、QSPI idle、NOR WIP 与 SETTLED 分别证明；取消和失败排空保留贷款。公共 async stop 保留 PREPARED，Platform cold stop 才归还硬件。可选 W25Q128JVSIQ 六 pad、6Bh／8 dummy 的 1-1-4 quad read，与 program DMA 独立；接管须 QE=1、SUS=0，不写状态或恢复外来 suspend。间接 Flash 端点整个生命周期禁止访问 0x90000000 aperture。另选 `mapped_storage/nor-mapped-cpu` 仅与 PIO 控制器组合，静态分配 1..8 reader slots，只授权 live CPU loan 范围；无 Flash 旁路 face、DMA reader 或 XIP。最后释放失败只保留清理，不能继续读取；不提供 DDR／dual 或自动保护修改 | [间接 QuadSPI](../contracts/io/f479-qspi.md) |
| LTDC／DMA2D | F479NI 原生并行 RGB565／RGB888；RGB565／ARGB8888 内存，一／二 layer、2 active＋1 全局 pending。IRQ 在真实 blanking 锁存同代退休证明，task join 后才归还旧 frame；visible 不等于 SETTLED。独立 DMA2D 提供 fill／copy／convert／blend；所选 SDRAM 需显式 `graphics-owned` 连接并持有同一 bank lease。无 DSI、CLUT、隐藏 frame buffers 或对 FMC CPU-only 窗口的 DMA 授权 | [图形所有权](../contracts/stm32-provider.md#f479-ltdc-scanout-and-dma2d-memory-operations) |
| 基础 CRYP／HASH | F415／F417 的有限 IRQ 或独立可选 DMA blocks：AES ECB／CBC／CTR、DES／TDES ECB／CBC，SHA1／MD5／HMAC。DMA 使用精确双流 CRYP／三阶段 HMAC，memory TC 不等于 engine／digest 完成；源与地址撤回、key／IV／context 清理后才结清。HASH DMA 输入／key 限正长度整 word，digest 仍按字节；普通 hash 无 key。caller key 不擦除，无 worker／堆；不提供 DMA 空消息、字节尾部、原地 cipher overlap、AEAD 或 F479 增强 IP 资格 | [F4 实现](../../../soc/stm32/stm32f4/README.md) |
| CRC／芯片身份 | 固定 CRC-32 数值字序列；实际 UID、device／revision 与 Flash 容量。数据不证明完整订货号、封装身份或认证秘密 | [F4 CRC／身份](../../../soc/stm32/stm32f4/README.md) |
| RNG | 唯一 typed RNG factory 与 Platform 生命周期；精确 PLL48 和活跃 System epoch，保留官方 seed／clock／连续值错误检查；不建立 DRBG、熵池或质量认证 | [RNG 时钟合同](../../../soc/stm32/stm32f4/system/README.md)、[公共 I/O](../api/io/system.md) |
| CAN | CAN1／2 classic IRQ、mailbox 与共享 filter domain；F407 不提供 CAN FD | [Classic CAN](../contracts/io/classic-can.md) |
| Ethernet | VE／ZG MII 与 ZG RMII、独立 MDIO；raw `eth` 与增强 `eth_frames` 分选。已选 HCLK 为 25–168 MHz，MDC 与 PTP 20／40／80 ns 增量由计划推导。增强 face 提供整帧 SG、单 C-tag VLAN、显式 checksum offload 和 MAC 硬件事件／PTP 时间戳；这不建立同步或物理精度资格，PHY 接线、协议栈、同步与网络政策由外部提供 | [Ethernet 帧](../contracts/io/ethernet-frames.md)、[MAC／MDIO](../contracts/io/ethernet-mac.md) |
| USB | FS／HS 引擎的 Device／Host、PIO 与 HS AHB DMA；Device 支持 control、bulk、interrupt 和单 transaction ISO packet。Device ISO 使用实际速度的帧窗口，接纳时剩余期限至多 1 秒，并满足受审 FIFO 分区；无 ACK／自动重试或周期调度。内部 PHY 仍为 FS，外部 ULPI 提供 HS／FS 回退；显式 managed roles 与按需 ST 官方 Core 桥接；Host 可用 caller 存储显式绑定周期机会和保守带宽预算，early BUSY 不暴露旧 DONE，无自动派发。同 epoch 提前零长 STATUS OUT 与 DATA IN 可并存，线上闭合后仍须双方向内存 join。不维护高带宽 ISO、自动 HNP／SRP 或 Host split。 | [USB 所有权](../contracts/io/usb-lifecycle.md) |
| USB Device 远程唤醒 | F407 的固定 Device 与 managed Device 阶段按需选择；FS、HS 内部 FS、ULPI 均由所选控制器的唯一 DCD 负责。host SET／CLEAR_FEATURE 在 STATUS IN 真实 ACK 后生效，取消或超时不撤销线上已提交作用。新 suspend 收条至少 5001 µs；有限 request 显式声明 service 延迟 B，满足 Core 时间／B 合同才证明 1–15 ms 信号。cancel／stop、拒撤或角色切换保留原 request／clock／pins，外部 USB 域失电或 CPU 停机不由该合同保证。共享 Flash 开销按实际链接另测 | [远程唤醒合同](../contracts/io/usb-device.md#device-显式远程唤醒) |
| 可选器件组件 | ILI9341 parallel16、XPT2046、W25 等通过公共端口访问；板级供电、存储分区和恢复政策由外部工程提供 | [组件入口](../../../components/README.md)、[I/O 所有权](../../explanation/io-model.md) |

模式矩阵不穷举所有芯片功能的笛卡尔组合。未列出的格式、外部触发、共享资源和
后端组合必须由相应 provider 明确接入；不能从一种模式或公共 API 的存在推定。


## 为自己的工程确认范围

1. 固定完整料号、Board、振荡源、PCB和必要跳线条件。
2. 查询控制器binding、route、mode、内存域和资源冲突。
3. 解析完整TOML；未知字段、未维护模式或冲突必须拒绝。
4. 对自己的源码/配置执行适用模型、ARM或SDK检查。
5. 需要物理结论时取得实际station和原始测量。

```sh
python tools/configure/configure.py list-bindings --board stm32f407_qiming_v31 --kind uart
python tools/configure/configure.py check --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
python tools/dev/dev.py support --assembly tools/configure/assemblies/qiming-baremetal-uart.toml
```

从完整源码根执行。查询给出可选事实和模式，不能代替全装配准入或运行结果。Native需要显式test support，物理MCU保持 `not_executed`，精确边界见[验证参考](qualification.md)。
