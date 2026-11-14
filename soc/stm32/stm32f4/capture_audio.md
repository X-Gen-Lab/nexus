# STM32F4 DCMI、I2S2／3 与 SAI

普通消费者通过 `nx_capture_port_*` 与 `nx_audio_port_*` 操作已经绑定的接口，无需解引用可选方法表。公共入口保留原始 request、stream 和输出指针，具体 owner 继续检查私有配置及活动借用别名。缺失能力返回 `UNSUPPORTED`；公共检查不添加复制、队列或线程。停止生产者不撤销已经发布的消费者借用，service 返回值也不替代 request 的终态。

这里记录 `capture.c`、`audio.c`、`audio_blocks.c`、`audio_sai.c`、`audio_sai_blocks.c` 与同源静态装配的合同。
F407 DCMI／I2S 的 AF 路线来自 DS8626 Rev12，寄存器、DMA 与关闭时序来自 RM0090 Rev22，
精确软件参考为 STM32F407VET6／ZGT6。SAI1 限定 STM32F446ZET6 与
STM32F479NIH6；SAI2 限定 STM32F446ZET6，分别依据 DS10693 Rev11／RM0390 Rev9、DS11118 Rev8／RM0386 Rev6。
这些软件范围不表示摄像头、codec、PCB 接线、电气时序或物理 HIL 已执行。

静态配置、资源责任与公共调用边界参见共享图示。

[F4 源码归属与接口边界](README.md)

图表示当前源码和依赖关系；具体模式、关闭证明和时间域规则以下文为准。

图示区分构建输入与运行期调用，不表示吞吐保证。factory 只返回已有
`nx_capture_t` 或 `nx_audio_t`。Platform prepare 取得资源并配置格式，不启动
CAPTURE、I2SE、SAIEN 或一次 DMA 传输；应用通过公共 face 提交有限请求、启动 RAW／JPEG
storage blocks 或 CM1 帧会话。
Platform close 调用同一个 C owner 排空，失败会阻止 System 退出，保留全部借用和
精确清理责任。没有 heap、运行时名字注册表、默认 buffer pool 或后台任务。

DCMI 的 stream、slot 表、request／RAW frame 控制及每个 payload 的完整容量不得
覆盖 provider 配置、DMA 域表、引脚事实或逐 pin 的恢复 snapshot。所有未来 block
在首个 producer loan 或 request 接纳前一起检查，不能把尚未使用的 payload 尾部
用作仍需读取的配置或关闭收条。此检查不增加 frame／buffer 分配或新的采集模式。

## DCMI 输入与模式

`capture` 选择 `snapshot-raw`、`snapshot-jpeg`、`raw-blocks`、`jpeg-blocks`、`frames-raw`
或 `frames-jpeg`；后两种是有限 CM1 快照会话，不能解释为无缝连续图像。
8-bit RAW 每四个输入样本存入一个 32-bit DMA word；10／12／14-bit RAW 的每个样本
占一个 16-bit 容器，有效数据在低位，不是紧密位打包。VE 最多选择 12-bit；14-bit
需要 ZG 的 PF11／PG15。所有路线 AF13，D0–D7 为
PC6／PC7／PC8／PC9／PC11／PB6／PB8／PB9，D8–D11 为
PC10／PC12／PB5／PD2，PCLK 为 PA6。hardware sync 另取 PA4 HSYNC、PB7 VSYNC；
8-bit embedded sync 不取得这两根 pad 或未用高位 pad。

embedded 必须显式给出 frame-start、line-start、line-end、frame-end 四个 byte
code。官方 `HAL_DCMI_Init` 的 READY 状态只绕过 MSP，仍启用 LINE／VSYNC／ERR／OVR；
owner 先持有责任，再掩蔽并回读 IER。官方 Init 不写 ESUR，因此这里显式写入并观察
四个 `0xff` unmask。栈上 Handle 的 State／Lock 明确初始化，热态 IRQ 和 DMA
不引用该 Handle。crop 参数直接表示输入 clock／line 计数，按寄存器存储，不额外
减一，也不把它们解释成输出像素长宽。JPEG 不允许 crop 或 embedded sync。

`pixel_clock_hz` 是调用者声明的实际输入上界，必须同时满足 54 MHz 电气上限及
PIXCLK/HCLK≤0.4。它用于软件观测预算，不是总线持续吞吐资格；sensor、DMA 仲裁及
并发负载仍需物理验证。DMA2 只取得 stream1 或 stream7、channel1；选择冲突会在
同一资源图中拒绝，包括 stream1 与 ADC3 默认 DMA 路线的冲突。ES0182 Rev19 §2.2.19
列出的受影响访问为 QUADSPI indirect、带 FIFO 的 FSMC 外设读写，以及 DMA2
memory-to-peripheral 写 GPIO。该条没有声明 DCMI＋ADC 通用禁用；这里的 DCMI
P2M peripheral port→SRAM memory port 没有选择上述外部 FIFO／GPIO 写路径。
这不证明全部 DMA 并发吞吐或丢帧边界，实际负载仍需 HIL 验证。

## Snapshot 与 JPEG 前缀

调用者初始化 stream 和 request。RAW 还将同一个 stream 绑定到一份完整、连续的
`nx_frame_stream_t`，请求 `storage_bytes` 与该布局精确相等。RAW 要求 FRAME、
完整存储计数、CAPTURE 退出、DCMI FIFO 空、DMA 总线停止、两个 IRQ 实际排空及地址
解绑全部成立，才发布完整 frame。空 crop 也可能产生 FRAME，因此单有 FRAME
不代表完整 RAW 图像。

JPEG 使用普通 stream，`raw_frame=NULL`。正常短图不会触发 DMA TC：FRAME 到达后
等待 CAPTURE 退出和 DCMI FIFO 排空，再停止 DMA，观察稳定 NDTR。返回的
`base.transferred` 和 block length 以字节计，表示按 32-bit word 对齐的已存储前缀，
包含最后一字中的硬件补零，绝不等于 JPEG 有效 codestream 字节数。应用自行解析
SOI／EOI／长度。
RAW 和 JPEG 每次 snapshot 的存储上界为 65535 个 DMA words，且须完整位于已审
DMA 可达域；CCM 被排除。大图可用相应的 RAW／JPEG storage blocks，但这些块没有图像边界保证。

snapshot 硬件忽略 FCRC，因此 snapshot 的 `frame_divisor` 只能为 1。连续 RAW／JPEG 可
选择 1／2／4，表示输入帧筛选，而非准确输出 N 帧。没有按可合并 FRAME bit 与
NDTR 推导精确帧数的接口。

## 连续 RAW／JPEG storage blocks

应用给出至少三个等容量、四字节对齐的 stream slots 和明确
`max_service_gap_us`。owner 用已有 posted producer 借用两个 DMA targets，并在每个
可信 TC 后取得 replacement、更新非活动 target、再次观察 CT／TC，然后 FIFO
发布脱离硬件的旧 target。块是连续存储片段，不是完整图像或精确帧数。

`jpeg-blocks` 明确选择 CM0、8-bit hardware sync 和 JPEG packing，禁止 embedded sync
或 crop。RM0090 Rev22 §15.5.6 p467 规定每个完整 32-bit word 触发 DMA，帧尾残字补零
后同样触发 DMA。因此它原样发布存储字，不解析 SOI／EOI，不提供精确 JPEG 字节长或
帧边界标志；一个 block 可以跨帧并包含 padding。FRAME 不作为完成依据，连续 JPEG
只启用 DCMI error／overrun IRQ；DMA TC 和两个 bank 的借用交接决定 block 发布。
selected-contract 的单位为四字节 word；普通 stream 的 block length 仍以字节计，
且为四的整数倍。

双缓冲 CT 只能观察当前 bank，不能恢复丢失的两次或三次翻转。两次显式 service 的间隔
必须在 `max_service_gap_us=G` 内。两种 blocks 共用保守准入：容量 `B` 为四字节整数倍，
每个 bank 不超过 65535 DMA words，并满足 `(B-44)*1000000 > (2*G+2)*R`。
`R` 的单位为 byte/s：8-bit RAW 使用 `PIXCLK`，10／12／14-bit RAW 使用 `2*PIXCLK`，
JPEG 使用 `4*PIXCLK`，不能以平均每四拍一个 word 忽略短帧补零。44 B 保守预留一个
packing word、DCMI 四 word FIFO、DMA 四 word FIFO，以及两个独立 AHB master 各一个
SINGLE 在途 word；两微秒另覆盖 tick 观察相位。依据是 §15.5.6／7、§15.8.11 的
packing／FIFO 和 §10.3 的 DMA 双端口；这不是总线吞吐实测。

共用检查收紧了旧 RAW 仅预留 32 B 的过松准入；例如 8-bit RAW 在 1 MHz／10 µs 的 64 B bank
现在拒绝，68 B 可准入，不静默增加应用 buffer。全部 bank 仍由调用者显式提供。

超预算、CT 歧义、替换期间 bank 切换或没有 replacement 时停止并报告 LOSS，不能
覆盖 READY／BORROWED。LOSS 只表示确实观察到一次无法继续的边界，不虚构错过的帧数。
IRQ 只锁存事实，应用必须主动 service，不创建隐式 worker。

取消及关闭可能等待 sensor 帧尾后才看到 CAPTURE 清除；sensor 停钟会使退出失败。
失败保留 request、stream、两个 targets 以及已经取得的第三个 replacement。
显式 `recover` 先观察 DMA 禁用和 IRQ 排空，再只复位独占的 DCMI；绝不 reset 共享
DMA2。丢失 clock／reset／System epoch 历史的借用不能靠恢复成 reset 外观来撤销。

source／DMA 已退出也不撤销 READY／BORROWED consumer；这些块未归还时 stop 返回
BUSY，调用者显式 acquire／release 后重试，才完成整个 stream 的关闭。

## 有限 CM1 完整帧会话

`frames-raw`／`frames-jpeg` 使用 `nx_capture_frames_request_t` 和显式
`nx_capture_port_start_frames(capture, &request)`。调用者提供正
`frame_count`、有限绝对 deadline、完整 stream 及每帧存储；RAW 要求与 `raw_frame`
的精确布局相等，JPEG 将
`storage_bytes` 作为每帧容量。每帧仍受 65535 DMA words 上限和完整 DMA 域约束。

每次 CM1 快照都先完成 source、FIFO、DMA、IRQ 和地址 join，再发布该完整帧并
重启下一帧。两个帧之间存在间隙，不提供 CM0 无缝完整帧或无丢帧保证。

CM0 连续 JPEG 的精确帧边界尚未开放。RM0090 Rev22 §15.5 pp458–462 明确帧同步、
变长 JPEG 和 32-bit packing，§15.8 的 FRAME 状态没有同时锁存 DMA NDTR／CT 的
寄存器。FRAME IRQ 后读取 DMA 指针不能证明该字节属于前一帧；FIFO／最后 word
还可能包含 padding。这里限制的是当前可证明的软件合同，不否认芯片能够连续接收
JPEG。`jpeg-blocks` 已覆盖无帧界的 CM0 存储字流。当前完整帧替代是上述 CM1 rearm；
如产品确需 CM0 完整帧，应先明确传感器边界握手、
最坏 IRQ／DMA 观测界限或外部帧解析接口，再建立单独可验证的合同。

没有 FREE slot 时 source 保持关闭，等待调用者归还 consumer block；会话的绝对
时间预算继续消耗，驱动不覆盖 READY／BORROWED，也不分配替代 buffer。

会话 `base.transferred` 统计已成功发布的完整帧数，失败保留这个有效前缀；JPEG
每帧 block length 以字节计，仍含 DMA 最后 word 的硬件 padding，不是 codestream
字节数。stop 取消并 join 当前 producer／硬件责任，不能撤销之前已发布的 consumer
loans。
应用独立归还这些 loans 后才能回收 stream、控制或 payload。

同一会话保留最初 System epoch，包括每个 deliberate source-off 间隙。clock 历史
丢失、DMA／IRQ／地址拒撤时保留 request、stream 和当前存储，不把重新启动外设
或寄存器 reset 外观当作过去借用已经退出。该合同只规定软件退出与帧发布边界，
不建立 sensor 配置、电气连接或实测图像质量资格。

`tests/contracts/f407_capture_audio/ve-frames-raw.toml` 与 `frame_session.c`
组成完整的公共软件消费者：显式提供三个128B slot、8×8 RAW12 的16-bit容器布局、
两个完整帧和绝对 deadline。输入源必须产生该声明的图像；夹具不猜测或配置 sensor。
第一个已发布 consumer token 保留到 producer stop 成功后，再单独 release；
后续帧使用自己的 token。错误仍消费已发布前缀，不能把 session 错误解释成全部帧
失效，也不能把 stop 成功解释成 consumer 已归还。任何撤回或 release 失败都保留
静态 request、stream、token、payload 和 Platform 责任，不丢弃仍可恢复的借用。
这些同一生产 C 路径的前缀、consumer 持有、迟到完成和失败排空由现有完整模型程序
检查；ARM／SDK 链接额外验证公共 factory、配置和源码闭包，均不替代物理执行。

## 音频存储布局查询

应用通过同一个 `nx_audio_t` 和 `nx_audio_port_layout` 查询已准备实例的存储事实；
不能仅凭 24-bit／32-bit 采样精度推测内存顺序。查询不分配、等待时钟或转换数据，
原始 output 指针直接交给 provider 检查，不用临时结果绕过私有控制与借用别名约束。

| 实现 | 有效采样位 | 每个样本的内存容器 | 有效位位置／存储顺序 |
| --- | --- | --- | --- |
| I2S | 16 | 16 bit | LSB；little endian halfword |
| I2S | 24 | 32 bit | MSB；HIGH halfword 后接 LOW halfword |
| I2S | 32 | 32 bit | 全部有效；HIGH halfword 后接 LOW halfword |
| SAI | 8／10／16／20／24／32 | 8／16／16／32／32／32 bit | LSB；native little endian |

`sample_alignment` 表示容器内有效位的位置，不是 buffer 地址对齐；`container_bits`
表示内存宽度，不是包含 padding 的 wire slot 宽度。`slots` 和 `active_slots` 明确
每帧的 wire slots 与有效 mask。SAI 的 FIFO 只传输有效 slot 的样本；
`nx_frame_layout_audio` 按有效 mask 的置位数计算紧凑存储，不为未启用 slot 填充。
I2S 仍保留两个存储 channel slots，PDM lane 的存储规则也不改变。
SAI 10-bit 数据使用既有 frame planner 的 16-bit 容器，不能把该格式传成 I2S。

layout／rate 的结果不得覆盖完整 System 对象、私有 state／config、资源描述表、引脚
恢复 snapshot 或任一在借控制与 payload 的完整 capacity。查询没有新增每实例状态；
只读共享 ops 增加可选 layout 方法，原接口仍直接进入同一个硬件 owner。

## I2S2／3 格式、时钟与有限 DMA

`audio` 选择 master／slave 与 tx／rx／duplex 六种模式，支持 Philips、MSB justified、
LSB justified、PCM short／long，16／24／32-bit data，16 或 32-bit wire slots。
16-bit data 在 32-bit slot 的低 16 位由硬件补零，内存仍只用一个 halfword；24／32-bit
每个样本用两个 halfwords，内存按 HIGH halfword、LOW halfword 的 DMA 顺序存储，
不是 native little-endian uint32 PCM。I2S／justified frame 含两个 channel slots；
PCM frame 是一个单通道同步周期。owner 不转换数据、复制音频或选择 codec。

I2S2 用 PB12 WS／PB13 CK／PB15 SD AF5，extension RX 用 PB14 AF6，MCLK 用 PC6 AF5。
I2S3 用 PA15 WS／PC10 CK／PC12 SD AF6，extension RX 用 PC11 AF5，MCLK 用 PC7 AF6。
不选择 extension 或 MCLK 时不取得对应 pad。主接收、主发送及 slave simplex 分别
使用其精确 DMA request；duplex 由主单元 TX 配合 extension RX 共享 CK／WS。普通
SPI2／3 personality、pad 或 DMA 路线冲突在配置期拒绝。

master 显式选择 PLLI2S N／R、目标采样率和最大名义分频 ppm 误差。PLLI2S 复用 System 的
source／M；生成器计算精确有理数名义 rate 和 I2SDIV／ODD，不把分频误差当作振荡器物理精度。
两个音频 owner 共享同一个兼容 PLLI2S lease；最后一个关闭者才撤回新增 PLLI2S，
不会修改主 PLL 或共享 DMA1。slave 不取得 PLLI2S、不生成 MCLK，rate 查询返回
0/0 unknown，不虚构外部 master 采样率。

TX 接受现有 `nx_frame_dma_loan_t`，RX 从调用者的 framed stream 借用一个 fill；
duplex 要求两个方向等长完整块。DMA 以 halfword 为单位，每方向最多 65535 个。
外部 slave 时序由应用协调：准备时 CK 保持所选 idle level，WS 在 Philips 为 HIGH、
其他标准为 LOW。只有 acquire-observed ACTIVE 才启动外部 clock。slave TX 的 ACTIVE
需要真实 DR prefill、精确 NDTR=N−1、TXE=0、使能后至少三个实际微秒 tick 及
三个外设 clock，并再次确认 CK／WS 仍保持 idle。

TX 关闭观察 DMA EN0、TXE1、随后新读到 BSY0，再撤销 I2SE。主 RX 的 BSY 始终为0，
因此采用 RM0090 专用序列：按格式在 n−1 或最后一个 DMA halfword 后，用保留原值的
DWT CYCCNT 观察 1 CK（16-bit extended LSB 为17 CK），撤销 I2SE，必要时从 DR 取
最后一个 CPU halfword。迟到的 RXNE／OVR 不能变成成功证明。DWT 的新增使能位也有
共享收条，最后关闭时恢复。有限主 RX 至少需要两个存储 halfwords。实际 IRQ 延迟、clock 波形与关闭窗口仍需物理 HIL 验证。

错误／取消报告零个有效音频字节。DMA TC、NDTR0、timeout 或停止 clock 都不直接
归还借用；Serializer／DMA／IRQ／地址全部排空后才归还 TX 或发布 RX。任何失败
保留完整 request 和未归还 loans，close 不允许 System 提前退出。所有 deadline
包含最终排空，重新建立 System epoch 后的旧借用保持 quarantine。

## 连续 I2S blocks

显式 `master-tx-blocks`、`master-rx-blocks`、`master-duplex-blocks` 和对应
`slave-*` 模式使用独立的连续 state/config。普通有限实例不增加运行时字段；
同一 `nx_audio_port_t` 的只读 ops 追加可选 `start_blocks`，有限表该项为 NULL。
连续表不提供有限 `submit`／`cancel`，会话通过已有 `service`／`stop` 推进和退出。
factory 仍然只查询已有静态对象，不把一个控制器同时装成两种 personality。

调用者准备 `nx_audio_blocks_request_t.base`、显式 `block_bytes` 及所选 TX／RX
`nx_stream_t`。两方向控制、slot 元数据和全部 capacity 必须互不重叠；每块是完整
音频 frame 的整数倍，每方向最多 65535 个 DMA halfwords。格式仍遵守上述原始
halfword／slot 合同，不复制、重排或转换数据。

TX 至少提供两个 READY block。provider 取得两个精确 consumer token，作为两个
DMA bank；每次可信 TC 后先取得下一块、替换非活动地址，再回读 CT／事件和时间预算，
最后归还旧 block。数据生产者通过原 stream 的 producer 操作补充后续 READY block。
RX 至少提供三个 FREE slot，复用 posted producer 同时借用两个 FILLING bank；
先取得第三块并完成相同的地址替换证明，才 FIFO 发布旧块。消费者使用原 stream
acquire／release，仍可持有已发布块；provider 不覆盖 READY／BORROWED。

装配必须声明 `max_sample_rate_hz` 和 `max_service_gap_us`。master 的精确有理数
rate 不得超过前者；slave 的值是调用者必须保证的外部 clock 上界，rate 查询仍为
0/0 unknown。block 时长必须大于两倍 service 上界加两微秒裕量。每次 service
及地址替换前后观察真实单调时间、CT、硬件和 IRQ 锁存 TC／error；无法区分多次回卷
时停止并报告 LOSS，不从一个 CT bit 推造精确丢失样本数。IRQ 只锁存事实；source
OVR 等 level 错误先撤 ERRIE，使任务能够执行完整排空。

这些预算不是物理吞吐资格。持续运行要求应用在预算内推进 service、补充 TX READY
和归还 RX consumer blocks；没有隐藏 worker、默认 buffer pool、软件补静音或自动
重放政策。违反外部时钟或服务预算时，硬件在软件观察之前的欠载、回卷及 wire 行为
不能由软件结果追溯保证。duplex 两方向独立观察 bank，内存块不附加伪造的 aperture
时间或相位同步测量。

slave TX 的 ACTIVE 仍要求 DR prefill、NDTR=N−1、TXE0、三个实际微秒 tick 和
稳定 idle CK／WS；之后才能启动外部 clock。stop 先撤请求、观察 DMA／serializer／
IRQ 与全部地址真正退出，再归还两个 TX token 和撤销未完成 RX fills。主 RX 停止
额外观察实际 DWT 的 terminal bit 时间；当前部分 block 不发布为完整 frame。
拒撤、地址不清、未退出 IRQ 或 time-domain 历史丢失时保留 QUARANTINED 会话和
全部存储，Platform close 不归还其时钟。普通可恢复拒撤可在同一实例重试；历史丢失
不因把寄存器恢复成 reset 外观而自动解除。

`SETTLED` 是 provider 对会话及 producer/consumer 权限的最后访问。之前发布的 RX
consumer loans 继续有效，应用需独立 join 它们再回收 stream 或 buffers；未被 DMA
取得的 TX READY blocks 仍在原队列。`base.transferred` 在选择 RX 时统计已发布 RX
字节，否则统计已脱离 DMA 并归还的 TX 字节；错误／取消也可返回先前完整前缀，
不代表实际播放／采样的精确 wire frame 数。整段持续会话可显式使用
`NX_DEADLINE_NEVER`，或由一个绝对 deadline 触发有界排空。

## F446／F479 SAI 有限帧与连续块

SAI 复用公共 `audio` request、frame stream 与借用状态机。两个 personality 共用
同一份冷态配置、System 时钟和 source／sink 关闭实现；有限与连续分别选择静态
ops 和 state，不经过 I2S 转发层，也不向有限实例添加双 bank 或队列字段。
SAI1 在 F446ZE／F479NI 提供六种 `master-*`／`slave-*` 的 `tx`、`rx`、`duplex`，
及对应 `-blocks` 模式；SAI2 在 F446ZE 复用同一实现，保留自己的 gate、IRQ 和
DMA 路线。AC97、SPDIF、PDM、跨 SAI 同步与运行期格式切换不在所选范围。

`frame_sync_*`、有效位宽、wire slot 宽度、偏移、位序和采样边沿均为静态配置。
I2S 的 `standard` 或 PLLI2S 字段不属于 SAI。`active_slots` 可以省略，默认全部
有效；显式值必须是非零且不越过所选 `slots` 的 mask。RM0390 §28.3.7／RM0386
的同源 SAI slot 合同规定，未启用 slot 不访问 FIFO，也不产生相应 DMA 样本。
因此查询的 `slots` 保持 wire 几何，`active_slots` 保留 wire 位置；存储每帧包含
`popcount(active_slots)` 个自然 8／16／32-bit 容器，按有效 slot 升序排列，不做
软件重排或补零。24-bit 样本为小端 32-bit LSB 对齐，10-bit 样本为 16-bit 容器。
`nx_frame_layout_audio` 可直接消费查询值，计算相同的紧凑内存长度。

| IP／时钟源 block | 精确 pads | 可选数据与 MCLK | 所选 DMA2 |
| --- | --- | --- | --- |
| SAI1 A，AF6 | PE4 FS_A、PE5 SCK_A、PE6 SD_A | duplex 接 PE3 SD_B；PE2 MCLK_A | stream1/channel0 |
| SAI1 B，AF6 | PF9 FS_B、PF8 SCK_B、PE3 SD_B | duplex 接 PE6 SD_A；PF7 MCLK_B | stream4/channel1 |
| F446 SAI2 A，AF10 | PD12 FS_A、PD13 SCK_A、PD11 SD_A | duplex 接 PE11 SD_B；PE0 MCLK_A | stream4/channel3 |
| F446 SAI2 B，AF10 | PE13 FS_B、PE12 SCK_B、PE11 SD_B | duplex 接 PD11 SD_A；PE14 MCLK_B | stream6/channel3 |

路线来自 DS10693 Table10／11、DS11118 的精确封装 AF 表，以及 RM0390 Table29／
RM0386 Table30。SAI2_B 的另一条硬件 DMA 请求未由当前路线选择，不能自行换 stream。
source block 由 route 推导；duplex 的 source 为 TX，另一 block 同步 RX。
每个 owner 独占所属 IP 的全局配置、A/B 两个 blocks 及唯一 source vector；SAI1
为 IRQ87，F446 SAI2 为 IRQ91。关闭 SAI2 不修改仍由 SAI1 持有的寄存器、gate、
IRQ 或 priority。跨 controller 配对及 DMA 冲突在取得硬件效果前拒绝。

master 只消费 System ClockPlan 的唯一 `pll_sai.sai1` Q 输出；这里的 `sai1` 是现有
单一 Q 输出字段，SAI2 不另设第二份 PLL 参数。F446 SAI2 的已选 mux 也由 System
配置、回读和撤回。F446 auxiliary M 独立，F479 与主 PLL 共用 M；Q／postQ 与
可选 LTDC R／postR 仍归同一 System owner。驱动不修改 PLL 或 RCC mux。
普通 master 使用 `mckdiv=0` 的除1或 `2*mckdiv`，frame bits 为二次幂；
`no_divider=true` 禁止 MCLK、要求 mckdiv0，rate 为 kernel/frame bits。
查询始终返回精确有理数，slave 为0/0 unknown。

所有模式保守要求 APB2 严格大于两倍 SCK。`max_bit_clock_hz` 是所选 master 的
电气 ceiling，或调用者保证的外部 slave 上界，不是实测值。实际选择 MCLK pad
时还要求共同已审 2.048–45MHz 范围；未选 MCLK 不附加该最低输出频率。引脚
使用数据手册表中的 OSPEEDR 二进制10，原恢复 snapshot 仍由 Platform 保持。

官方 `HAL_SAI_Init` 负责联合冷态格式，Handle 仅在栈上存在。已审官方 DMA
start／stop／abort 的私有等待和回调不直接建立 Nexus 完整归还证明，因此静态
选中的 C transport 独占 DMA 借用与退出。启用前和每个 source／sink 效果之间
重新观察绝对 deadline 与原 System epoch，TX FIFO prime 使用显式 service。
正常有限完成要求 source→sink 退出、FIFO／DMA／IRQ／地址全排空，才返回 TX 或
发布 RX，并 SETTLED；有限错误／取消不发布有效结果。

### SAI 连续 bank 交接

连续配置额外给出 `max_service_gap_us`，包含任务抢占和本次 service 自身的最坏
间隔。调用者提供 `nx_audio_blocks_request_t`、两个方向独立的 stream 和完整 frame
整数倍的 `block_bytes`；单方向仍最多65535个自然容器。TX 至少两个 READY blocks，
RX 至少三个 FREE slots。所有控制、slot 元数据及完整 capacity 在首次接纳前校验，
不临时分配缓冲，不补静音、不重放，也不创建后台 worker。

| 时点 | 硬件与借用责任 |
| --- | --- |
| 接纳 | 原 session、stream 和两个 DMA bank 保持借用；拒绝没有效果或引用残留 |
| IRQ | 只锁存 TC／error；FIFO error 掩蔽源，不能在 IRQ 里发布或替换借用 |
| service | 先取得 replacement，再更新非活动地址；重新观察 CT、硬件／锁存事件和时间预算 |
| 发布／归还 | 上述检查全部成立，旧 RX 才发布、旧 TX 才归还；两个方向独立前进 |
| 停止 | 先停 source 再 sink，撤回 DMA 请求并排空 DMA／IRQ，清空全部地址与 FIFO，再返回未完成 bank |
| 失败 | 拒撤或历史丢失保持 QUARANTINED，原 session、旧 bank、replacement 和资源均保留 |

准入用实际 wire frame bits 和有效样本数换算块时长，预留八字 FIFO、一个 serializer
样本及一个直接 DMA 传输的保守裕量，再要求剩余时长大于两倍 service 上界加两微秒。
源的真实 clock 不得超过声明值，应用还需按时补 TX、归还 RX。检查预算、CT／TC
只能建立这些前提下的软件交接，不能追溯保证违反预算之前的物理音频。

缺 replacement、bank 歧义或超预算会停止并报告 OVERFLOW；不能覆盖 READY／BORROWED
块。已经发布的 RX 前缀在停止后继续有效，需要消费者单独 release；未借用的 TX
READY 仍留在原队列。连续 `base.transferred` 选择 RX 时统计已发布 RX 字节，否则
统计已脱离 DMA 的 TX 字节；错误／取消仍可带有效前缀，不代表精确 wire frame 数。
普通拒撤可在原 epoch 重试，丢失历史不能用恢复寄存器外观解除。SETTLED 是对
session 和生产／消费权限的最后访问，不撤销先前独立的 RX consumer loans。

`tests/contracts/stm32f4_sai/` 保留原四个有限 reference，并新增 F446 SAI2、F479
稀疏 mask 的 master／slave duplex blocks。两个公共消费者分别覆盖布局和显式
caller-owned session。GoogleTest 验证真实 C 的交接、欠供给、替换拒写、CT 竞态、
epoch 与停止失败；ARM ELF／Source SDK 分别验证实际源码选择及外部消费。
PCB、codec、外部时钟、波形、FIFO 余量与持续负载仍须物理执行，当前 HIL 为
`not_executed`。

## 装配与验证入口

`tests/contracts/f407_capture_audio/` 是外部 software-only Board 夹具，覆盖 VE RAW12
配 I2S2 duplex、ZG embedded RAW blocks 配 I2S3、ZG RAW14、JPEG 以及 I2S3 MCLK。
`jpeg_blocks/ve-jpeg-blocks.toml` 复用 VE 软件路线，独立强类型消费者显式提供三个
4096 B bank 并调用 start／service／acquire／release／stop；不增加其他有限夹具的 RAM。
它们不修改公共 Board 的接线事实。`tools/configure/test_f407_capture_audio_configuration.py`
检查严格字段、typed IR、真实 claims、pure factory 及固定生命周期连接。
`tests/google/f407_capture_audio_test.cpp` 编译真实 C11 owner 和官方 DCMI cold Init，
只替换 CPU／MMIO 边界；失败 drain、迟到 IRQ、CT 歧义、欠替代 buffer、原始借用及
多实例共享时钟均在同一个完整程序中执行。模型结果、ARM ELF 和 HIL 分别验收；
本软件工作不执行 HIL。

## 旧 I2S2 专用角色迁移

`sky-i2s2-slave-tx.toml` 和 `qiming-i2s2-slave-tx.toml` 保留参考输入名称，改用
随 SDK 交付的 `f407-audio-reference/ve`、`zg` 软件 Board，不声明 Sky 或启明实板
音频接线。标准 `[audio.sound]` 与 factory／Platform 已覆盖原 Philips stereo16
引脚、DMA 和完整借用职责；旧 `[i2s2]`、组件、手动 owner 初始化及专用 driver 已删除。

旧程序中仍有效的回归转入同一个 capture/audio 程序：

| 原职责 | 保留的独立行为验证 |
| --- | --- |
| prefill、冻结时钟、WS 丢失 | 真实三微秒 tick、DR/NDTR/TXE、保持 idle 后才 ACTIVE |
| TC／NDTR 不能归还借用 | 新读 TXE 后再读 BSY，取消／deadline／停止失败保留 loan |
| DMA／IRQ／地址退出失败 | duplex 和 TX 的完整借用、停止重试及已取得的线上排空证明 |
| 几何、DMA 权限、计数上限 | 非空防溢出整块 rows、禁止 padding、131068B stereo16 上界 |
| 伪造及别名控制 | copied／stale／未对齐 loan、request 落在未用 payload capacity 尾部 |
| read-clear 与历史丢失 | 首次 SR 故障、reset／DMA gate／时基／epoch 丢失后永久 quarantine |
| SETTLED 最后访问 | provider 清除所有 payload 引用，控制块 PROT_NONE 后迟到 IRQ 和 close |
| 执行上下文及共享状态 | privilege／ISR／三个 incoming masks、共享 PLL、邻 DMA、IRQ 原优先级 |

原手动 face／storage ABI 的布局检查随该 ABI 退出。原 System 特殊 gate 分支的三项
测试只证明手动调用者已自行 join 的前提，随该架构删除；标准 Platform 现在实际调用
audio close，无法完成借用排空就拒绝资源撤回。cold 状态及共享 GPIO／DMA 门由已有
Platform 资源 owner 统一取得，factory 不再允许调用者构造第二个硬件 owner。
linked provider 验证沿用普通 generated face→共享只读方法表→真实私有函数的同一机制，
无需专用角色、公共 opaque storage 或另一个硬件发现入口。
