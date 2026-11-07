# GD32F303 精确芯片软件参考

维护型号为 **GD32F303RCT6**，LQFP64、256 KiB Flash、48 KiB SRAM，使用名义内部
IRC8 8 MHz。不声明已制造 PCB、LED、外部晶振、连接器或 UART 适配器接线；物理 HIL
**未执行**。

## 模块结构与基础装配

[soc.json](soc.json)声明精确 controller／mode，Board 独立选择绑定。基础夹具为
GPIOA PA4/PA5 和 USART0 PA9 TX／PA10 RX IRQ bytes。GPIO 使用真实 CTL0/CTL1
四位字段，不套 GD470 不兼容的两位 mode／AF 布局。USART0 默认 AFIO、8N1、
oversample16、priority5 和调用者容量的 byte ring。只读 typed ops 接收独立私有状态，
factory 仅返回已有对象，不初始化或分配。

`configure/` 生成不可变事实、精确存储与类型化生命周期调用；CMake 为所选 mode
选择 C provider 与受审共享引擎。`system.c`、`drivers/` 和私有 owner 持有硬件行为，
不加入运行时库选择或公共 vendor 类型。

## 官方库与所选源码审查

锁定 Firmware Library 3.0.3 导入保留精确 HD startup／register 头，**没有导入或
链接官方外设 C 库**。原下载及嵌套归档身份保存在
[source.lock.json](../../../vendors/gigadevice/gd32f30x/source.lock.json)。

2026-11-05 的审查根据两个 SHA-256 独立提取原库：官方 `adc_calibration_enable()`
无限等待 RSTCLB／CLB，`dma_channel_disable()` 写 CHEN 不证明退出，USART／I2C 是
寄存器原语而非完整 request、deadline、接收和取消 transport。这些函数不能结束
已接受借用或替代有界 calibration／撤源。完整模式审查后仍可复用窄 leaf，但不能
引入第二个 live transport owner。原库只作资料参考，没有悄悄进入依赖。

| 机制 | 精确实现与来源边界 |
| --- | --- |
| 启动／时钟／时间 | 官方 HD vector、CPU-only `SystemInit`、固定 IRC8、C TIMER1/TIMER4 owner；hardware carry 仍待 HIL |
| GPIO／EXTI | legacy 四位字段、精确 mask、独立 EXTI0/1；无 F470 AF 布局或连接器承诺 |
| USART0 IRQ bytes | C provider、STAT0/DATA 清错序列、真实 TC 和显式撤源 |
| USART0 有限 DMA | SDK ABI 断言的 channel engine；TX channel3、可选 RX channel4、48 KiB SRAM；不推断 stream/FIFO 或无间隙 RX |
| I2C0 IRQ message | V1 engine＋PB6/PB7 lease；80 kHz、IRQ0；无 kernel IRQ notify、slave、SMBus、DMA |
| SPI0 polling | 有限 C transport、显式 PA4 CS；无已接 slave 或 DMA |
| ADC0／PWM／Flash | 精确 GD wrapper 的 legacy engine；ADC PA0、PWM PA6、bank0 几何和显式供电 |
| FWDGT | 真实 key/reload/status，不可逆 enable；IRC40K 物理边界未表征 |

Errata Rev1.4 PDF 曾重新获取并与 [references.json](references.json) 字节和 SHA-256
匹配。下表覆盖所选软件与各勘误组的关系，不观察 silicon revision，不验证外部电路、
振荡容差或物理 workaround。

| Errata 节 | 所选源码处置 |
| --- | --- |
| 2.1／2.2 | standby、deep-sleep、LXTAL、debug-hold 组合不属于固定 IRC8 |
| 2.3.1 | VBUS disabled 时 PA9 不耐 5 V，USART 要求匹配外部电压，实际 Board 电压未观察 |
| 2.4.1／2.4.2 | enable 后等名义 2 ms 才 calibration，使用 APB/2；外部 VDDA 电容／磁珠及实际延迟仍属硬件责任 |
| 2.6.1 | PWM 和保留 timebase 不用受影响 shadow compare 的 capture/compare DMA，timer DMA 不支持 |
| 2.7.1／2.7.3 | TEN 先于 DMA CHEN，wrapper／engine 保留准入和撤回检查 |
| 2.7.2／2.7.4–2.7.7 | mute、hardware flow control、mute wakeup 未选择；IDLE 有限 RX 不是 mute wakeup |
| 2.8.1 | priority-zero 有界 master RX 窗口，拒绝 kernel ISR notify；实际优先级／总线时序未测 |
| 2.8.2／2.8.3 | 不准入 I2C slave 和 SMBus |
| 2.5／2.9–2.13 | DAC、I2S、SDIO、EXMC、CAN、ENET 没有本精确型号的维护 provider；SDK 头不开放能力 |
| 2.14.1 | Arm776924 涉及 FPU、lazy context 与极短 ISR。所审 baremetal-io ELF 的每条可返回已安装 handler 路径在 return 外至少三条指令，沿 wrapper 跟入 callee；不返回默认循环另计。其他模式和外部 IRQ 仍须遵守，不继承 F103 或物理 FP 资格 |

来源索引保留 `applicability_only`、`not_observed`、`not_fully_audited`；实际逐模式
结果归既有交付台账，资料表不构成整芯片或生产认证。

## 时钟与时间所有权

官方 GD32F30X_HD startup 调用 Nexus `SystemInit`，仅设置 FPU、VTOR、priority
组，不选 PLL、不启动 scheduler。冷事务在取得 timer 前拒绝未知运行 clock、错误
Flash／SRAM 密度、已有 TIMER1 gate 或活动／pending 保留 vector。密度不证明物理
封装；只维持类复位 IRC8，不 reset 外来外设或接管 bootloader 的运行平台。

保留 TIMER1／TIMER4 都是 **16-bit**。TIMER1 的 PSC7、CAR65535 形成名义 1 MHz
低字，Manual Rev3.4 Table16-1 将 update TRGO 接 TIMER4 ITI0，以 external clock
mode0 SMC7/TRGS0、PSC0 在硬件计低字 overflow，不需每圈 IRQ。
低 IRQ 只 ACK 已合并 health flag，高 TIMER4 overflow IRQ 扩展 32-bit 域，必须在
**4,294,967,296 微秒**内服务，含 Flash 停顿和 mask。

high/low/high 快照观察 carry 时重读 low，并修正一次 pending full wrap。它要求
low update 在后续稳定 high/low/high 之前传播到 high，CPU DSB 不能证明 timer bridge
传播。HIL 须观察 ITI0 carry 和无 setup-UG carry；ACK 后 flag 恰好重置也会保守 park。
初始化 UG 时 master 输出跟随 disabled CEN，之后才 arm high slave 并选 low update
TRGO，不把 setup pulse 算作时间。ACK 失败为 terminal：独立隔离各源、保留所有权、
观察 fault 并 park。所有 timer source／flag 撤离后 stop 再 disable／clear vector；
最后撤回被拒保留 clock／owner。stop/restart 使旧 deadline 失效，不扩大 IRQ mask、
NMI/HardFault、IRC8 精度、deep sleep 或动态频率的时间保证。

## 引脚与异步生命周期

Platform 精确租用所选 GPIOA mask：enable bank 后在首次修改前核对全部 powered
nibble／lock，保存原 mode／latch。释放先撤选中 output，再恢复 latch／mode，保留
其他 pin 和已有 gate，仅撤自增 gate。回读失败保留 lease 重试，UART／GPIO 不互关
共享 bank；AFIO／USART gate 同样由冷 C owner 跟踪恢复。

TX request／payload 直到真实 wire TC、撤源后才 SETTLED。cancel／timeout 进入有界
drain，失败保留 QUARANTINED。IRQ 最多搬一个 TX 和一个 RX 字节，按真实 STAT0/DATA
序列采错；RX 满不覆盖旧字节。可选 callback 只提示，stop 须 join 已捕获 publisher
再归还 target。基础 IRQ-byte profile 没有隐藏任务、fallback 或 DMA，有限 channel
DMA 由另外的 `dma-tx`／`dma-duplex` 选择。

C Platform owner 准入精确 controller，租 GPIOA／AFIO，先恢复 pin 再归还 gate。
Platform receipt 为 20 bytes，每个独立 pin lease 为 16 bytes；I2C0 独立租 PB6/PB7，
无第二 owner 重复保存同一选中 pin，回读失败保持未完成清理。

## UART 有限 channel DMA 与 I2C IRQ

USART0 TX 为 DMA0 channel3。`dma-tx` 保留 byte ring，不取得 RX channel4；duplex
另以 channel4 接收调用者 normal block。flag 每 channel 四位，不能套 F470 stream／
FIFO／flag 布局。共享 engine 验证 1..65535-byte buffer 完整位于 48 KiB SRAM 域，
不准入 Flash TX。

生成器为 mode 选共享只读 ops、直接 initializer 和 IRQ。TX-only 只有 compact TX
context／ring，duplex 在同一 prefix 后嵌 RX extension；section GC 删除 TX-only
未选 RX 代码。没有新增 runtime forwarder、配置开关或分配，原借用、deadline、
撤回与 notify 合同不变。

Rev1.4 §2.7.1/2.7.3 要求 TEN 先 CHEN。cold prepare 建立 transmitter，每次 TX
admission 在保留 request／启 DMA 前验证 UEN/TEN。memory TC 不结清 request，须真实
USART TC 和撤源；cancel／timeout 保留借用，被拒 drain 为 QUARANTINED。deadline
后第一次观察完成报告 timeout。

RX 在 DMAR/CHEN 撤源、count 稳定和 ACK 后发布；IDLE 仅报告观察 prefix，不承诺
精确帧界。错误丢弃该块并报 loss，有限边界丢弃 DATA 也计入。service rearm free
block 可有间隙，READY／consumer 不被覆盖；有任何 loan 或撤回未结束，RX stop 仍
BUSY。不提供 circular、无间隙或 hidden-worker 模式。

I2C0 默认 PB6/PB7 AF open-drain，IRC8/APB8 MHz、CCR50/TRISE9，名义 80 kHz、
event/error IRQ priority0。Device limitations §2.8.1 要求更高优先级 RX；有界窗口
保留进入 mask，拒绝 kernel notify。V1 engine 是唯一 transfer owner，不推断 F4
stream DMA。cold owner 独立租 PB6/PB7 与新增 GPIOB/AFIO/I2C0 gate，先 join core／
notify publisher，再恢复 pin／gate，回读失败留责。上拉、电压、电容和 endpoint
未检查，不支持 slave、SMBus、自动 GPIO recovery 或 I2C DMA。

## SPI 与独立看门狗

SPI0 默认 PA5 SCK／PA6 MISO／PA7 MOSI，无 remap，显式 PA4 低有效 endpoint。
有限 8-bit polling 每次最多 256 bytes、有限绝对 deadline、完整 CS interval；8 MHz
分频 2..256 给 31.25 kHz..4 MHz，低于 datasheet 30 MHz 限制。controller 和 endpoint
是不同静态身份，无 IRQ、DMA、隐藏 scratch 或 fallback。

故障后 reset／回读证明排空；无法证明 reset drain 或 inactive CS 的已接受传输保留
active execution、controller、CS 和 buffer，mask IRQ、观察 fault；observer 返回也
park。只有返回的传输才证明 drain／CS inactive 并归还 CPU buffer。独立 idle stop／
recover 失败保留具体 lease 供重试，物理时序、CS setup/hold 和外部电气仍未验收。

FWDGT 只观察 hardware option，startup 不启用或喂狗。FMC option 读取错拒绝准入，
后续读取失败保留已知激活责任，不解释无效 USER bits。产品显式请求名义 activation
和 debug freeze。Manual Table14-1／DS Table4-49 使用 `(RLD * divisor + 1) / 40000`，
名义 25 us..26.208025 s，与 STM32 reload 公式不同。

DS 只提供 IRC40K typical，故 `minimum_timeout_us = maximum_timeout_us = 0`。
名义 programming 成功返回 SUCCESS；启用 state 查询返回 UNSUPPORTED，同时报告
真实 enabled／irreversible／debug。需要保证界限的产品必须在激活前拒绝此未表征
profile，PR/RLD 部分更新也不能建立物理界限。激活前 oscillator/debug lease 仅在
回读后恢复，准备失败保留重试；一旦发 enable key，错误不能停狗或释放责任。
Platform stop 在启用时返回 STATE，reset cause 只 boot-latch，不隐式 ACK 或重启。

## Flash、ADC、PWM 与 EXTI

FMC 暴露完整 bank0 的 128 个 2 KiB page、2-byte halfword program。Board 必须声明
`supply_mv`，参考 3300 mV 是未观察的假设，允许声明 2700..3600 mV。产品明确提供
可写 erase-aligned region，并拥有掉电、watchdog、code placement、恢复策略；驱动
不预留产品分区。除 manual 的 all-zero 例外外检查已擦半字，all-0xff 输入不发 pulse。
timeout 不取消 pulse，当前 BUSY 结束后才返回或启下一次，relock 失败留 owner 重试。

DS Rev3.0 Table4-21 最大 page erase 为 **800 ms**。同 bank code/vector 可停顿，不
承诺紧 IRQ deadline 或 RAM 执行；timer 硬件级联保留低字每次 wrap，不再假设旧
65.536 ms IRQ 间隔。永不 idle 不归还存储；FreeRTOS SysTick 停顿可合并，独立物理
时间保留不等于 kernel tick catch-up。未进行物理擦写。

ADC0 只允许 PA0/channel0、12-bit 独立软件 shot、APB2/2=4 MHz、sample code7。
Rev1.4 §2.4.1 要 enable 后至少 1 ms 才 calibration，本参考留振荡裕量等名义 2 ms；
Board VDDA 去耦／磁珠仍外置。APB/2 避开 §2.4.2 在 ADC<=APB/4 的 immediate-read
hazard，供电保持至 close。物理采样＋转换为 239.5＋12.5=252 cycles，整数分量
保守取整报告 253 cycles。reference、模拟精度、输入接线未验收。

TIMER2 CH0 默认 PA6、固定 8 MHz 分频、period1..65535，显式 Board inactive 和
零／满 duty。stop 先预置／选择 GPIO inactive，再独立撤各 timer source；controller
leaf 失败不能跳过 pad detach，保留 lease，全部退出后 Platform 才恢复原 pin。
TIMER2 不代替保留 TIMER1/TIMER4。ADC/PWM/Flash 与 F103 共享私有 legacy engine，
逐芯片检查 SDK ABI，不增加仅为转发的运行层。

EXTI0/1 只允许 PA0/PA1、独立 vector6/7、GPIOA mux0、调用者定容事件 ring。IRQ
PD W1C 一次并记一个可合并 timestamp 事实，both-edge 方向只采实时电平。overflow
保留旧事实并报后续 loss boundary；stop 撤 mask／trigger／pending／vector 后才恢复
选定 mux，捕获的 wake publisher 结束后才能释放 target／ring。pull、debounce 和
外部电气由 Board／产品负责。

## 软件验证与扩展

`references.json` 绑定 DS Rev3.0、Manual Rev3.4、Errata Rev1.4 实际字节和 anchor。
SDK3.0.3 notice 与最小 startup/header 闭包固定在导入锁。GoogleTest/GoogleMock
对硬件模型运行生产 C，严格 C11 ARM 和 SDK 消费另有证据，不证明 clock 精度、UART
波形或实际 revision 适用性。

六个 `gd32-f303-{baremetal,freertos}-{empty,io,spi-watchdog}.toml` 是精确静态参考。
另外十二个 `tests/contracts/legacy_transport_assembly` profile 覆盖两精确芯片、
三种 transport、baremetal/FreeRTOS 外部 factory 消费；
`tests/contracts/mainstream_assembly/gd303-devices*.toml` 选择上述器件和 PA1 EXTI。
不据此开放 USB/CAN/Ethernet、其他 controller／封装或未实现 mode。新增 clock／route／
mode 必须配套已审 facts、生产代码、RED/GREEN、ARM 和 SDK 退出条件。
