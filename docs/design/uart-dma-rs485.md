# UART 接收 DMA、双向 DMA 与电气适配

UART 公共面继续使用 `nx_uart_port_t` 和共享只读 `nx_uart_ops_t`。实例由配置
生成，调用方只保存接口和上下文。新机制按模式选择；既有 IRQ、`dma-tx`、
`irq-blocks` 的状态结构和默认 ops 槽数没有增加。生产代码仍为 C11，没有
heap、设备扫描、隐藏 worker 或默认 OS 锁。生产 owner 是
`soc/stm32/stm32f4/`，当前精确芯片范围为 STM32F407VET6／ZGT6；不由家族目录
推导其他 F4 器件或全部 UART 模式已适配。Native 仅在 `tests/support/native/`
提供显式启用的主机测试支撑。

## 模式与精确资源

| 模式 | TX | RX | 接收特点 |
| --- | --- | --- | --- |
| `rx-dma-idle` | IRQ | NORMAL DMA | 有限块，IDLE 发布稳定前缀，owner 显式 service 重启 |
| `dma-duplex` | DMA | NORMAL DMA | 独立 TX/RX 引擎；接收仍有有限块重启间隙 |
| `rx-dma-continuous` | IRQ | 双缓冲 DMA | 两个预置目标与第三个可用替换块 |
| `dma-duplex-continuous` | DMA | 双缓冲 DMA | 双向独立引擎，连续接收使用相同贷款协议 |

这里的接收／双向 DMA 模式只接入 STM32F407 USART1。RX 是 DMA2 stream2
channel4，TX 是 DMA2 stream7 channel4。其他 UART 的 IRQ／有限 TX DMA 能力
须按其独立配置合同选择，不能推导其已支持这里的接收模式。控制器编号来自芯片手册，
不能把公共 opaque resource ID 的编码当成物理 DMA 编号。配置必须独占 UART、
DMA 目标、向量、时钟和引脚，冲突在生成阶段拒绝。

接收块容量为 1～65535 字节。连续模式至少有三个等容量块；有限模式允许
不同容量。DMA 目标必须落在已授权、可写且 DMA 可到达的区域；F407 的
CCM 不属于接收 DMA 可达 SRAM。F407 没有 CPU D-cache，当前 provider
不执行 cache clean/invalidate。其他芯片的接入必须重新核对内存域和缓存维护
合同，不能复用这里的无 cache 假设。

STM32F407 的 RX-only 使用独立紧凑接收状态，保留普通 UART 前缀的 IRQ TX
能力；只有 duplex 在同一接收 owner 后附加 TX-DMA 扩展。两种模式复用接收
贷款算法，使用各自只读 ops 和直接 TX 调用，没有可选实现指针或运行时
`dma_transmit` 判断。RX-only 不取得或撤销 TX-DMA 目标，其链接不保留 TX-DMA
ops；普通 IRQ 状态与默认 ops 槽数保持不变。这些是所选 F407 实现的结构合同，
具体 RAM／Flash 成本仍以当前配置、工具链与 ARM ELF 为准，不是整个家族的常数。

## 块所有权与连续接收

调用方初始化 `nx_stream_t` 与槽表，再通过 `nx_uart_port_rx_start` 借出。
设备负责 producer，调用方负责 consumer。两者不能混用普通 producer 与
`nx_stream_post_*` 预置 producer API。stream、槽表、所有块和 provider 状态
互不重叠，并在成功 `rx_stop` 和外部 observer 排空前保持有效。活动 TX
payload 不允许与整个 RX 块池重叠。

有限模式直接将 DMA 指向 caller payload，不做 CPU payload copy。TC、IDLE
或错误到达后，先撤销 UART DMA request 和接收中断，再写 DMA disable。
实际 EN、request 与 interrupt 位回读证明静止后，才读取稳定剩余计数。
最后再次读取 DMA/UART 错误，随后执行唯一 status/data 清除顺序。可靠的
正长度前缀才能 READY 发布；不可靠的前缀回退为 FREE 并记录 loss。
stop 时没有收到的空块不会假发布为成功数据。

连续模式以两个 FILLING 贷款启动双缓冲 DMA。TC 后先保留第三个 FREE 块，
再替换已完成的非活动目标。发布旧块前同时证明替换地址、另一个目标地址、
CT 位、EN、无新 TC/错误和旧地址从两个硬件目标中完全消失。失败不会将
硬件仍可能写入的块交给 consumer；它撤源、排空并按 LIFO 撤销未来贷款。
成功发布遵守 FIFO。IRQ 不复制 payload，只做有界控制和至多三个贷款处理。

没有可替换 FREE 块时，立即停止接收写源并等待实际排空，保留 consumer 的
BORROWED 块。稳定的已完成块和部分块仍可发布，同时记录一次停止间隙。
不会猜测丢失字节数。失败的 disable 保留所有 producer 贷款，owner 后续
`service` 有界重试；这段时间调用方不能复用任何块或状态。

TC 是标志位，不是回绕计数器。连续资格要求最坏 IRQ 延迟和目标替换时间
小于一个块的填充时间；必须按实际波特率、块大小、中断优先级、屏蔽窗口和
并发 DMA 负载测量。软件检查能识别可观察的错误、目标变化与 CT 异常，不能
恢复任意多个合并的回绕。违反延迟预算时不能声称数据序列可靠，即使所有
目标始终保持 FILLING 而没有侵犯 consumer 内存。

IDLE、有限块重启和容量压力后的恢复存在接收间隙。owner 的 service 周期
也是显式资源预算。重启前观察到 UART overrun/error 会在清 status/data 前
记录 loss；无错误位的间隙不能凭软件推断精确遗漏数。loss 是已观测边界，
不是精确 wire 字节统计。上层协议决定是否允许间隙及如何恢复。

## TX、停止与调用上下文

TX DMA TC 只证明 payload 已送入外设，不证明最后一位已离开引脚。TX
settlement 继续等待实际 UART TC；取消仍先撤源，再证明 DMA/外设排空。
RX DMA 和 TX DMA 使用不同目标与向量，二者的完成状态互不替代。

`rx_start/rx_stop/service/stop` 由唯一未屏蔽的任务 executor 调用。ISR 只使用
生成器绑定的精确 IRQ 入口；保护区保存并恢复原中断状态。`rx_stop` 关闭
stream admission，可能返回 BUSY：已发布、已借用的块必须由 consumer 正常
归还。成功必须同时满足源已撤销、引擎静止以及 outstanding 为零。关闭后
service 不重新启动 RX。完整 `stop` 也先关闭 TX admission，只有 RX 排空
后才能停底层时钟。失败时不能销毁或更换 owner。

## RTS/CTS 与 RS485

`nx_stm32_uart_flow_configure` 是显式可选冷配置，设置 CR3 RTSE/CTSE，
读回失败会回滚。它在初始化后、活动 TX/RX 开始前调用，不能修改正在运行的
借用。USART1 的可用信号为 PA11 CTS、PA12 RTS、AF7；这些芯片事实不代表当前
四块开发板已经接线或外部设备支持握手，也不授权与 CAN／USB 复用引脚并选。
板级必须审核 PCB、GPIO 权限、电压、复用与 peer 行为再绑定。硬件 RTS
反映 UART 数据寄存器的接收状态，不代表软件块池剩余容量，不能替代协议
背压或在无 FREE 块时自动保证不丢数据。

RS485 选用 `nx_uart_rs485_state_t`、不可变 `nx_uart_rs485_config_t` 和
`nx_uart_rs485_ops`，默认 UART 状态不增加 DE、timer 或 task。调用方绑定
真实 GPIO、唯一授权位、DE 极性、setup/hold 时间以及精确底层 provider 的
纯 TX validator。validator 与 submit/start 共用同一拒绝规则；它不写 GPIO、
MMIO、不 admit，也不留引用。config、两个 face、ops、context 绑定和电气
事实全程不可变，所有冷事实与 mutable request/payload/control 必须分离。
同一 executor 独占底层 TX；不得绕过 adapter 直接提交或修改 transport。

拒绝发生在 DE assertion 前。接受后 DE 建立成功才开始 setup，绝对 transfer
deadline 包括 setup 与 wire 完成。setup 超时不启动 UART。底层实际 SETTLED
才开始 hold，随后 DE 撤销成功，adapter 删除内部 payload/active 引用，再
发布外层 SETTLED。失败的 drain 或 DE 撤销保留 QUARANTINED 外层贷款与 DE
所有权，owner service 重试。必需 drain/hold 可以延长借用超过 transfer
deadline；不能因 deadline 已过就提前释放内存或 DE。

setup/hold 使用 `nx_time_now_us` 的 monotonic 域；service 的最坏间隔决定
边界偏差。hold 保证至少声明时长，并不保证精确到时刻就撤 DE。精确波形
需求需要显式硬件 timer/电气预算，当前不会猜测收发器或提供假定时资格。

## 软件证据和实板验收

GoogleTest/GoogleMock 直接执行生产 provider，覆盖银行替换拒绝、CT 翻转、
地址错写、新错误到达 W1C 窗口、disable 拒绝、终止期间 DMA/UART 错误、
status/data 清除竞态、epoch 耗尽、双向取消、flow 配置、RS485 setup/hold
和 DE 失败。Native adapter 另验证真实请求生命周期。这些结果建立软件
合同，不证明电气波形、总线仲裁或物理延迟。

当前 F4 接收、双向与 TX DMA 实现分别位于
`soc/stm32/stm32f4/drivers/uart_receive.c`、`uart_duplex.c` 和 `uart_dma.c`，
冷态资源归还由同目录的 `uart_cold.c` 管理。公共 RS485 adapter 在 `io/src/`
实现，Native 叶模型位于 `tests/support/native/drivers/`。模式与 factory 装配测试、
寄存器模型和 ARM 链接分别验证对应合同。

历史成对开发 ELF 只描述其记录的固定源码、配置和工具身份；跨系列计数、状态
大小或 text 差值不能继承为本轮 F4 候选预算。当前候选必须重新绑定配置、源码、
ELF／map／nm 与工具身份，并验证 RX-only 不保留 TX-DMA 实现、duplex 保留所需
独立引擎。结构复用不证明零 Flash 开销，也不证明实际 IRQ 周期。

当前没有连接实板，所有物理验收均未执行。实板退出条件包括：两个引擎
并行的最大 baud/负载，最坏 IRQ 与 service 延迟，目标换址和 wrap 压力，
consumer 长持有后的 overflow/recovery，IDLE 与最后 DMA beat 竞态，真实
TC wire tail，RTS/CTS stall/resume，DE setup/hold 极性与边沿，以及反复
stop/start 后时钟、向量和 SRAM 贷款均排空。逐板记录 PCB 修订、探针、串口、
外部接线、逻辑分析仪原始波形和同一 ELF，未执行项不能标成通过。
