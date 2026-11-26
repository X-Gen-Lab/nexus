---
id: reference.contracts.io.f479-qspi
type: reference
audience: maintainer
source_language: zh_CN
---

(reference.contracts.io.f479-qspi)=
# F479 QuadSPI 与串行 NOR

本页限定于已维护的F479与W25Q128JV软件配置；并行存储、SDRAM与图形bank归[外部存储合同](f4-external-memory.md)。

## 间接访问与串行 NOR

首个软件参考固定 `STM32F479NIH6`、QUADSPI Flash1 和 `W25Q128JV`，控制器
`external_bus/qspi-indirect-sdr` 与既有 `flash_async-endpoint/nor-indirect`
保持独立身份、共享同一 C owner。此处仅支持 1-1-1 SDR、24 bit 地址、READ03、
256 B page program 和 4 KiB erase；16 MiB 物理几何以单个 erase span 表达。
`base_address=0` 只描述未映射的物理地址，不授予 CPU 映射。物理 Flash 的
`erase_spans` 完整覆盖容量，checked sector 查询不向下舍入、不保留展开表；失败不
修改查询输出。产品分区仍由外部工程明确声明，不能把 geometry 当作分区政策。
同步 Flash face 只提供有界读／几何／恢复，
写擦通过调用者持有的有限请求和显式 task `service` 推进，不新增驱动线程或队列。

`qspi-indirect-sdr` 使用 PIO；另一个显式控制器模式 `qspi-indirect-dma` 仅把有限
page program 的数据阶段交给 DMA。两者均保留上述只读同步 face、有限写擦请求、
1-1-1 命令和同一 controller owner；读取、状态和写后校验继续使用 PIO。
DMA 模式不新增异步读取合同、后台 worker、队列或缓冲区复制。

完整官方 HAL command／receive／transmit／IRQ／abort transport 在该 task 中推进，
QUADSPI 和所选 DMA 向量始终保持 NVIC 禁用。每个 Handle 的回调 ABI 固定为 1，厂商类型
保留在私有头；官方 HAL DMA 实现由所选 target 共享一次，不以测试 stub 代替。
Nexus 在交出缓冲区之前读回实际 CCR／AR／DLR，防止 HAL 从错误 DLR 扩展访问范围。
HAL 回调、CPU 数据退出、控制器 ABORT／FIFO／IRQ join、外部 Flash WIP 清零是不同
事实；当前单元读回确认之后才增加完成前缀。全 `0xff` 输入不发 program 脉冲。

DMA 模式按 RM0386 Table30 独占 DMA2 Stream7 Channel3，Platform 持有共享 DMA2
clock。生成器只连接可选状态和只读内存域，PIO 不分配 DMA 运行状态；CMake 只在
选择该模式时编译 DMA 扩展。编程请求的完整输入必须在准入前通过所选主 SRAM 或
只读内部 Flash 域检查，CCM、未授权外部窗口和覆盖 owner／配置的缓冲区均拒绝。
HAL 提交后分别读回稳定的地址、方向和模式；NDTR 允许真实递减，不能超出已借用
长度，EN=0 还须有完整传输计数与本轮 TC，不能将任意关闭当作完成。

task `service` 先推进官方 DMA handler，再推进 QSPI handler。DMA TC 只说明内存
传输阶段结束；QSPI BUSY／FIFO 退出、NOR WIP 清零和写后验证完成之后，才能发布
SETTLED。停止先撤回 QSPI 请求源，再观察 stream EN=0，清地址、计数、标志与 pending，
撤回 HAL 回调和缓冲指针；清理被拒绝时继续保留输入和请求。停止预算到期进入
QUARANTINED，后续显式重试实际 join，不重放 page program 或重置共享 DMA2。
两种模式的软件参考分别见
[PIO 装配](../../../../tests/integration/assemblies/stm32f4_qspi/f479ni-qspi.toml) 与
[编程 DMA 装配](../../../../tests/integration/assemblies/stm32f4_qspi/f479ni-qspi-dma.toml)。

取消或 deadline 阻止下一脉冲，不能撤销已发生的写擦；中途 ABORT 仍可能启动部分
page program。请求／输入持续借用，直到控制器和器件实际退出并发布 SETTLED。
清源或退出被拒绝时保留同一请求和冷态责任；重试必须完成真实 join，不能只清软件
active，也不能把 service 一次成功推进误报为 recover 已完成。首个错误保留，
已经确认的前缀单独报告。`flash_async.stop` 只封闭新请求并取消／排空当前请求，
保留 bank、配置、时钟与同步只读 face；失败退出保持 QUARANTINED 借用，不能冒报
已完成。只有 Platform 的冷停止才能归还控制器资源。任何停止都不能越过未结清
请求或重置共享 DMA／外来控制器。

端点显式提供 prescaler、NCS 高电平周期数和读／准备／停止预算；配置与 C 均审核
READ03 不超过 50 MHz、NCS 高电平至少 50 ns，并使用原 System 时间域。四个已审
SoC pad 不证明 PCB：外部集成须提供额定稳定电源与上电准备，WP#／HOLD# 或确切
后缀的 RESET# 处于数据手册允许的非活动状态。软件不自动修改保护、QE、QPI、
DDR 或复位器件，也不推断具体 Flash 封装／后缀已经实装。

## F479 选定 Quad Read

端点模式 `nor-indirect-quad-read` 显式选择 `W25Q128JVSIQ` 软件参考，保留同一
QUADSPI owner，可与上述 PIO 或有限 program-DMA 控制器模式组合。硬件路线增加
PF7／IO2 AF9 和 PF6／IO3 AF9，共六个 pad。DS11118 Rev8 Table10／12 证明精确
F479NI 封装的 bonding 和 AF，不证明 PCB 接线或实际 Flash 后缀。

Winbond W25Q128JV RevM（2024-12-24）§7.1.4 规定 IQ 选项 QE 固定为 1。
冷准备在单线 JEDEC probe 后只读 SR2，观察 QE=1、SUS=0 才发布 PREPARED；QE=0
或已有挂起操作均拒绝，保留实际冷清理责任，不修改 status、保护、供电或复位策略，
也不自动恢复外来挂起脉冲。JEDEC `EF4018`
不能单独证明 SIQ 订货号；实际器件声明和 IO2／IO3 连接由外部集成负责，不能继续
将这两个 pad 按旧 WP#／HOLD# 功能固定在非活动电平。

生成配置在编译期选择 `NEXUS_QSPI_QUAD_READ`，不增加运行时模式标志、最大状态
或另一层转发。同步只读 face 使用 6Bh、24-bit 单线地址、8 个 dummy clocks 和
四线数据；写后 verify、status 和 02h program 保持单线。数据仍由 PIO 处理，
默认只读 owner 的可变布局不增字段；增加的两份 GPIO 快照只属于所选六 pad 路线。
当前继续限制 SCK 不超过 50 MHz，每个底层读事务最多 32 字节，保留原有限 deadline
检查和实际 ABORT／FIFO／BUSY 收口。四线数据相位不等于端到端吞吐已经提升四倍，
命令、dummy、分段和 CPU 处理仍有代价，实际吞吐必须另行测量。

ES0321 §2.4.1 的额外 FIFO 条件同时要求 quad、DDR 和 AHB/2；本模式核对
DDRM=0 并继续执行完整 FIFO／ABORT 退出。§2.4.2 的写入 dummy 问题不因读命令
的 dummy 而适用：02h program 仍为 dummy=0，不增加 quad-program 命令。
6Bh 的 CCR／AR／DLR 必须在提供输出指针前全部读回一致；失败或超时的返回不得
继续访问调用者输出，未完成的硬件清理仍保留原 owner。

软件参考分别为 [Quad PIO 装配](../../../../tests/integration/assemblies/stm32f4_qspi/f479ni-qspi-quad.toml)
和 [Quad Read＋program DMA 装配](../../../../tests/integration/assemblies/stm32f4_qspi/f479ni-qspi-quad-dma.toml)。
它们不启用 quad program、QPI／DDR、dual Flash、映射或 XIP。

ES0321 §2.4.5 进一步要求
整个 provider 生命周期都禁止访问 `0x90000000` aperture，包括其它 CPU、IRQ、
DMA 和调试访问；仅不返回 mapped pointer 不足以满足约束。精确物理验收保持
`not_executed`，模型与 ARM 软件证据不替代电气／时序／实物验证。实现见
[QuadSPI 私有合同](../../../../soc/stm32/stm32f4/drivers/memory/stm32f4_qspi.h)。

## F479 CPU_READ 映射端点

`mapped_storage/nor-mapped-cpu` 是独立端点，固定同一 `STM32F479NIH6`、`W25Q128JVSIQ` 和六个已审 pad。控制器仍选择 `qspi-indirect-sdr`，不与 program-DMA 组合；读取采用 6Bh、24 bit 地址和八个 dummy 时钟。该模式不导出 Flash／FlashAsync 旁路 face，也不授予 XIP、DMA reader、DDR、QPI、quad program 或写窗口。原间接 Flash 模式继续禁止所有 aperture 访问。

配置显式提供 `parameters.read_slots=1..8`。生成器仅为选中实例分配静态 composite、通用 owner 和确切数量的 reader slots，并通过 `nx_factory_mapped_storage_ref` 返回既有公共 face。未选模式不增加这些对象、组件头或链接依赖。CPU reader 只能访问 loan 授权的范围，必须先完成读取与 release；最后一个 release 证明映射退出后，有限间接请求或冷停止才可继续。失败保留 loan 和清理责任，不能提前重用 storage。

release 失败后，调用者已完成的 CPU 读取不得继续；保留 loan 只表示清理责任仍在，
不授予继续访问。仅原 System `time_epoch` 仍有效、受支持时钟条件恢复后才可重试
release；若 epoch 已改变而旧 loan 未 join，必须保持隔离并由产品恢复或复位，不能
回滚 epoch 使旧 token 复活。平台不承诺自动恢复。

ES0321 §2.4.3 要求先完成状态读取、将 AR 置零，并在前一次 indirect read 后证明 ABORT／BUSY／FIFO 完整退出；此后不再写 DR，才进入映射。§2.4.4 的 timeout counter 和 timeout IRQ 始终禁用，最后一个 CPU reader 退出后经同 CPU 屏障和有界 ABORT 撤回映射，并读回实际 FMODE。§2.4.5 的间接阶段 aperture 禁令保持。调用者必须排除其他 DMA、调试器及外来 master；reader 标签不构成总线防火墙。软件参考见 [映射装配](../../../../tests/integration/assemblies/stm32f4_qspi/f479ni-qspi-map.toml)，不证明实装器件、PCB 或 HIL。
