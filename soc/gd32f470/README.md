# GD32F470ZGT6 provider

该目录维护精确 GD32F470ZGT6 / LQFP144 的软件实现。公共使用接口在 `io/`；
`private/` 中的厂商 SDK 和实例 storage 仅供生成的装配 translation unit 与
SoC 实现使用，不导出到 `Nexus::Platform` 的公共 include 路径。

`soc.json` 和 `routes.json` 声明物理事实与已复核的有限路线。官方 SDK 3.3.3
保持原样，构建实际验证 `vendors/gigadevice/gd32f4xx/source.lock.json`；厂商
目标的 headers 与 CPU/FPU ABI 私有使用，不带旧 HAL、OSAL 或设备 registry。

[派生能力矩阵](../../docs/delivery/capabilities.md) 直接核对 facts/routes、参考
Board binding、实际 resolver 夹具和所选生产 ops 定义；它不建立新的配置权威，
也不把芯片路线或软件夹具提升为 PCB 接线与物理资格。

| Provider | 首发实现与边界 |
|---|---|
| Clock/startup | 25 MHz HXTAL、200 MHz SYSCLK、APB1 50 MHz/APB2 100 MHz；有界稳定等待与保留责任的回滚；无 USB 48 MHz/动态切换 |
| Time | 独占 TIMER1，1 MHz、64-bit overflow 延伸；IRQ 必须在 2^32 微秒内服务；时钟域只能在启动时建立 |
| GPIO | 授权 mask，单次 BOP set/reset；初值先写 latch，再改变 mode；toggle 要求单一串行 writer |
| UART | USART0/1 IRQ、8N1；USART0 可选 DMA1 CH7 selector4 有限 TX；真正 TC；byte/event RX 或可选 IRQ block RX，独立容量，无复制池 |
| SPI | SPI0/4 的 8-bit MSB first、最多 256-byte polling；SPI4 可选 DMA1 CH3 RX/CH4 TX selector2 有限全双工；端点独立 CS/mode/rate |
| I2C | I2C0/1 100 kHz polling；最多 8 条消息，每条 read/write 1–256 byte；任意消息可读，1/2/末三字节 ACK 时序；repeated START、NACK、仲裁、STOP；每实例独立线路，无 DMA/十位地址/任意 scan |
| Flash | 全物理 1 MiB、256 个 4 KiB 独立 page、2-byte program；范围、erase alignment、1→0 与 pulse 验证；不预留产品分区 |
| Watchdog | 真正 FWDGT enable/feed/reset cause；硬件 option 已启用可识别；enable 不可撤销，无产品健康策略 |
| EXTI | 唯一 line、有界 queue、共享 vector 的静态 16-line 分派；同 vector priority 必须一致；overflow 可见，无 debounce/callback |
| PWM | 固定 TIMER2 CH0、PSC/ARR、CCR shadow；0/100% 明确，拒绝修改共享 period；停止先输出 inactive low；无 capture/break/dead-time |
| ADC | ADC0 12-bit；polling 模式使用 480-cycle sample time；独立 block 模式使用 TIMER2 TRGO、DMA1 CH0 selector0、15-cycle sample time，每块稳定和校准后触发完整 scan；消费者借用期间不覆写 |

UART 正常完成以 TC 为准。GD32 的取消／传输超时通过 USART reset 终止当前
wire interval，会截断帧并生成 RX loss；未观察到 TC 的 reset abort 返回保守
transferred=0，不把装入 DATA 的 byte 数当作 wire 完整字节。TC IRQ 观察时
锁存绝对期限结果，晚到的 service 不改变早已观察到的完成；期限后才观察到
TC 则报告 TIMEOUT 与 TC 证明的完整 byte 数。error-only RX 与 LOSS marker
携带 NO_BYTE，不能把旧 DATA 值当作新字节。reset 无法证明成功时保留
QUARANTINED 与全部 borrow，继续 service/retry，不能复用 request 或释放 buffer。
SETTLED 是 provider 对 request 的最后访问；旧 IRQ 不持有请求引用。

可选 UART IRQ block RX 使用独立静态 state/ops，默认 byte/event 实例不承担
block metadata 空间。冷启动不分配 RX 环，也不打开 RX IRQ；应用通过 rx_start
提供精确 caller-owned block array。IRQ 只向当前 FILLING block 写一个 byte，
满块或 IDLE 发布，error-only 不生成旧 DATA 的伪字节；消费者保持 READY/BORROWED
时不会被覆盖，储存不足锁存 LOSS，下一有效块带可见标记。RX block 属于逐字节
IRQ 接收，不宣称 DMA 环形接收或硬件时间戳。stop 先关闭 RX/error IRQ，再发布
最后有效 prefix，保留全部消费者 loan 并返回 BUSY，直到消费者 drain/release。
独立 TX 服务与真正 TC 契约保持；TX reset 对 RX 造成的间隙也进入 LOSS。

有限 SPI DMA 要求两个非空、不重叠的 DMA 可访问 buffer，长度 1–65535 byte。
controller 和端点共享一条串行执行门禁；接纳失败不改变硬件或持有 buffer。
DMA 的 FTF 不等于线上完成：两条内存通道关闭、IRQ 源撤销、SPI TRANS 清零且
TBE 置位后，才释放 CS 并发布 SETTLED。取消／超时不提前释放 buffer；200 µs
排空期限失败保留 QUARANTINED，直到真实排空。错误和取消的 transferred 保守
为零，不把剩余 DMA counter 当作线上 byte 数。IO 错误要求显式 recover，
恢复不重放请求。UART TX DMA 同样独立观察 DMA 与 USART TC；两类 provider
均不分配 copy pool、DMA stream allocator 或后台任务。DMA1 时钟由装配统一
持有，单设备 stop 只关闭自身通道与 IRQ，不关闭共享时钟。

ADC 冷启动独占 ADC0/1/2 的共享 reset 与时钟分频域；任一 ADC clock 已启用
会在引脚变更前返回 BUSY。外部 clock-gated owner 也必须由调用方保持静止，
该检查不建立 ADC1/2 支持。初始化先启用并观察 GPIO clock，再保存实际选择
引脚的 mode/pull。稳定等待、reset 校准或校准失败时，关闭 ADC0、恢复共享
配置和所选引脚字段，仅释放本次新增的 GPIO clock，保留相邻引脚及其他
时钟变化；失败不发布实例，也不要求调用方 stop。快照只占冷启动栈，不
增加运行时实例存储或依赖 Nexus GPIO provider。block rearm 使用已有所有权，
不再次执行冷启动或恢复仍持有的引脚。

ADC block 模式使用正常 DMA 而非覆盖消费者的循环缓冲。固定 ADC 时钟为
12.5 MHz，每个通道需要 15 个采样周期和 12 个转换周期；请求频率同时受
完整 scan 时间和 100 MHz TIMER2 的精确 PSC/ARR 整数分频约束。块容量必须
是完整 scan 字节数的倍数，且最多 65535 个 uint16_t 样本；启动前逐个检查
所有块的 DMA 可写域和两字节对齐。TIMER2 与 PWM 独占冲突由配置拒绝。

每块结束后先停止 TIMER2、ADC 外部触发和转换，再关闭 DMA 与 IRQ 源；
只有实际读回 EN 已清零才能发布或归还 producer loan。拒绝停机时保留
FILLING 与整个 buffer，等待显式 service/stop 重试。start 接纳块之后，
service 负责两个微秒的稳定期和最长 100 微秒的校准观察，然后才启用触发；
GD 校准因子有效至下一次 ADC power-off，每次 rearm 都重新执行校准。
消费者释放后通过 service 显式接纳下一块，不建立后台任务。块间存在采集
间隔；没有 FREE 块时停止触发，不猜测间隔内错过的 trigger 数。真实 DMA
错误丢弃不完整块并记录一次可观察的块损失。stop 保留 READY/BORROWED，
直到消费和释放完成才收回 ADC、TIMER2 与配置存储。

SPI/I2C/ADC polling 不被描述为低 CPU 异步。I2C 错误后的 reset 仅结清本
controller，不代表外部线路已恢复；recover 必须观察 bus idle 且配置线路均
释放为高。它不生成假恢复脉冲或重放事务。Flash pulse 无法被 deadline
抢占，返回前必须等待 BUSY 清除；产品负责电压、Flash 执行停顿、watchdog
维护窗口和掉电策略。

FWDGT 使用名义 IRC32K 32 kHz 配置。官方 Datasheet Rev2.2 Table 4-19 仅
给出 typ 32 kHz，没有保证的 min/max，因此 `minimum_timeout_us=0`、
`maximum_timeout_us=UINT32_MAX` 明确表示未取得物理 timeout 边界；不能使用
名义数值证明安全时限。IRQ 观察 timestamp 也不等于 UART start-bit 或 EXTI
每条实际电气边沿的准确时间。EXTI pending latch 可能合并多次边沿，事件带
COALESCED 标记；both-edge 方向根据 IRQ 时的输入电平推断。

停止顺序由外部执行 owner 负责：先拒绝 producer、继续 TX service/drain、
停止 UART/EXTI/PWM，释放 SPI/I2C/ADC，静止 GPIO writer，再停止 SoC。SoC
stop 拒绝外部 active/enabled IRQ、DMA、运行中的 controller、SysTick 或 pending
PendSV/SysTick；运行中的 FreeRTOS scheduler 不能被平台偷偷停掉或更换 clock。

资料复核来源：GD32F470xx Datasheet Rev2.2、GD32F4xx User Manual Rev3.3 与
锁定官方 SDK。2026-10-09 下载的 Datasheet PDF SHA-256 为
`19b76c644705999f5062cc780009ade9bd662fc7db4d122029fd8efcf8b73888`。
User Manual PDF SHA-256 为
`c8e1d20d7cde338892d08dcb233345e03f7960256a60a36c755c576d9cd65c13`。
UM §2.3.4 明确给出 F470 独立 4 KiB page erase，provider 使用锁定 SDK 的
`fmc_page_erase()`，不把 16/64/128 KiB sector API 当作 4 KiB 擦除。Datasheet
Table 2-1 的 ZG 条目给出 1 MiB Flash、512 KiB SRAM；UM §1.3.2 与存储图
给出 112+16+64 KiB 主 SRAM、64 KiB CPU-only TCM，其余 256 KiB ADDSRAM
位于 0x20030000。UM 总线矩阵允许 DMA 访问主 SRAM 与 ADDSRAM，TCM 不允许。
默认 linker 仅使用 192 KiB 主 SRAM；ADDSRAM/TCM 是事实声明，不冒充已分配区域。
默认 Board 的接线依据见 `boards/gd32f470_liangshan`。I2C/EXTI/PWM/ADC 的
SoC route 不代表默认板上已资格化的 connector 或外部器件。

实际软件证据由 `tests/contracts/gd32_io_test.c`、`gd32_system_test.c` 执行
生产 `.c` 的独立链接模型，并由 ARM 构建与 ELF 资源门禁覆盖编译／链接。
模型与 ELF 不证明物理 IRQ、clock 精度、波形、复位、Flash 掉电、ADC 精度或
长期负载。当前未连接实板；所有物理资格明确未执行。
