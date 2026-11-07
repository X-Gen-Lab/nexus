<a id="stm32u575-single-state-reference"></a>

# STM32U575 单状态芯片集成

本目录维护精确 **STM32U575ZIT6Q**，以 NUCLEO-U575ZI-Q／MB1549 资料为软件参考。
默认 Platform 只接纳 **TZEN=0、SWAP_BANK=0** 的单状态配置，提供 CPU 启动、类复位的
MSIS 4 MHz 时钟、独占 TIM2、精确封装的 GPIOA..GPIOH、USART1／USART2、选定的
有限传输外设，以及由应用显式启用的 IWDG。默认链接脚本使用 SRAM1 192 KiB。
物理 HIL 均为 `not_executed`；源码和链接结果不证明实际 PCB、供电或安全归属。

## 架构、依赖与代码入口

| 入口 | 职责 |
| --- | --- |
| [system.c](system.c) | 主 DATA／BSS 初始化前的 CPU 设置、固定时钟准入、TIM2 单调时间域及终止故障处理 |
| [platform_cold.c](platform_cold.c) | 运行期预检、共享 GPIO／时钟租约、部分失败后的清理和停止 |
| [drivers/](drivers/) | 以 C 实现所选外设的准备、推进、停止、实际读回和排空 |
| [configure/](configure/) | 不可变配置、精确静态存储、类型化 factory 接口和固定调用 |
| [CMakeLists.txt](CMakeLists.txt) | 私有选编实际 SDK 单元与官方 U575 启动代码，不引入厂商原版 `SystemInit` |

GPIO／UART／看门狗实例共享只读方法表，各自持有独立私有状态。factory 返回已有
只读接口，不分配或启动硬件，也不生成任务、后台执行者或运行时注册表。GPDMA
硬件链、RTC、LPTIM 的硬件准入和撤回同样由 C 实现；空收条不授权停止外来责任者，
失败时仍保留自有责任。独立的保留时间／Stop2 组件继续由各自已有的 C 责任者管理，
不改变普通 Platform 的边界。

私有 CubeU5 v1.9.0 输入固定为 15 文件锁，包含选定 LL 头、HAL GPIO C 单元和必需
头文件。LL 复用寄存器编码、只读查询及选定的数据／控制操作。GPIO 的四个既有命令
位置，对已准入的非空掩码调用一次合并的 `HAL_GPIO_WriteMultipleStatePin`；公共
空掩码写操作仍成功。不引入 `HAL_Init`、厂商时基、可变 Handle 或第二个 IRQ／
传输责任者。仅选择看门狗的装配只需锁定 LL IWDG，不要求 GPIO 实例或 HAL GPIO
源码；Nexus 仍在激活前记录不可逆责任，并检查同步和读回。具体范围见
[官方驱动合同](../../../docs/design/official-stm32-provider.md)。

CMSIS Device U5 固定为 `624374fa1e21ca195d6f2102ac0caaa50d0ea4c8`
（v1.4.3、Cube v1.9.0），保留原 Apache-2.0 许可证和 ST 通知，SDK 类型保持私有。

## 模式与装配导航

| 装配／接口 | 维护的精确范围 |
| --- | --- |
| `mainstream.toml`／`mainstream-freertos.toml` | 两路 USART、八个 GPIO 端口、未激活看门狗的静态构造，以及 ARM／SDK 组合 |
| USART1／USART2 IRQ 字节模式 | AF7、8N1、4 MHz、FIFO 关闭，独立状态和环形缓冲区 |
| USART1 `dma-tx` | GPDMA1 channel0／request25 的有限连续 SRAM1 TX，RX 仍使用 IRQ |
| USART1 `dma-chain-tx` | 同一独占通道，最多 3 个硬件块组成连续负载 |
| USART1 `irq-fifo-bytes` | 独立 FIFO 阈值与每次 IRQ 字节预算，不与 GPDMA 混用 |
| `peripherals.toml` 与显式 FreeRTOS 变体 | SPI／I2C／ADC／EXTI／Flash |
| `pwm.toml` 与显式 FreeRTOS 变体 | PA6 PWM，与 SPI1 MISO 分开 |

这些有限传输外设不新增 DMA 引擎，也不自动接管时钟。USART1 的 `dma-tx` 与
`dma-chain-tx` 互斥；普通实现不开放 LPDMA、RX DMA 或自主低功耗。

<a id="startup-and-execution"></a>

## 启动、时间与退出

工装／启动责任者必须在执行镜像前确认选项字节。芯片实现不写选项字节、不修改安全
归属或存储体布局；启动后读取选项寄存器，不能使 TZEN1 启动变成合法的 TZEN0，
也不建立 Secure／NonSecure 隔离。

`SystemInit` 在主 `.data`／`.bss` 初始化前，仅用常量 MMIO 地址和屏障设置 CPACR、
VTOR 与优先级分组，不访问已初始化的上下文、`SystemCoreClock`、libc 或堆。
RAM 就绪后的冷态准入检查实际 MSIS range 4、时钟与状态、相等的 HCLK／APB 分频、
未运行的 PLL／Cache、Flash 就绪，以及未占用的 TIM2／IRQ45。在写入前拒绝未知
运行状态，不复位 RCC／电源、不自动接管既有硬件，也不需要外部晶振。

TIM2 使用 PSC3、ARR `0xffffffff`、IRQ45／priority0，形成名义 1 MHz 时间域。
启动在有限轮询预算内观察 UG 产生的新更新，再确认 UIF 清除后发布有效时间。
在同一个保存并恢复中断掩码的排他区内检查 epoch 和两个计数器字，最多校正一次
待处理回绕；IRQ 至少每 `2^32` 微秒执行一次。

运行期 UIF 确认失败会记录 `timer_error=NX_ERROR_IO`，使时间失效并保留定时器责任，
不递增溢出计数。ISR 的有限操作只尝试一次 DIER／CEN／NVIC 撤回。时间域丢失始终
导致终止：即使隔离成功，仍保留终止中断掩码并调用平台故障观察者；观察者返回则
停驻。无效的零时间不能恢复截止时间的使用者，普通停止／重启也不是此故障的恢复
方式。产品可提供另行审核的不返回恢复路径，但故障不授权 ISR 轮询、调用内核或
释放借用。

停止时观察定时器、IRQ 和本次新增门控的撤回。冷态准入除 CR1／DIER 外，还检查
SMCR、CR2、CCER／CCMR 的触发和通道字段。未开启 APB 时可能读零，因此开启并观察
TIM2 门控后，在写 PSC／CNT／UG 前再次检查；拒绝外来定时器时只释放新增门控，
不清除外来字段或 IRQ。自有设置暂置 `MMS=1`、`CEN=0`，防止 UG 导出复位 TRGO；
更新和清除后，观察恢复为 `MMS=0` 才启动。无法清除用于抑制触发的选择器时保留
所有权。动态变频、运行中的 bootloader 移交、NMI／HardFault 时间调用及深 STOP
连续性均不在此合同内；精度和延迟仍须实测。

<a id="gpio-and-serial-ownership"></a>

## GPIO 与引脚租约

参考 Board 选择高有效 PC7／LD1 和 PA9／PA10 USART1／VCP AF7。PA2／PA3 USART2
及单引脚绑定是软件参考，不证明连接器、器件或电气状态。封装清单中的 111 GPIO
来自独立复核的封装表和图，不表示已授权 111 条 PCB 路线。

PG[15:2] 要求外部先准备 VDDIO2、开启 PWR 门控，并实际读到 `PWR_SVMCR.IO2SV`。
预检在取得门控前拒绝缺失的配置，GPIO 在产生引脚效果前复查。不启用供电监测、
不修改隔离，也不从 IO2SV 推测电压；RM0456 10.4.5／10.6.3 的供电顺序由 Board
电源责任者验证。PG0／PG1 使用 VDD。

冷态装配只租用选定字段，保留邻接引脚和原有共享 GPIO 时钟。只接纳输入／模拟
模式，已锁定、复用功能或输出模式的外来责任者均拒绝。先观察全部门控、验证全部
模式和锁，再保存租约；后续端口被拒绝，不能授权恢复先前尚未修改的端口。回滚先
观察模拟模式，再恢复输出锁存器和精确复用字段，失败时保留责任。GPIO 先预置并
读回输出锁存器，再发布输出模式；BSRR 是直接命令，翻转操作必须与同一引脚的全部
责任者串行执行。启动和停止是特权任务操作。

## USART IRQ 字节模式

USART1 为 IRQ61／PCLK2，USART2 为 IRQ62／PCLK1，均为 priority5、4 MHz、PRESC1、
OVER16、8N1，FIFO 关闭。BRR 是四舍五入后的时钟／波特率比值，范围 16..65535，
接受 baud 62..250000。115200 对应 divisor35，名义为 114285.7 baud、约 -0.794%；
振荡器和对端误差仍须电气验证。

每次 IRQ 至多搬运 1 个 RX 和 1 个 TX 字节。环形缓冲区可取任意正容量，通过比较
实现回绕；读取操作在每个短中断掩码区间至多取出一个排队字节，累计丢失不替换旧
负载。TX 保持借用，直到真实线上 TC 与源撤回后，才以 release 语义发布 SETTLED。
取消或截止时间到期开始排空，失败仍为 QUARANTINED。停止会合已捕获的唤醒回调，
并观察 UE／TEACK／REACK／NVIC 撤回后才归还环形缓冲区和唤醒目标。通知只是提示，
调用内核前须检查选定 IRQ 策略。

<a id="optional-usart1-finite-gpdma-tx"></a>

### USART1 有限 GPDMA TX

[规范 TOML](../../../cmake/package/verification/io_evolution/u575-uart-gpdma.toml)
选择 `dma-tx`：PA9／PA10 AF7、8N1、FIFO 关闭、4 MHz；GPDMA1 channel0、request25／
IRQ29 和 USART1 IRQ61，两个 IRQ 均为 priority5。共享 UART 前缀保留独立 IRQ 字节
接收，选配扩展不增大普通 IRQ 字节模式的状态或方法表。完整负载为非安全 SRAM1
`[0x20000000,0x20030000)` 内的 1..65535 字节。

DMA TCF 不证明线上 TC，也不归还借用。取消活动传输时，使用新观察到的 SUSPF／
IDLEF、合法 RESET，以及通道已禁用／空闲的读回；IRQ 保留实际挂起证明。按 ES0499
2.21.2，在首次 UE 前置位 DMAT 并持续保持，直到整机停止证明 UE／ACK、两个向量和
全部发布者均已会合，才撤回 DMAT；失败保留 QUARANTINED。完整调用推进、源／IRQ／
唤醒会合及恢复见[有限 GPDMA 合同](../../../docs/design/u575-uart-gpdma.md)。

原开发记录包含 40 项真实驱动模型、8 项生成 factory 模型，以及正常 ARM 配置／
构建、资源／保留检查通过；不建立干净源码完整门禁的软件候选、物理勘误资格或
实测周期。HIL 为 `not_executed`；未选择 USART2 DMA、RX DMA、连续模式、低功耗／
自主运行和 TrustZone 代理。

<a id="optional-usart1-finite-hardware-chain-tx"></a>

### USART1 有限硬件链 TX

[唯一规范链式装配](../../../tools/configure/assemblies/u575-uart-chain.toml)
在同一 TZEN0／MSIS4 USART1 路线上选择 `dma-chain-tx`，仍通过公共
`nx_uart_tx_request_t` 借用一段连续 SRAM1 负载，不是分散／聚集列表。最多 196605
字节，还受实际 SRAM1 跨度，以及与全部私有状态、控制对象、环形缓冲区不重叠的
约束。首块直接编程，后续最多两个 65535 字节块由硬件获取，无需逐段调用 service
重臂；末项 `CLLR=0`，`TCEM=LAST` 抑制中间完成事件。

选配的 24 字节可变数组包含两个紧凑排列的 12 字节节点，32 字节对齐使整组位于
同一 64 KiB CLBAR 窗口。类型化 IR 声明放置和容量，冷态初始化与准入检查实际链接
后的 SRAM1 地址。请求、完整负载和节点持续保留，直到链表获取、通道／FIFO／IRQ
以及独立 USART 线上传输全部排空。链表或源撤回被拒时，保留请求以供重试；共享
有限传输引擎证明排空后，才清除硬件链表、源和私有节点地址。

[私有构造合同](private/stm32u575_uart_gpdma_chain.h)与编译期共享算法保留单块及
普通 UART 成本，不新增后台执行者、分配、转发层或 RX UE 循环。不开放循环 DMA、
非连续片段、RX DMA、LPDMA、自主 Stop2 或 TrustZone。主机总线模型、ARM 链接／
保留检查各自建立证据；HIL 仍未执行。

<a id="optional-usart1-bounded-hardware-fifo-bytes"></a>

### USART1 有界硬件 FIFO

独立 `irq-fifo-bytes` 仍使用 USART1 PA9／PA10 AF7、8N1、priority5、TZEN0／MSIS4。
RX／TX FIFO 每方向 8 字节，RX 另有 1 字节 RDR。阈值和每次 IRQ 的 RX／TX 预算
分别显式配置；经读回确认的 RX 非空条件可继续排空未达阈值的短尾，无需另一次
IDLE。每个队首的错误在弹出 RDR 前确认；TX 有空间或 FIFO 已空不能代替线上 TC。
取消和停止必须保留存储，直到源、向量、发布者和 UE／ACK 均撤回。

只有此选配状态增加只读配置引用，普通 UART、初始化及 FIFO 关闭的 GPDMA 模式
不变；与 H743 共用编译期算法和锁定的 U5 LL USART 叶操作。不开放 USART2 FIFO、
DMA／FIFO 组合、非 8N1、流控、TrustZone、自主运行或低功耗。
[FIFO 合同](../../../docs/design/uart-fifo.md)所述原记录包含 45 项真实驱动模型、
9 项正常 TOML／生成 factory 模型，以及选定 ARM／资源／保留检查；不证明抗噪、
时延或吞吐，HIL 为 `not_executed`。

<a id="independent-watchdog"></a>

## 独立看门狗

静态装配只绑定 IWDG／RCC／FLASH／DBGMCU，不启动看门狗或修改 LSI。应用显式选择
启用、喂狗和恢复。RM0456 Rev7 Chapter61 要求激活后才更新 PR／RLR 影子寄存器；
后续任何失败均保留 `enabled=true, irreversible=true`。Platform 停止不撤销激活，
也不释放保留的 LSI 门控。激活前的私有停止操作只重试本次新增的 LSI／调试冻结
变化；失败阻止另一次启用，也可通过 Platform 停止重试。

有界启用过程先观察 LSI／调试写入、影子寄存器同步和 PR／RLR，再重装计数器。
失败边界包含新旧预分频／重装值的混合组合。LSI 启动失败只撤回本次新增启用，
保留原有共享时钟源。当前关闭窗口／提前唤醒中断 EWI，`LSIPREDIV=0`；拒绝接管
250 Hz LSI、外来窗口／EWI 或影子寄存器更新。U5 的 4 位 PR 支持 /4.. /1024，
名义超时为 375 us..131072000 us，最小重装值为 2（RM0456 61.7.3）。DS13737 Rev10
Table84 在声明电压／温度下给出 30.4--33.6 kHz LSI 包络；报告值不是物理测量，
必须在计数器降至 1 前刷新。

仅观察由硬件选项激活的状态，不重新编程。未维护的预分频／更新中选项启动返回
UNSUPPORTED，仍保留不可逆责任，未知超时字段为零。复位原因来自启动时已捕获的
标志，不确认清除硬件；单独 BOR 不虚构额外的上电原因。

<a id="finite-peripheral-selections"></a>

## 有限外设：路由、单位和所有权

| 类别 | 精确软件选择 | 所有权与限制 |
| --- | --- | --- |
| SPI | SPI1 PA5／PA6／PA7 AF5，PA4 低有效 CS | 全双工 8 位、1..256 字节、有限截止时间；TSIZE／CSTART 与 EOT／TXC；先复位再释放 CS，排空失败保留调用帧并停驻；无 DMA／异步准入 |
| I2C | I2C1 PB8／PB9 AF4，独立 HSI16 | 受审 V2 前缀，7 位端点、1..8 条消息、每条 1..256 字节，重复 START／256 字节 RELOAD／最终 STOP 空闲；独立 8 字节 ARM 冷态租约持有 HSION／I2C1SEL，V2 端口停止后恢复，并保留 SYSCLK／TIM2；`TIMINGR=0x10f42f2f` 使 SCL 低于 100 kHz；电气由 Board 确认 |
| ADC | ADC4 PC0／channel1、12 位、sample code7 | 4 MHz 下采样 814.5 周期＋转换 12.5 周期，单次软件采样，无流／触发／DMA；外部先准备 VDDA、PWR 门控、ASV；观察 LDO、校准、4 个 ADC 周期等待、ADRDY；返回前完成 ADSTP，失败则复位或停驻 |
| PWM | 独占 TIM3 CH1 PA6 AF2 | 固定 4 MHz 整数分频，周期 1..65535 tick，占空比 0..100%；先锁存初值再输出，停止时恢复声明的非活动电平；不扩展高级定时器／刹车／捕获 |
| EXTI | PA0／EXTI0、PA1／EXTI1 | IRQ11／12、8 位复用字段步幅、独立上升／下降沿 W1C 寄存器组、有界环形缓冲区和有序丢失；事件为合并观察，不是完整边沿历史；源／IRQ 未撤回前保留环形缓冲区和唤醒目标 |
| Flash | 完整 STM32U575ZIT6Q 2 MiB | 256 个 8 KiB 页、每存储体 128 页、16 字节编程单元；按字节对齐的输入先复制为 4 个字，每单元连续四次 32 位写并保留进入前的中断掩码；仅 TZEN0／SWAP0；截止时间只能在脉冲间停止，已开始脉冲须完成并重新上锁；区域、电压、看门狗、同存储体停顿和掉电策略外置，无选项字节写或产品预留 |

同时选择 PA6 PWM 与 SPI1 MISO 会被资源解析拒绝。ADC 链接夹具要求外部启动／电源
责任者在 `nx_platform_start` 前准备 VDDA／ASV，不虚构 Board 已供电。factory 查询
始终只返回冷态只读身份。

<a id="selected-errata-controls"></a>

## 勘误、供电和 Flash 脉冲控制

审核采用官方 ES0499 Rev12（June2026），PDF SHA256 为
`52dac38c801ae72c42ebffd39296ddf7c80ae378cd555f1a1d1baf49daf86a56`。
即使 SPI CRC 关闭，仍保留 CRCSIZE=7（2.23.1），接受真实复位 CFG1；在 EOT／TXC
后至少等待 1 个实际 SCK 周期加 2 个 1 MHz 定时器节拍，才复位（2.23.7）。轮询
次数是推进界限，不是实测微秒。

I2C 按 2.20.2 在主模式清除伪 BERR 后继续；独立 HSI16 避开 2.20.1 的 4 MHz MSI
容差边界，同时保留 4 MHz 系统时钟／1 MHz TIM2。按 DS13737 Table81 的
15.65..16.25 MHz 包络，低／高时段均大于 5.9 us，SCLDEL 大于 1.96 us，可覆盖
上升时间 <=1000 ns／下降时间 <=300 ns 和标准模式 250 ns 建立时间。封装、焊桥、
上拉和实际总线时序仍须工装测量。冷态准入拒绝外来 HSI／HSIKERON／I2C1SEL
责任者和非默认 HSITRIM，保留出厂校准、不修改微调；源／选择器撤回失败保留租约，
并阻止全局拆卸。不能把 H7 的接收 RELOAD 策略用于 U5。

ADC4 拒绝已启用的 ADC1／2 接口责任者，保持与 2.9.1 同时转换条件分开，不使用
ADC12 预分频或线性校准状态。必须先观察校准已接纳，才能等待完成；稳压器／就绪／
采样标志先清除，再接收新的完成证据。ADC4 Table106 声明参考电压 1000..3600 mV；
外部 VDDA 至少 1.62 V 且不低于 VREF+，配置数字和 ASV 不能测量供电质量。

Flash 在 WDW 置位时不开始擦除；每个 16 字节编程单元连续执行四次 32 位写，
完成后才开始下一操作，排除 2.2.25 的部分写缓冲区／STRT 顺序问题。几何要求每个
16 字节单元在真实擦除后只编程一次，失败或中断后须重新擦除。在解锁前拒绝可见
的非 FF 内容，但 FF 不能证明隐藏 ECC 的来源；全 FF 输入不发物理脉冲。

每个脉冲先观察旧 EOP／错误已清除；BUSY／WDW 撤回后必须有新 EOP，空闲本身不能
证明命令已接纳。RM0456 的 EOP 受 EOPIE 控制，准入要求独占已禁用、非活动且无
待处理中断的 FLASH IRQ；在自有脉冲期间置位并读回确认 EOPIE，NVIC 保持禁用。
清理时先清除模式／EOPIE 并上锁，再观察待处理中断撤回，保留优先级。失败时保留
端口以供重试，并阻止全局拆卸。这些有限传输实现均未选择低功耗、自主运行、SPI
CRC／Ready、高级定时器刹车／ocref_clr 或 Flash 选项编程。

<a id="capability-and-physical-limits"></a>

## CPU 能力与独立低功耗组件

当前 CPU 特征组合包含 DSP、FPv5-SP-D16、精确 8 个 MPU 区域和 8 个物理 SAU
区域。RM0456 Rev7 3.5.2／3.7.1 明定 8 个区域，DS13737 的 “up to 16” 不能作为
精确 16 的保证。配置数量要求 TYPE 精确匹配，受保护 FreeRTOS 也拒绝 16，不将其
视为下界，也不自动发现 MPU。单状态配置不授予 SAU 或 TrustZone 代理。

独立[双域参考](../../../docs/design/u575-trustzone-contract.md)有明确的 Secure／NS
构建和冷态责任者；其无 OS 映射器独立观察并初始化真实 8 或 16 个 MPU 区域，
不改变本实现的 TZEN0 或精确 CPU 特征组合。DWT CYCCNT 保持关闭，直至审核精确
U575 实现；M33 字符串不授权 DWT。SoC ICACHE／DCACHE 外设关闭，其存在也不声明
SCB 缓存行。GPDMA1／LPDMA1 的总线路径和链表语义不同：普通实现只开放上述
GPDMA1 TX，不选择 LPDMA1、循环链表或 RX DMA。SRAM2／3／4、备份 SRAM、Secure
别名不属于其源域；Secure／NonSecure 别名不重复计入容量。

[FIG-14 双域图](../../../docs/assets/diagrams/u575-security-domains.svg)仅表示独立
TZEN1、无 OS 的同步参考，不是默认 TZEN0 Platform 的运行路径。

<a id="separate-retained-time-and-stop2-components"></a>

保留时间组件使用显式公共目标和外部启动／电源责任者，默认镜像不选择。它们与
普通 SRAM1／GPDMA 域分开：

| 目标 | 精确软件边界 |
| --- | --- |
| `Nexus::U575RetainedTime` | 外部准备 LSE／RTC 日历和独立保留 ClockDomain；[合同](../../../docs/design/u575-retained-time.md) |
| `Nexus::U575Stop2` | 同一合同的仅 WUT Stop2／恢复，无 RTOS tick 补偿或 VBAT 连续性 |
| `Nexus::U575Stop2Copy` | 显式非安全 SRAM4 内有限 LPDMA1 channel0 内存到内存复制；[复合退出](../../../docs/design/u575-stop2-copy.md) |
| `Nexus::U575Stop2Lptim1` | 同一独占 LPDMA 通道将一组固定 SRAM4 元组写入 LPTIM1 CCR1／RCR／ARR；[LPBAM 合同](../../../docs/design/u575-lptim1-lpbam.md) |

3 个 Stop2 目标互斥使用同一个具体责任者。停止 TIM2 Platform 前，先关闭 RUN
准入并会合全部用户，保留时间的截止时间继续属于独立 generation。唤醒恢复必须
证明最终外设／DMA 访问、IRQ 撤回和时钟恢复已完成，才可释放或建立新 RUN epoch。
DMA 空闲、唤醒 IRQ 或清除软件账本都不归还复合借用。这些切片不建立通用 LPBAM
调度、TrustZone 睡眠、物理保留或完整 IO-32／SOC-35 退出。

## 实板与执行证据边界

ES0499 Rev12 2.21.1 描述停止位后半段不足半位时间的低电平毛刺会导致 RX 损坏；
W／U 修订无软件规避措施，要求无噪声线路或外置毛刺滤波器，IRQ 错误标志不保证
检测或抗噪。2.21.2 描述清除／重设 DMAT 后丢失 DMA 请求，UE 循环规避措施排除
接收器；所选实现使用上述持续保持 DMAT 的方式，以保留 IRQ RX。寄存器模型通过
不证明具体硅片规避措施有效或工装已获资格。

实际 PCB、焊桥、供电／SMPS、硅片修订和选项字节均未观察。模型证明限定的软件
合同，真实 ARM ELF 证明编译／链接／资源；既有开发数字不因本文中文整理而建立
新执行。HIL 为 `not_executed`，静态指令或时钟估计不等于实测周期、WCET、安全
认证或产品发布。
