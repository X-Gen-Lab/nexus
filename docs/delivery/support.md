---
id: delivery.support
type: reference
audience: consumer
source_language: zh_CN
---

# 当前精确支持范围

当前生产入口为 STM32F4，迁入的具体实现仍限定 STM32F407VET6／STM32F407ZGT6。
两者共享系列源码，Flash 密度、封装和允许引脚来自精确事实。其他 F4 型号尚需逐项
接入与验证，不能由官方 HAL 或器件宏存在推定支持。

## 型号、板卡与后端

| 对象 | 当前边界 |
|---|---|
| F407VET6 | LQFP100、512 KiB Flash、128 KiB SRAM 与 64 KiB 非 DMA CCM |
| F407ZGT6 | LQFP144、1 MiB Flash、128 KiB SRAM 与 64 KiB 非 DMA CCM |
| 启明 V3.1／核心板／V4.1 | 三个独立 ZG 来源与装配身份，不能互相代替 |
| 天空星青春版 | 独立 VE 板卡事实，不推广到 VG 高配板 |
| 裸机／FreeRTOS | 按实际 assembly、IRQ ceiling 和模式选择 |
| ThreadX | 明确维护的 M4 子集；不继承 FreeRTOS 全部 profile |
| Native | 显式 host 测试 scope，不是生产芯片或 PCB |

精确资源权威是 [soc.json](../../soc/stm32/stm32f4/soc.json)、
[routes.json](../../soc/stm32/stm32f4/routes.json)和各 [Board 包](../../boards/README.md)。
当前 F407 时钟仍为受审 HSE8／168 MHz profile；不能配置任意时钟或运行期变频。

[器件目录](../../soc/stm32/stm32f4/devices.json)只描述官方宏、启动和 IRQ 的
SDK 编译事实，不为其他 F4 型号建立驱动、Board 或运行时模式资格。

## 已有模式与限制

下表是源码维护范围摘要，完整选择须通过实际 resolver；不表示本次重构全部软件
退出或物理测试已经通过。

| 类别 | 维护范围与必要限制 |
|---|---|
| GPIO／EXTI | 受审引脚、静态电气与事件存储；不存在自动消抖政策 |
| UART | IRQ byte/event、word 与明确 block／DMA 模式分别选择；有限 TX、有限 RX、连续 RX 和双向模式不自动互相授权 |
| SPI／I2C | SPI1／2／3 polling、普通与增强 DMA 事务；I2C1／2／3 polling／IRQ／DMA；显式 endpoint 与受审路线 |
| PWM | TIM3／4／5／9／10／11／12／13／14 的 20 个已审通道，各 controller 显式选择一路；APB1 timer 84 MHz、APB2 timer 168 MHz；TIM5 32-bit，其他 16-bit；无隐式共享时基 |
| ADC／Timer | ADC1／2 外部 0--15、ADC3 按封装选择外部输入；ADC1 内部温度／VREFINT／VBAT；普通 shot／scan 最多 16 ranks、软件注入组最多 4 ranks；三个转换器均有独占 TIM3 的有限／连续 DMA |
| Encoder | TIM3／4／5 独占 TI1／TI2 原始正交计数；x2 TI1、x2 TI2、x4 TI12，独立输入反相与 4-bit filter；无 IRQ／DMA／累计位置策略 |
| DAC | 单／双通道 direct、软件或独占 TIM6／7 触发、noise／triangle；有限／循环触发 DMA、三种输入格式与显式空闲码 |
| Flash／IWDG／WWDG／RTC | 精确几何与生命周期；构造不隐式启狗，产品分区与备份域政策外置 |
| CRC／芯片身份 | 固定多项式 CRC-32 字序列；实际 UID、device／revision ID 和半字 Flash 容量；不建立认证或订货号推断 |
| Classic CAN | 所选 mailbox 与共享 filter domain；不提供 CAN FD |
| USB | 所选 OTG FS device PIO；不由此授予 Host、HS、DMA 或全部 class stack |
| Ethernet | 所选 RMII／MDIO 与帧模式；协议栈、PHY 接线及实物行为分别验证 |
| SDIO／FSMC | 所选 polling SDIO、外部窗口与 parallel16；启动 SRAM 为永久 boot owner |
| 可选器件 | ILI9341 parallel16、XPT2046、W25 等通过公共端口访问，板级供电和产品策略外置 |

六个 UART 的 `dma-tx` 共用官方 HAL 有限 TX 和分离 LL IRQ RX，支持 byte／event
接收。六个实例也分别支持 `rx-dma-idle`、`rx-dma-continuous`、`dma-duplex` 和
`dma-duplex-continuous`，复用同一接收、双缓冲和借用排空算法。固定 TX／RX 路由为：
USART1 DMA2 stream7／2 channel4；USART2 DMA1 stream6／5 channel4；USART3
DMA1 stream3／1 channel4；UART4 DMA1 stream4／2 channel4；UART5 DMA1
stream7／0 channel4；USART6 DMA2 stream6／1 channel5。只读 C 硬件事实表同时用于
冷态准入、标志位和 IRQ 选择，不增加可变实例字段。TC 与 DMA 完成分别观察，撤回
失败继续保留借用，W1C 只清理所选 stream。六路也支持独立 9N1 的 `irq-words`
和由调用者提供存储的 `irq-blocks`；9 位数据不经过 byte 接口。其他可选 UART
模式、替代引脚和 DMA 路线必须另外接入。

ADC1／2 通道对应 PA0--7、PB0--1、PC0--5。ADC3 在 VE 开放 PA0--3 的通道 0--3
和 PC0--3 的通道 10--13；ZG 另有 PF6--10 的通道 4--8、PF3 的通道 9 和
PF4／5 的通道 14／15。序列可以重新排序或重复同一输入，最多 16 ranks，重复
输入的 sample time 必须一致；每个使用的引脚都须有明确 Board route。
同步多 ADC 模式尚未接入。16-rank 与三独立实例参考装配见
`tests/contracts/adc_external_assembly/`，不授予真实 PCB 或物理采样资格。

ADC1 的内部通道 16／17／18 分别返回温度、VREFINT、VBAT／2 的原始计数。
`adc1-internal` route 不取得 GPIO；现有采样模式共用静态序列，内部输入固定选择
`sample_times=7` 的 480-cycle acquisition。温度、电压换算和校准由应用负责，
`reference_mv` 仍是名义值。冷态拒绝接管已启用的所需模拟路径，仅撤回本次取得的
TSVREFE／VBATE；失败保留 clock 与清理责任。1 MHz 时间域的启动等待包含取整裕量：
外部输入 4 μs，含内部输入 11 μs。软件参考见
`tests/contracts/multi_controller_assembly/*adc-internal*.toml`；不建立物理采样资格。

三个 ADC 的 `resolution_bits` 均可选择 6／8／10／12，默认 12。各转换器的 poll、
有限和连续 DMA 共用该静态选择，转换周期与触发速率准入随之变化。DMA 仍使用
halfword，样本仍是右对齐的 `uint16_t` 原始计数；分辨率选择不建立模拟精度或实际吞吐资格。

`injected-shot` 独占所选 ADC，以一次 JSWSTART 执行 1–4 ranks，顺序读取 JDR1–4。
保留 6／8／10／12-bit 原始计数，offset 固定为零；全部采样和停止成功后才发布
samples／count，超时或停止失败保持 count=0，失败清理责任仍由实例保留。
不提供同一 ADC 的 regular／DMA 联合、自动注入或外部触发，不建立额外实例状态。
软件参考 `tests/contracts/multi_controller_assembly/*adc-injected*.toml` 同时选择
三个转换器；ZG 还选择独立 TIM3 PWM，二者没有相位同步关系。

共享 ADCCLK 固定为 PCLK2／4，即 21 MHz。冷态改分频前先取得数字接口观察责任，
确认所有 ADC 已关闭，再撤回本次临时 gate；匹配分频时不写 CCR。准备与其他 ADC
启动必须串行，不支持并发热接入或动态调频。软件采样停止回读失败保留设备清理责任，
平台关闭成功前禁止再次采样，但返回后不保留调用者的样本或 count。

DMA buffer 必须位于所选可达域，CCM 不可用于该 DMA。连续 producer 不覆盖已借用
块，缺替换存储时停止并报告 loss；模型或静态预算不证明任意负载下无丢失。
取消、超时和停止失败继续保留 buffer 与恢复责任，直到真实排空并发布 SETTLED。

DAC 由一个 controller owner 独占，按所选通道取得 PA4／PA5；与 ADC 或 SPI 的
引脚冲突必须拒绝。公共 CPU 写入始终接收未移位原始码，`u12-left` 由 C 编码，
数字 DOR 始终为右对齐 12 位，`u8` 观察值为输入码乘 16。TIM6／7 触发只开放
独占的 84 MHz 固定时钟源，API 的 1 MHz 上限是数字配置政策，不是模拟性能保证；
实际分频可查询。noise 是确定性波形，不能用于熵源。stop 保持所声明空闲码，
完整 Platform stop 才禁用 DAC 并恢复原引脚字段；失败保留责任且可重试。两种密度的
`tests/contracts/dac_assembly/` 只提供软件参考，不代表 PCB 接线、负载、精度或
模拟 settling 资格。外部共享触发与物理验收仍未完成。

DAC 的 `trigger-dma`／`trigger-dma-circular` 是可选静态 TX 分面，共用同一个
controller ID；CPU 模式不预留 DMA 状态。调用方提供最多 65535 个编码帧，明确
字节数、对齐和 DMA 可读域；左对齐与双通道 lane 已由调用方编码，保留位须为零。
通道 1／双通道使用 DMA1 stream5 channel7，单独通道 2 使用 stream6 channel7，
同时独占所选 TIM6／7、stream IRQ 与 TIM6_DAC IRQ。IRQ 只锁存事实和可选 wake，
调用方推进 service；不会隐式建立 task、queue 或复制缓冲区。

有限 DMA 成功包含最后数字锁存；首触发输出 idle，尾部可能短暂停 timer、插入
间隔或重复末码，不承诺恰好 N 次模拟转换。循环模式持续借用至取消、超时或故障后
完成排空。SETTLED 之前 request 与帧保持有效且不可改写；无法排空时保留
QUARANTINED，next_service 返回 NEVER，由调用方明确恢复和重试。欠载状态需显式
stop 与 W1C 回读恢复。软件模型和 ARM 链接不授予物理输出性能资格。

静态装配指定唯一 CRC controller owner；factory 只查询既有接口。
`crc32-words` 使用多项式 0x04C11DB7、
初值 0xffffffff、MSB 优先、不反射且无 final XOR。输入为 32 位数值字，长度以字
计；不隐式拼装或补齐字节流。空序列返回初值。每 16 字和最终读回检查绝对 deadline；
失败不改输出、不保留借用，下一次计算重新 reset。输入／输出与 state、CRC 和其
时钟控制区隔离；关闭只撤回新增 gate，失败保持可重试 receipt，CRC_IDR 保持不变。

`nx_chip_info_read` 无需启动外设，直接读取实际 UID、device／revision ID 和
16 位 Flash 容量，不补用配置值、不开时钟、不分配常驻状态。身份数据不代替硅片
启动准入，也不证明完整订货号／封装或认证秘密。Native 显式返回 UNSUPPORTED；
模型与 ARM 链接仍不能证明物理身份、吞吐或实板功能。

SPI1／2／3 共用轮询和有限全双工 DMA 实现，均可选择 8／16 位增强事务；I2C1／2／3
共用 polling、IRQ 和 DMA 的 C 时序计算，`max_hz` 默认 100000，名义范围为
5129–400000 Hz。CCR 向上取整；Fast-mode 使用 2:1 duty 与 300 ns TRISE 公式。
RM0090 §27.6.8 要求特定 PCLK 条件才能达到 400 kHz；固定 42 MHz 下，
`max_hz=400000` 选择 CCR=36，名义约 388.9 kHz。时钟容差、上拉和总线电容仍须实板验证。
异步恢复保留取得的时序与 wake；请求、通知发布者或 DMA 尚未退出时返回 BUSY，
不会先复位硬件。SPI endpoint 的 `lsb_first` 默认 false，可显式选择 true，
覆盖三个控制器的普通 byte face、8／16 位增强 polling 与 DMA；只在空闲后修改
硬件位序，不反转或复制 buffer。I2S／TI 和固定 MSB 的 BMP280 不使用该选择。
SPI3 的 AF6 路线是
PC10／11／12 或 PB3／4／5；I2C3 是 PA8 SCL 与 PC9 SDA AF4，恢复检查两条真实
输入。SPI2 使用 DMA1 stream4 TX／stream3 RX channel0，SPI3 使用 stream5 TX／
stream0 RX channel0；I2C3 使用 stream4 TX／stream2 RX channel3。共享物理 stream
时配置拒绝，不自动替换路线。增强 SPI DMA 事务的每段完成都保持同一 CS，取消和
停止失败保留整条请求与借用，直到所选引擎、线端和 CS 均已撤回。

I2C1／2／3 的 polling／IRQ／DMA 支持显式 7／10-bit master endpoint。
`address_bits` 默认 7，地址为 8–119；显式 10 时为 0–1023，两种宽度可共用控制器。
10-bit read 先完成 transmitter 地址前缀，再以内部 repeated START 发送 header-R；
只有最终 ADDR 才释放接收窗口及 DMA 请求，地址字节不计入 payload。
依据 ES0182 Rev19 §2.10.4，10-bit Standard profile 要求 `max_hz <= 88000`，
或显式选择 Fast `max_hz > 100000`；C 入口另校验实际 CCR，42 MHz PCLK 不豁免勘误。
现有 drain／quarantine 合同保持不变，ARM endpoint 8 字节及异步 state 64 字节不增长。
软件装配为 `tests/contracts/multi_controller_assembly/*i2c-tenbit*.toml`，不证明物理
target、上拉、电容或重复 START 时序，也不提供 I2C target／SMBus／PEC。

ADC1 使用 DMA2 stream4 channel0，ADC2 使用 stream2 channel1，ADC3 使用
stream1 channel2；有限和连续模式均按所选 lane 解释并清除标志。TIM3 是独占触发源，
不同 ADC 的 DMA stream 不冲突也不能同时取得同一 TIM3。软件参考装配见
`tests/contracts/multi_controller_assembly/*tertiary*.toml`，覆盖 VE／ZG 和裸机／
FreeRTOS；它们不授权实际 Board 接线。

WWDG 通过既有 watchdog 接口、静态 factory 和独立 C owner 接入。prepare 不启狗，
显式 enable 先证明 CFR／debug 配置，再以单次 CR 写入激活；未证实的激活也保留不可逆
责任。`window_ticks` 为 1–62，默认 1；名义 42 MHz PCLK1 的 1／2／4／8 分频须保留
至少一个 `0x40<T<W` 合法计数，不能容纳窗口的 timeout 明确拒绝。早喂 BUSY、迟喂
STATE，均不 reload 或等待。未激活时按 CFR→debug→Sleep→gate 逆序恢复，失败保留
清理收条且禁止再次 enable；激活后 stop 保留 System。实际物理 bounds 仍为 0/0 unknown，
不提供 EWI、STOP／Standby、自动喂狗或物理复位资格。复位原因是独立 SoC 能力，不依赖
选择 IWDG。两份参考为 `*wwdg-baremetal.toml` 与 `*watchdogs-freertos.toml`。

## 选择、证据与保留依赖

[派生矩阵](capabilities.md)关联声明模式、受审路线、Board、装配与实际生产 ops。
查询通过只建立配置一致性；模型、ARM、SDK、软件候选和物理 station 记录分别验证。
当前物理 HIL 为 `not_executed`，未知 PCB、跳线、电源与波形仍保持未知。

所有 vendor、子模块和许可证保留；未选 SDK 不进入生产支持或默认构建闭包。
退出的其他 MCU 适配仅保留历史追溯，原始范围见
[重构前固定源码](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18)。
本次重构不能继承旧候选、软件计数或实板结果。

普通 PWM 共用一套 C 实现；以下按通道顺序列出已审路线，仍须由 Board 显式授权。

| Timer | 通道与引脚 | AF | 固定 timer clock |
| --- | --- | --- | --- |
| TIM3 | CH1–4：PA6／PA7／PB0／PB1 | 2 | 84 MHz |
| TIM4 | CH1–4：PB6／PB7／PB8／PB9 | 2 | 84 MHz |
| TIM5 | CH1–4：PA0／PA1／PA2／PA3 | 2 | 84 MHz |
| TIM9 | CH1–2：PA2／PA3 | 3 | 168 MHz |
| TIM10 | CH1：PB8 | 3 | 168 MHz |
| TIM11 | CH1：PB9 | 3 | 168 MHz |
| TIM12 | CH1–2：PB14／PB15 | 9 | 84 MHz |
| TIM13 | CH1：PA6 | 9 | 84 MHz |
| TIM14 | CH1：PA7 | 9 | 84 MHz |

period 表示完整周期，ARR=period−1；TIM5 上限为 UINT32_MAX，其他已审 timer 为
65535，duty 可达 period，保留 CCR 表达 100% 的空间。set 只暂存 duty，在 update
event 生效；周期与 PSC 保持固定。每个控制器由一路 owner 独占，不隐式共享时基或
同步多个 timer。TIM3 与 ADC trigger、TIM4 与 capture 互斥；现有 TIM1 advanced
PWM 仍保守独占共享 vector 的 TIM9。冷态 C 校验精确 timer、pad、channel、分频和
位宽，启钟／撤回失败保留所选 APB gate 的清理责任；已有 GPIO owner 不被覆盖。
`tests/contracts/multi_controller_assembly/*pwm-family*.toml` 同时链接九个独立实例，
不授权 PCB 接线或实际输出时序。

Encoder 复用现有公共接口与 typed factory：TIM3 为 PA6／PA7、TIM4 为 PB6／PB7、
TIM5 为 PA0／PA1，均为 AF2。计数宽度分别为 16／16／32 bits，filter clock 固定
84 MHz；`counting` 选择 `x2-ti1`／`x2-ti2`／`x4-ti12`，两路 filter code 为 0–15。
每个 encoder 独占整个 timer，与同一控制器的 PWM／capture 互斥。

准备成功时 counter 仍关闭；应用显式 start。stop 保留原始 modulo position，
完整 close 先证明输入／counter／DMA 请求和中断源已停止，再撤回本次 clock；失败保留
静态 descriptor 与清理责任。read 的时间是软件观察时刻，不代表某条边沿；不能恢复
漏采的整圈，方向历史、速度、机械零点和累计位置由产品决定。代码不建立 ring、
任务、IRQ 或 DMA。示例为 `tests/contracts/multi_controller_assembly/*encoders*.toml`；
filter code 不构成实测抗噪或物理边沿速率保证。
