# STM32F4 系列支持包

本包按系列复用官方 STM32F4 HAL／LL 与 CMSIS Device。当前行为实现限定
**STM32F407VET6**（LQFP100、512 KiB Flash）和 **STM32F407ZGT6**
（LQFP144、1 MiB Flash）的固定类型 C 接口。公共接口在 `io/`，厂商类型与可变状态
留在本包私有目录；Board 只授权已审接线，产品任务、分区和恢复策略由外部工程提供。

## 源码与状态归属

| 入口 | 责任 |
| --- | --- |
| `soc.json`、`routes.json`、`references.json` | 已维护精确型号、路线与资料来源 |
| `devices.json` | 官方器件宏、启动文件、IRQ 与时基存在性的编译事实 |
| 根目录 `tools/configure/stm32f4/` | 当前集中配置适配、静态存储与固定连接 |
| `system.c` | 启动、固定时钟、TIM2 时间域与失败退出 |
| `private/stm32f4_platform_lifecycle_impl.h`、`drivers/platform_coordinator.c` | System 与共享资源的同源 C 协调实现及通用入口 |
| `drivers/`、`private/` | 具体模式、实例状态、硬件生命周期与独立回读 |
| `CMakeLists.txt` | 所选源码、官方库与私有编译依赖 |

![FIG-31：共享 IP 与精确 SoC 的源码责任](../../../../docs/assets/diagrams/driver-source-ownership.svg)

图表示构建期源码归属，不增加运行时转发层。公共 face 共享只读方法表，各实例保有
独立 context；factory 只返回已有身份，不初始化硬件。

`devices.json` 的 SDK 编译覆盖不代表其他 F4 型号已具备资源准入、驱动、
Board 或运行时模式；当前具体支持以 F407VE／ZG 配置和执行范围为准。
Board／SoC 中没有 Python 插件。默认集中主机工具仍使用 Python，连接手写 C 的
System／资源与整平台协调器。配置工具只维护一套权威实现；
源码实现、模型回归、ARM 链接和实板资格分别说明。

## 内存、启动与时钟

主 SRAM 为 128 KiB；独立 64 KiB CPU-only CCM 通过 `[memory.regions.ccm]` 显式选择。
`NX_MEMORY_CCM_DATA`、`NX_MEMORY_CCM_BSS`、`NX_MEMORY_CCM_NOINIT` 使用同源生成的
linker／startup 计划，`NEXUS_MEMORY_CCM` 表达所选域。默认装配不增加 CCM 初始化
存储或放置；所有 DMA 域均排除 CCM，不预留产品 Flash 分区。

主 RAM 初始化前先启用并观察所选 CCM clock，再在第一次写入前验证完整初始化计划。
该路径仅依赖 Flash 常量和冷态本地栈，不使用 libc、global 或 heap。`.noinit` 只表示
不复制／清零，不保证复位或断电保留。

`system.c` 建立已审 8 MHz HSE／168 MHz PLL、APB1 42 MHz、APB2 84 MHz。保留 TIM2
以 1 MHz 运行，通过 overflow IRQ 扩展 32-bit counter，IRQ 至少每 2^32 微秒服务
一次，不占 RTOS SysTick。USB FS 另行显式取得独立 48 MHz clock owner，基本启动
不会开启它。初始化分别记录 primary、cleanup、remaining effects；HSI 切换失败
保留责任，不误关活动 PLL。单调时钟尚不存在的 bootstrap 阶段使用明确 poll 界限。
不支持动态变频。

## 官方 HAL／LL 复用与 FPU 约束

官方 ST Device 与 Arm CMSIS 子模块提供寄存器布局、mask、完整 GCC vector／startup。
STM32F4 HAL Driver 为私有状态机提供所选 LL 操作，并仅在 UART `dma-tx` 模式提供
HAL 有限 TX。审核版本、精确 Git、CMSIS 配对、文件摘要和许可证由
[stm32f4-hal.lock.json](../../../../dependencies/stm32f4-hal.lock.json)固定。

厂商头只进入实现与生成 binding。只有 `dma-tx` 选入私有 UART/DMA Handle 与官方
UART／DMA／RCC C 单元；其他 UART 保留各自 provider。不调用全局 HAL 初始化，不启动
HAL SysTick，不宣称全 HAL 支持。[官方驱动合同](../../../../docs/design/official-stm32-provider.md)
定义完整模式的所有权、时间、取消、成本比较和交付要求。

`SystemInit` 开启 CP10/CP11，此 hard-float 平台不关闭 lazy FP context。FreeRTOS
CM4F port 显式开启 ASPEN/LSPEN。因此 ES0182 Rev19 §2.1.2（Arm776924）对所列全部
F407 cut 都是集成约束：当受影响 VDIV/VSQRT 条件可能出现时，中断 handler 在异常
返回之外必须执行超过两条指令。须沿生成尾分支进入真实 callee，不能计算不可达填充
或 literal。外部 handler 同样遵守，修改编译器／优化／handler 后重新检查实际 image。
平台不会绕过内核 ABI 修改 FPCCR，详见
[短 ISR 合同](../../../../docs/design/official-stm32-provider.md#cortex-m4f-short-isr-constraint)。

## 已维护模式

| 接口 | 执行与限制 |
| --- | --- |
| GPIO | 单 port 的 16-bit 授权 mask；原子 BSRR set/reset；串行 toggle；先初值后 mode |
| UART | 六个实例共用 IRQ bytes/events、独立 9N1 words、IRQ blocks、HAL 有限 TX DMA、有限／连续 RX DMA 和 DMA duplex；真实 TC、显式 loss、drain、quarantine |
| SPI | SPI1／2／3 共用短 polling／有限全双工 DMA；每端点 MSB／LSB-first master、mode0–3；普通 byte face 为 8 位，增强 polling 事务支持 8／16 位 word，三个控制器均可选择增强 DMA 事务；polling 最多 256 words，DMA 每段最多 65535 words |
| I2C | I2C1／2／3 polling/IRQ/DMA；42 MHz PCLK、名义 5129 Hz–400 kHz；7-bit 地址、最多 8 条 read/write message，各 1–256 bytes；repeated START、末字节 ACK、NACK／仲裁／STOP 错误 |
| Flash | VE 8-sector／ZG 12-sector 精确几何；声明 2.7–3.6 V 的 x32 program；范围／对齐、完整 pulse 和 cache invalidation；C deadline 不能抢占 erase |
| IWDG | 4–256 预分频、12-bit reload、名义 32 kHz；观察 option 已启用和不可逆软件 enable，debug freeze 显式；当前物理 timeout bounds 为 0/0 unknown |
| EXTI | 静态 line、有界共享 vector 分派、独立事件存储和 overflow；pending 可合并边沿，无 debounce 策略 |
| PWM | TIM3／4／5／9／10／11／12／13／14 按各自通道数显式选择一路；固定 PSC/ARR；TIM5 为 32-bit counts，其他为 16-bit；duty0–period、shadow update；stop 建立 GPIO inactive |
| ADC | ADC1／2 外部通道 0--15，ADC3 按封装选择外部输入；ADC1 内部温度／VREFINT／VBAT；独立 6／8／10／12-bit 软件 shot/scan、最多 16 ranks；各转换器均有独占 TIM3 触发有限／连续 DMA block |
| DAC | PA4／PA5 单／双通道，CPU direct、软件或独占 TIM6／7 触发、noise／triangle；有限／循环触发 DMA；u8／u12-right／u12-left |
| CRC | 多项式 0x04C11DB7、初值 0xffffffff、32 位字输入；同步计算、独占 controller、失败输出保持 |
| CAN | CAN1/CAN2 Classic CAN IRQ、三邮箱、共享 filter domain；无 CAN FD |
| USB | OTG FS device PIO endpoint，独立 USB48 owner；无 Host/HS/DMA/class stack |
| Ethernet | Raw RMII frame/MDIO、调用者 descriptor／RX loan；IP／协议策略外置 |
| Timer capture | 已审 TIM4 edge-capture 路线和有界事件 |
| Advanced PWM | 已审 TIM1 互补输出／deadtime／break，功率级电气安全外部验收 |

IWDG 数据手册 17–47 kHz 是 VDD=3 V、TA=-40..105 °C 条件下的表征；当前没有
声明满足条件的 operating profile，不能直接据此保证时限。名义配置可成功，但启用后
物理界限查询仍可 `UNSUPPORTED`，同时保留 enabled／irreversible／debug 事实。

独立核心板和 V4.1 包各自保留原理图与跳线身份，不替换旧 V3.1。内部 Flash／IWDG
可以显式选择，但不会预留存储或在 startup 启狗。SoC AF 路线不证明 Board 接线、
外部上拉、器件实装或物理支持，用户必须提供已审绑定。I2C GPIO bus-clear 为独立
通用组件；未声明的 ADC 模式仍拒绝或不在 schema 内。

已选 erase-aligned offset image 有明确 startup／vector／linker／ELF/BIN 检查，
不包含 boot 认证、激活或运行镜像 handoff 资格。精确 mode／Board／生产源码见
[派生矩阵](../../../../docs/delivery/capabilities.md)。

## UART、DMA 与执行进度

有限 `dma-tx` 复用同一个 ST HAL owner 与只读 ops；冷态 C 事实集中维护精确的时钟、
pad、TX／RX stream、IRQ 和 selector，HAL admission、接收引擎与资源取得复用
`uart_hardware.c` 中的只读事实，不维护每实例路由副本。
六个实例没有新增可变字段，不使用 TX-only 模式中未选择的 RX DMA／continuous
配置。GPIO 快照只属于冷态 lease；已经取得资源的直接 HAL 初始化无需重复持有快照。

| 实例 | TX／RX DMA stream | selector | APB 时钟 |
| --- | --- | --- | --- |
| USART1 | DMA2 stream7／2 | 4 | 84 MHz |
| USART2 | DMA1 stream6／5 | 4 | 42 MHz |
| USART3 | DMA1 stream3／1 | 4 | 42 MHz |
| UART4 | DMA1 stream4／2 | 4 | 42 MHz |
| UART5 | DMA1 stream7／0 | 4 | 42 MHz |
| USART6 | DMA2 stream6／1 | 5 | 84 MHz |

`tests/contracts/multi_controller_assembly/*uart6*.toml` 覆盖两种封装、裸机／FreeRTOS 和四种 RX DMA／duplex
模式的六实例链接参考。每种模式共享一张只读 ops 表，六个 context 独立；不授权 PCB 接线。

额外 byte/event 路线为 USART3 PB10/PB11 AF7、UART4 PA0/PA1 AF8、UART5 PC12/PD2
AF8、USART6 PC6/PC7 AF8。USART2/3、UART4/5 从 APB1 取得 42 MHz，USART1/6 从
APB2 取得 84 MHz。冷准入在取得资源前检查精确引脚、两个 GPIO bank、clock、gate 和
IRQ。小型只读 cold descriptor 表不在字节热路径使用，不增加可变实例状态。
RX DMA 根据实际 lane 选择低／高 status bank、W1C mask、channel 与 vector；
duplex 的 TX 和 RX 独立清理，停止失败不释放借用。表中只开放固定路线，不能据此
推导未选择的 UART 协议、其他 DMA stream 或引脚已具备资格。

`dma-tx` 的一个 owner 组合官方 HAL 有限 TX 与分离 LL IRQ-byte/event RX，所选
单元 callback ABI 都为 1，实例共享只读 ops。RX 不交 HAL，TX abort 不改变 RX
责任。短 CPU guard 延后抢占的所选 vector，不在 HAL 调用外套长全局 IRQ mask。
HAL DMA handler 每次进入都要求 EN 清除；HT-only 使用有界官方 acknowledgement。
错误撤回被拒时先 mask 此 vector，再交 task 推进 HAL，避免锁定 IRQ poll。
排空／deinit 失败保留 request、pad、gate 和清理责任。私有 bounded atomic 与
nearest-BRR 编译配置依赖受审单 CPU owner，不修改厂商源码。`HAL_GetTick` 使用既有
TIM2 epoch，`SystemCoreClock` 在失败清理中追踪确认的 HCLK，不启动 HAL 或 SysTick。

有限 UART／SPI DMA 只借用已审 DMA 可达域中的调用者 buffer，排除 CCM。DMA TC
不能释放借用，还须 UART TC／SPI idle、memory channel disabled 和 IRQ detached。
排空失败保留 QUARANTINED。`irq-blocks` 是逐字节 IRQ＋IDLE，不是 DMA RX；DMA RX
另选有限或连续双缓冲 provider。拒绝 `dma-tx`＋IRQ blocks；双向显式选 `dma-duplex`
或 `dma-duplex-continuous`。连续模式不覆盖 READY/BORROWED，缺 replacement 时停止
并报告 loss，不虚构错过事件的准确次数。

SPI endpoint 可通过 `[spi_device.<id>]` 的 `lsb_first = true` 选择硬件 LSB-first，
默认 false 为 MSB-first。三个控制器的普通 byte face、8／16 位增强 polling 和 DMA
事务均使用同一冷配置；端点间可以有不同位序，只在线端与引擎空闲后写 CR1，不在
传输中改变格式。内存中的字节或 `uint16_t` 数值保持不变，不隐式反转、复制或改动
DMA 宽度。该选择不用于 I2S／TI；固定 MSB 的 BMP280 驱动拒绝 LSB 配置。
新字段使用既有 padding，ARM endpoint state／只读配置仍分别为 20／16 字节。

SPI 每次 CS 命令都在原子 BSRR 写入后以 DSB 和官方 LL 读取 ODR，核验本 CS latch；
这不证明外部引脚电压。通用 GPIO 写热路径不增加屏障。polling 返回后不再访问调用者
payload，仅保留静态 endpoint 身份与 fault，供显式 recover／close 重试撤回。
普通 DMA 在受理后遇到 assert 失败仍保持 ACCEPTED，未启动两个 engine；release
失败不发布 SETTLED，保留原 request／buffer，逾期隔离后继续 service。增强 DMA
复用既有整链 owner，在最终 CS 释放前保留全部段；关闭跳过已完成撤回的 CS。
原 byte face 的 length 仍按字节计；增强 16 位数据是本机存储的 `uint16_t` 数值，
长度与 NDTR 按 word 计，不做字节序转换或隐式复制。

ADC1／2 外部输入按 DS8626 Rev12 Table7 对应 PA0--7、PB0--1、PC0--5 的通道
0--15。Board 必须明确授权相应 SoC route；新增 silicon route 不修改既有 PCB
接线。序列只能选择 Board 授权 route 中的通道，资源计划仅取得实际使用的唯一
引脚；rank 可重新排序或重复，最多 16 个。重复通道共享同一个 SMPR 字段，
sample time 不一致时在取得资源前拒绝。
软件 shot/scan 保持逐 rank 转换语义，不承诺通道同时采样。

ADC3 的通道 0--3 使用 PA0--3，10--13 使用 PC0--3；ZG 另有 PF6--10 的通道
4--8、PF3 的通道 9、PF4／5 的通道 14／15。VE 没有这些 PF 引脚，故只开放八种
外部输入，仍允许用重复输入组成最多 16 ranks。封装差异由事实和 Board route
验证，不能从芯片头文件中存在 GPIOF 推导引脚已引出。三个转换器共用同一套软件
shot/scan C、只读 ops 和类型化 factory，实例 context 保持独立。

RM0090 §13.3.2／§13.13.16 区分每个转换器的数字接口 gate 和共享模拟 ADCCLK。
本实现固定 PCLK2／4，即 21 MHz，拒绝继承 MULTI、公共 DMA 或 DDS。匹配的配置不再
写 CCR；调整前临时打开尚未可读的数字接口，逐项记录责任，确认三个 ADON 都为零后
才设置分频，再撤回本次临时 gate。启钟或撤回回读失败保留 receipt，由 close 重试。
整个冷态准备与其他 ADC 启动必须串行；不提供动态调频、并发热接入或共享 ADCRST。
停止一个实例不改写 CCR，也不关闭其他实例的 gate。

软件采样在 ADON 写入经过 DSB 和回读确认后起算 3 µs 稳定期。结束时独立回读停止
结果；失败返回 IO 并保留设备清理责任，新采样返回 BUSY，平台责任者须重试关闭。
返回时不再访问调用者的样本或 count，已完成前缀仍由 count 描述。转换器、接口
gate、临时观察 gate、TIM3 和 DMA IRQ 分开记录责任，复用现有状态字节，不增加
可变字段；只读配置新增控制器身份。TIM3 或 DMA IRQ 已被占用时拒绝取得，未取得
的 IRQ 不参与失败清理。

`resolution_bits` 在主机配置时确定，默认 12，可显式选择 6／8／10。静态 C 配置
保存对应的 RES 编码，C owner 在 ADON 清除后通过官方 LL 设置；poll、有限 DMA
和连续 DMA 使用同一选择。依据 RM0090 Rev22 §13.7，每 rank 的转换时间为
所选 sample cycles 加分辨率位数。速率准入和 sequence metadata 使用该完整成本；
降低分辨率不改变 DMA halfword 宽度和 `uint16_t` 原始样本；块容量与长度仍以字节
计，且必须包含整数个完整 scan。分辨率选择不证明实际信号的精度、有效位数或可达
采样率。

`tests/contracts/adc_external_assembly/` 提供两种密度的软件参考装配，使用三个
GPIO bank 的 16-rank 反向序列，以及 VE／ZG 的三转换器独立实例装配；不代表实际
板卡接线。注入组和同步多 ADC 模式继续未实现。ADC1／2／3 分别使用
DMA2 stream4 channel0／stream2 channel1／stream1 channel2，复用有限和连续 C
实现与只读 ops，不增加每实例 route 字段。TIM3 仍是独占触发源，不允许独立 stream
实例暗中共享。VE／ZG 的 DMA 组合参考见
`tests/contracts/multi_controller_assembly/*tertiary*.toml`。

F407 的 ADC1 内部通道 16 为温度、17 为 VREFINT、18 为 VBAT／2。pinless
`adc1-internal` route 共用现有 shot／scan／有限和连续 DMA，不取得 GPIO，也不新增
运行时字段；静态 channels 与已有 cold receipt bit 表达路径身份和清理责任。
21 MHz ADCCLK 下，内部输入只准入 480-cycle sample time。温度、电压和校准不进入
驱动，返回值保持原始计数，`reference_mv` 不代表实测 VDDA。

冷态 prepare 先拒绝已有的所需 TSVREFE／VBATE，取得前记录责任，写后独立回读。
close 先证明 ADC 停止，再只撤回所取模拟路径，最后释放 clock；任一步拒绝保留责任，
其他未取得路径保持原值。为覆盖整微秒时间戳的取整，ADON 后外部输入等待 4 μs，
含内部输入等待 11 μs，三个执行模式使用同一 helper，不保留额外启动时间戳。
这遵循 RM0090 Rev22 §13.10／13.11 与 DS8626 Rev12 的启动和采样时间要求，
不承诺实际模拟精度、功耗或 VBAT 测量条件。

有限 ADC `trigger-dma` 在发布完整块前停 trigger／conversion／DMA，稳定后由显式
service rearm，块间可有间隙。消费者借用不能被覆盖，未释放时 stop 保持 BUSY；
TIM3 与 PWM 独占冲突。独立 `trigger-dma-continuous` 维护双缓冲和 replacement，
进度不足时停止而非覆盖消费者。

普通 PWM 共用 TIM3／4／5 的四通道、TIM9／12 的双通道，以及 TIM10／11／13／14
的单通道实现。精确 pad 与 AF2／3／9 路线见[支持表](../../../../docs/delivery/support.md)。
TIM9／10／11 使用 APB2 的固定 168 MHz timer clock，其他所列 timer 使用 APB1 的
84 MHz。C 从 controller 身份派生 APB、AF 与 gate，不新增实例字段；配置生成只连接
精确静态事实。TIM3 与 ADC trigger、TIM4 与 capture 互斥；现有 TIM1 advanced PWM
仍保守保留 TIM9 共享 vector。TIM2 保留给单调时钟，TIM6／7 为 DAC trigger。

`period_ticks` 表示完整周期，ARR 为 period−1。上限保留了 CCR 表达 100% duty 的
空间：TIM5 为 UINT32_MAX，其他已审 timer 为 65535。set 只更新 duty 的预装载，不改变周期
或 PSC；state 报告已暂存值，实际输出在 update event 生效。长周期不会自动插入
更新事件或额外任务。冷配置含精确 timer／GPIO／channel／pin，C 在启钟前校验；
启钟或撤回失败保留选中 timer 的清理责任，不修改其他 timer。已初始化的外部 GPIO
对象不能被重新绑定。运行时 state 未增加字段；该扩展增加只读静态配置。

每个 port 只有一个执行者，不引入 heap、registry、字符串查找、默认 worker、OS
lock、通用设备引用或预留 copy pool。UART IRQ 搬运字节和事实，task service 观察
传输 deadline，并在 cancel／stop 期间持续运行。晚到 TC 可以结清隔离传输，但在此
之前 request 保持借用。SETTLED 后无 IRQ／provider 引用，adapter／observer 还需
独立排空。

SPI3 的 AF6 pad 是 PC10／11／12 或 PB3／4／5，APB1 为 42 MHz；固定 DMA1
TX stream5 channel0、RX stream0 channel0。初始化先检查选中 vector 的 enable、
pending 和 active，再取得 publisher；恢复只脉冲 SPI3 的 APB1 reset 位。

I2C 的 `max_hz` 默认 100000，允许名义 5129–400000 Hz。公共 C 时序计算向上取整
12-bit CCR，使非整除配置也不超过所选名义上限；超过 100 kHz 时使用 Fast-mode
2:1 duty 和 300 ns TRISE 公式。依据 RM0090 §27.6.8，固定 42 MHz PCLK 下
`max_hz=400000` 选择 CCR=36，名义约 388.9 kHz，不声称达到精确 400 kHz。
该计算不保证实际时钟误差、上拉和总线电容满足规范。
冷态取得后保存 CCR；局部 recover 复用该配置并保留已挂接的 wake，不继承可能损坏的
寄存器内容。请求、通知发布者或 DMA 尚未退出时先返回 BUSY，不复位硬件。
新增 CCR 使用现有 state padding，ARM 可变状态仍为 64 字节；每个只读冷配置增加
4 字节。polling 与异步模式不增加 heap、队列或 worker。

I2C3 固定 PA8 SCL／PC9 SDA AF4、APB1 42 MHz，DMA1 RX stream2／TX stream4
channel3，EV／ER IRQ 为 72／73。C 冷态 owner 从 controller 身份取得两个 GPIO
bank，逆序恢复每个引脚，再恢复原有三个 gate 位；6 字节 receipt 与热路径状态均
不增长。失败保留原 gate／snapshot 和撤回责任，后续 close 可重试。

I2C recover 同时检查 BUSY 与已保留 SCL/SDA GPIO 实时输入；即使 BUSY 清除，只要
线路低就返回 IO。局部 reset 不打 GPIO 恢复脉冲、不重放事务、不保证未来总线可用；
缺线路事实时在寄存器写入前拒绝。

## DAC 输出与触发所有权

`dac.h` 暴露固定格式、物理通道授权、原始码写入、双通道暂存、start／trigger／stop
及数字状态；公共接口不泄露厂商 Handle。`drivers/dac.c` 使用已锁定官方 LL，
`private/stm32f407_dac.h` 保存只读配置和按实例定容的 CPU 状态，不预留 DMA buffer、
worker 或请求队列。生成器只声明静态对象、factory 和 prepare／close 调用。

PA4 对应通道 1，PA5 对应通道 2。单通道只取得对应引脚；双通道通过一次官方 dual
DHR 写入暂存两个值。公共 CPU 输入始终是未移位原始码；`u12-left` 在 C 中左移，
`u8` 的数字 DOR 回读为输入码乘 16。无论格式，状态均返回右对齐 12 位数字计数，
不测量电压。noise／triangle 的偏移加幅度不得超过 4095，noise 不提供随机熵。

软件触发不产生 DMA 请求。当前 CPU／wave 实现没有 DAC IRQ；TIM6／7 为独占
trigger owner，从固定 84 MHz timer clock 取得向下限速的整数分频。API 只准入
1 Hz--1 MHz 数字请求，状态返回实际分频；此政策不是实测模拟更新率或负载保证。
修改 PSC／ARR 时先禁用并观察 DAC TEN，再生成 UG，避免配置过程输出额外样本；
明确清除继承的 OPM／UDIS／URS／ARPE，并回读 divider／MMS。startup 延时从最后一个
输出 enable 之后计，结束前再次检查绝对 deadline。

stop 撤回触发与波形后安装所声明空闲码，完整 close 才关闭输出并撤回新增 gate。
拒绝已活动的 DAC、欠载中断源或 TIM6／7；不清除其他 owner 的 IRQ／状态。停止、
配置回读或 gate 撤回失败继续保留 cleanup receipt，可由同一 owner 重试。Platform
随后恢复选中引脚和 GPIO clock；完整释放不承诺持续驱动空闲电平。

`tests/contracts/dac_assembly/` 提供 VE 左对齐／ZG 8 位软件参考、真实 factory 与 ARM
链接消费者；它们不授权任何实际 PCB。当前没有 DAC 实板或模拟负载资格，物理 HIL
为 `not_executed`。外部或共享 trigger 尚未接入。

`trigger-dma` 与 `trigger-dma-circular` 通过独立 `dac_tx.h` 分面借用编码帧，
factory 的 `nx_factory_dac_tx_ref` 复用同一个 DAC ID 和 controller state；CPU 模式
返回 NULL，不增加 DMA 状态。单通道 8 位用 byte、12 位用 halfword；双通道 8 位
用 halfword、12 位用 word，通道 1 位于低 lane。左对齐码须由调用方预编码，保留位
为零。每次请求最多 65535 帧；缓冲区必须处于明确的 DMA 可读域，不能使用 CCM。

通道 1／双通道只使用 DMA1 stream5 channel7；仅通道 2 使用 stream6 channel7。
静态配置同时独占所选 TIM6／TIM7、DMA stream、其 IRQ 与 TIM6_DAC IRQ。显式
`irq` 指定优先级和是否调用 OS；可选 wake 绑定时核对两个实际发布源的 syscall
ceiling。IRQ 只锁存事实并发送提示；调用方推进 service 和处理绝对 deadline，
平台不创建 worker 或队列。

首个 trigger 输出声明的 idle，随后 DMA 填入第一个 frame。有限 TC 仅表示内存读取
完成；C 代码短暂停独占 timer、建立新的尾部周期，直到新 update 和最后 DOR 均被
观察后才结清。尾部可插入间隔或重复末码，不承诺恰好 N 次模拟转换或模拟 settling。
循环模式跨回卷持续借用，直到 cancel／timeout／故障完成排空；transferred 是最后
一轮已经消耗的字节前缀，不是总播放量。

排空必须回读请求／触发／中断源已撤销、DMA EN 已清除、计数稳定、内存地址已清零、
NVIC 发布者已 join。失败保留 request、payload、clock 与 cleanup receipt，超过
排空预算进入 QUARANTINED。此时 next_service 返回 NEVER，调用方显式恢复并重试
service／stop；不得回收存储或忙等。欠载后须显式 stop 并观察 W1C 清零才能复用。

## CRC 与芯片身份

CRC 是有可变硬件状态的 controller，使用同一静态 typed factory 和 prepare／close
生命周期。四个 Board 包提供片内 `crc` binding；`[crc.signature]` 选择该 binding 和
`mode="crc32-words"` 即可，无需新增 GPIO 或生成行为代码。实例只保留 initialized
和 clock_added 两个标志；官方 LL 执行 reset、32 位 feed 和 read，C 负责时钟归属、
上下文、绝对 deadline 与输入／输出隔离。CRC_IDR 和原有共享时钟保持不变。

输入为明确的 32 位数值字，MSB 优先；初值为 0xffffffff，不反射、不做 final XOR。
空序列返回初值，字节流的拼装和尾部补齐由调用方决定。每 16 个字检查进度，最后
读取后再次检查 deadline；失败不发布部分签名，也不保留输入。输入／输出不得覆盖
provider state、CRC 或其时钟控制寄存器。`tests/contracts/crc_assembly/` 提供 VE／ZG
真实 factory 与 ARM／SDK 链接夹具，不能据此认定实板或吞吐资格。

芯片身份是片内永久只读数据，`nx_chip_info_read` 直接查询，不建立实例、factory
ID、时钟 lease 或缓存。UID、device ID、revision 复用官方 LL；Flash 容量按 RM0090
Rev22 第 39.2 节读取半字，避开锁定 LL helper 的非对齐 uint32_t lvalue。返回实际
容量，即使与装配期望不同也不补成配置值；启动准入仍由 System 的硅片策略负责。
device ID 不足以识别完整订货号／封装，UID 不作为认证秘密。Native 返回 UNSUPPORTED
并保持输出，不生成物理身份；CPU-only Runtime 没有 SoC 后端。

## 冷态取得、逆序撤回与验证

构造是无其他 controller writer 的冷态 task 操作，明确安装 GPIO 电平和已审 AF。
`platform_cold.c`、`uart_cold.c` 和模式专属 cold 头以 C 实现准入、取得、回滚、停止
和回读。被拒 owner 的空 receipt 不能停止或擦除其他 live provider；部分效果保留
receipt，直到同一 close 证明退出。这不建立实测 IRQ、吞吐或物理 Board 资格。

高级构造仅在启动栈保存受影响 pin field／clock bit，后续取得失败先恢复这些字段，
再由外层回滚早先接口。局部快照与 Platform 持久 lease 分开：每个选中 pin 一个
`uint16_t`，另有有效性与 gate 责任。所有 pin mode／lock 在第一次修改前检查，
持有存储按实际装配定容，对齐由所选 ELF 计入。

已有 enabled channel／EXTI line 在改引脚前拒绝。停止逆序排空，先建立 driver
CS／PWM inactive；完整 Platform release 再恢复选中原 pin field／latch，观察恢复
后才撤新增 gate。失败保留精确未完成 lease，不重复恢复已释放 pin，因此完整释放
不承诺持续驱动产品安全电平。最后一个 EXTI vector owner 才关闭 vector；已启 IWDG
是阻止干净关闭的不可逆效果。

`tests/contracts/stm32_model` 用官方寄存器结构和外部事件模型编译真实生产 C，覆盖
时钟回滚、定时器回绕、GPIO 位掩码、UART TC／取消／迟到 IRQ／RX 丢失、SPI 复位／CS、
I2C 接收／重复 START 自动清除／NACK／仲裁／STOP／恢复、Flash 边界与编程脉冲、
IWDG 不可逆失败、EXTI 隔离及 PWM／ADC 错误。
构造模型运行与固件相同的生成 C，在前一 pin 已改后注入 GPIO clock readback 失败，
并检查 shared IRQ release 和 timer 冲突。两种精确密度的独立软件夹具链接全部维护
controller，外部连接器不据此获得 Board 资格。模型、ARM ELF 和资源证据分别记录；
实板断开时，clock／IRQ／电气波形／Flash 掉电／长期负载都未执行。

## 默认平台协调与可选资源切片

默认装配通过 Core 私有 `platform_lifecycle_impl.h` 协调整个平台，F4 私有
`stm32f4_platform_lifecycle_impl.h` 提供 system start、resources acquire、resources
release 和 system release 的手写 C 实现。生成器只写只读配置与固定调用，使编译器
在同一编译单元看到已选配置，裁剪恒定校验、未选 owner 和无诊断路径的元数据操作。
`platform_coordinator.c` 的四个通用入口复用同一实现并保留运行期检查，产品不包含
私有头。硬件算法、顺序、错误映射和撤回控制流只维护在 C 中。

System 路径依次处理基础／所选资源准入、芯片身份、pins 取得与 system start；
失败按实际责任合并 pin abort 与 System 结果。共享取得记录 Ethernet 责任后建立
DMA／SYSCFG；释放先检查永久 startup SRAM，再处理 CAN、Ethernet、pins 和共享
clock，全部完成后才释放 System。可选 owner 用固定 typed hook 连接，未选项不引入
相应链接依赖；没有 opcode、动作解释器或第二份可变收条。

`private/stm32f4_lifecycle.h` 与 `drivers/lifecycle_{pins,gpio,uart,spi}.c` 为
可选 `nx_lifecycle_*` 资源事务引擎提供独立集成切片：外部 System owner 先建立时钟，共享 pins
先取得；目前只覆盖 GPIO input／output、8N1 IRQ UART byte／event 和带静态 CS
的 SPI short-poll。只读配置／ops 与可变资源收条分开，每步只有一份真实资源收条。

该可选切片不替代默认整平台协调器。默认协调器连接原有各维护模式的 C owner，
不要求把 UART DMA／word／block、SPI DMA 等模式改成该切片的 ops。未维护模式仍
明确拒绝。具体模型与实际生成 factory、ARM 镜像分别验证；源码存在不建立全模式
通过、软件候选或物理 HIL。
