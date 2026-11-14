---
id: design.f407-fsmc
type: reference
audience: maintainer
source_language: zh_CN
---

# STM32F4 外部存储器

本页分别说明 F407 FSMC 和 F479 FMC／QuadSPI，三种控制器不因名称相近共享硬件
权限。F407 FSMC 实现限定 STM32F407VET6（LQFP100）与 STM32F407ZGT6（LQFP144）；
F479 软件参考的 SDRAM 数据窗口、显式图形存储模式与间接串行 NOR 见后续章节。
一个 `nx_stm32_fsmc_t` 管理唯一 FSMC gate、共享 AF12 pads 和所选 bank receipt。
SRAM、LCD、PSRAM、NOR、NAND 端点借用这个 owner；端点不会另建 RCC、GPIO 或
控制器注册表。原有 NE3 启动 SRAM 仍由复位计划的同一 const bank 描述符证明，
永久 boot owner 不允许 stop，也不向运行时发放覆盖 globals 的整 bank lease。

硬件依据是 RM0090 Rev22 第 36 章及 DS8626 Rev12 的 AF12 映射和 FSMC 时序限制。
官方 `stm32f4xx_ll_fsmc.c` 负责 NORSRAM/NAND 初始化、读写时序和 ECC enable/disable。
F407 没有 FMC 的连续 clock、write FIFO disable 等新增控制位。同步外部 CLK 不得超过
60 MHz；CRAM 页边界用 CPSIZE 明确选择 128/256/512/1024 字节或关闭。

## 精确路线与参数

| 端点 | 接口 | 选择与边界 |
| --- | --- | --- |
| 原 NE3 SRAM | `memory_window` | 原板级 16 位、1 MiB 描述符与启动证明不变 |
| 原 NE4 LCD | `parallel16` | 原 A12 命令/数据地址及有限同步调用不变 |
| PSRAM | `memory_window` 的 `psram-window` | NE1..4、8/16 位、复用/独立地址、字节线、burst、WAIT、EXTMOD 与周期显式选择 |
| NOR | `flash_async` 和同 ID 的同步只读 `Flash` | NE1..4、8/16 位、显式命令字节偏移、CFI 或显式命令集、完整 erase geometry |
| NAND | `nand` | NAND2/3、8/16 位、页/OOB 字节几何、列/行周期、common/attribute 时序与 ECC chunk |

`fsmc-reference-*` 路线描述芯片 pad inventory，端点参数选择实际需要的精确子集。
Board 必须显式授权该路线；配置不会替换一块未知 PCB，也不会自动探测安装器件。
VE 缺少 PF/PG，独立地址的旧 ZG SRAM 路线不能沿用；VE 可显式使用 NE1 的复用
地址/数据和 PB7/NL，或 PD7/NCE2 的 NAND2。NE1/NCE2、NE2/NCE3 共享 chip-select
pad，配置拒绝相冲突的两个 personality。NAND3 需要 ZG 的 PG9。

`tests/contracts/fsmc_assembly/` 是软件参考装配。它们没有指定真实存储器型号、PCB
或测得时序，也不代表实板资格。时序字段使用官方 LL 的编码周期约定；调用者依据
实际器件、电压、HCLK 和电气连接选择参数。器件自身的 PSRAM burst/configuration
寄存器须与所选传输协议一致，不能由一份通用 pad 描述猜测。开启 NWAIT 可能使
CPU 总线访问由外部电平延长；有限 service 步数不构成错误电平下的硬件退出保证。

## NOR 的两种合同

`nx_factory_flash_async()` 返回已有 NOR 端点；`nx_factory_flash_async_flash()` 使用同一个
`nx_flash_async_id_t` 返回同一状态的同步只读 Flash face。geometry/read 不保留目标；
同步 program/erase 返回 `UNSUPPORTED`，不会发起一条返回时仍可能忙的 pulse。
已有同步 Flash 的“返回证明 pulse idle”合同不变。

异步请求通过 `nx_flash_async_prepare/submit/cancel/service/stop` 使用有限 Core deadline。
SUCCESS submit 只表示 ACCEPTED。一个 service 最多提交一个 word/byte program 或
sector erase pulse；没有队列、worker、heap、HAL tick 或隐式重试。

命令集 1 是 Intel/Sharp，命令集 2 是 AMD/Fujitsu。unlock1、unlock2 与 CFI query 均为
显式字节偏移；8 位与 16 位器件不能共用猜测的 word 地址。CFI 选择会检查 QRY、
主命令集、容量以及每一个 erase region，并和完整声明 geometry 对照。关闭 CFI
表示调用者明确授权该命令集/几何，不能解释成探测成功。当前接口不提供 write-buffer
program、program/erase suspend 或芯片擦除等器件特定快捷命令。

官方 HAL NOR 使用进程级全局 word width，program/erase 与 status 分离，等待路径依赖
`HAL_GetTick`。这些行为不能同时保证多个不同宽度 bank 和 Core 借用排空。因此该
明确 SDK 缺口由 C11 命令状态维护，继续复用同一个官方 LL 配置的物理 FSMC。

取消与 timeout 禁止后续 pulse；正在忙的 pulse 保留 request、program input 和 bank lease。
DQ6/status ready 仍未证明 idle 时，状态保留为 DRAINING/QUARANTINED。错误或丢失 gate
不会释放借用。再次 service 只能观察和排空已有操作，不会重放命令。最早终止原因保持
不变；完成首次在 deadline 后观察到时报告 TIMEOUT。实际 pulse idle 后先 detach，再
release-publication SETTLED。全 `0xff` program unit 不提交 pulse；erase provenance、
掉电恢复和产品分区由调用者负责。

## NAND 原始操作与 ECC

NAND 不映射成无坏块的普通 Flash。一个请求执行一页主数据及可选完整 OOB、仅 OOB、
一个物理块擦除或显式复位。所有位置都是绝对物理页，包括坏块；库不扫描/跳过坏块，
不解释 OOB，不分配逻辑地址，不提供 FTL、文件系统或产品坏块表。

几何统一以字节表达；16 位总线只改变列地址和每次数据总线单位。512 字节的小页与大页使用各自的地址/确认序列。ID 读取不推断或替换声明几何；`nx_nand_status` 返回 ready、failed 和 write_protected 的类型化快照，调用者无需解码器件状态位。状态查询不归还活动请求，写保护和器件失败仍向请求返回明确错误。

`nx_nand_ecc_format` 在空闲已准备的同一 owner 上返回编码身份、step_bytes 和 code_bytes，不发出 NAND 命令。当前身份 `NX_NAND_ECC_STM32_FSMC_RAW32` 表示每步一个数值 `uint32_t`；4 字节是存储容器，不代表有效校验位数，也不是跨平台序列化格式或标准纠错算法。查询不推导纠错能力、OOB 放置或坏块策略，输出别名与忙状态在访问前拒绝。

ECC 是 FSMC 每 chunk 产生的原始代码：256..8192 字节的硬件 chunk 由配置选择，
可选输出数组每个主数据 chunk 对应一个 `uint32_t`。数据循环每次最多 32 个 bus unit，
chunk 边界等待实际 FEMPT 后读取 ECCR，再 disable 并回读 ECCEN；不调用带 tick 等待的
LL GetECC。部分 enable/disable 失败保留责任。代码的存放位置、校验纠错、OOB 格式和
可接受错误率完全由外部软件决定。

program data 尚在器件 latch 时取消会发出一次显式 reset，并继续等待 reset ready；
confirm 已发出后的 program/erase 不会被取消命令中断。页/OOB/ECC 借用直到设备与
ECC drain 都退出。错误读取可能已改写一部分目标，但只在完整成功时发布有效字节数；
错误的 transferred 为零。`ready_delay_us` 是实际器件忙状态建立时间的显式保守界，
避免在 tWB 等窗口内把旧 ready 状态误认作完成。

## 生命周期与验证边界

prepare 在首次写入前保存旧 bank control/read/write 寄存器，拒绝外来 NORSRAM、NAND、
PCCARD 或未拥有的 IRQ consumer。失败可保留 config/gate/bank receipt；调用者持续保存
全部来源并重试 close/stop。没有备份 reset、SRAM 清零、自测、隐式芯片复位或 DMA
权限授予。每个 lease 的原始地址、generation 和精确 storage 身份保持到显式归还。

顺序是：停止端点使用者，service 排空有限请求，close 端点 lease，stop shared bus，
最后由 Platform 恢复 GPIO 快照。仍忙的端点或 boot receipt 使 bus stop 失败，不会关
gate 或恢复 pads。FSMC 外部存储器仍不授权 STOP 中的 CPU/DMA 访问或可恢复执行域。

GoogleTest/Mock 执行真实 C owner、官方 LL 和命令状态，仅替换 MMIO 观察、CPU 边界与
外部存储器端口。完整程序覆盖取消、late deadline、busy、gate/readback 失败、8/16 位、
CFI region 和 ECC drain。ARM 消费者只证明精确 part 的选择、C11 编译、链接和资源预算；
物理 program/erase、WAIT 电气与 retention 仍是 `not_executed`。

## F479 SDRAM 的首个软件参考

`STM32F479NIH6` 的 FMC 增加独立 `sdram-controller` 模式，复用现有
`external_bus` 控制器和 `memory_window-endpoint`：两种 face 指向同一 C owner，
factory 只返回静态对象。本切片固定 Bank1、8 MiB、16 bit、12 行位／8 列位、
4 个内部 bank。默认 `cpu-window` 只授予 CPU 数据借用；额外的图形访问必须显式
选择下述 `graphics-owned` 模式。两种模式都不授予 XIP、堆、启动对象或自动内存探测。
具体器件时序必须由端点显式声明；软件参考数值不证明 RAM 实装或电气资格。

参数保留七组 `nanoseconds`／`cycles` 最小值、CAS、SDCLK 分频、读管线、首次刷新
次数、刷新窗口、上电等待和有限启停预算。配置工具拒绝几何不匹配、超过端点
`max_sdclk_hz`、不可编码的时序和非法刷新范围；C 在每次启动独立向上取整、满足
RC／WR 关系、计算 COUNT，并避开 RM0386 的禁止值。C 完成 CLK_ENABLE、实际完成后
计时等待、PALL、AUTOREFRESH、LOAD_MODE 和刷新设置；读回不把 MODE／CTB 的只写位
当作成功证据。System 的原始 `time_epoch` 必须持续有效，同频率／同 RCC 值不能
将旧 owner 接到新时间域。元数据预检在 System 之前只读执行，实际硬件行为全部在 C。

两种窗口模式都要求 SYSCFG 时钟与 `SWP_FMC=0` 的实际可观察状态，驱动不修改全局
交换或 MPU。原始稳定 lease、代次和派生指针使用者都退出后才能释放；停止以实际
Self-refresh／BUSY 状态证明写 FIFO 排空和 tRAS，随后恢复自己取得的资源。
失去 lease、时间域或读回证据时保留清理责任，不撤销已有 CPU 指针。同一持久 owner
可以恢复自己的 Self-refresh，并从实际 NORMAL 完成之后等待完整 tXSR／上电间隔。

ES0321 §2.3.4 要求保持共享 `FMC_BCR1.WFDIS=0`。FMC gate 关闭时不能可靠读取
该位：静态预检不假装完成物理证明，prepare 取得 gate 后在引脚／首条命令前检查，
后续命令与新借用再次检查。驱动不自动修改外来 WFDIS；已初始化后约束失效会阻止
新借用和普通停止并保留 owner／gate。只有尚未发命令／借用的 phase0 冷拒绝，
能够保持原 BCR1 不变并归还自己取得的 gate。实现见
[SDRAM 私有合同](../../soc/stm32/stm32f4/private/stm32f4_sdram.h)。

### 显式图形存储模式

将 SDRAM 端点的 `mode` 设为 `graphics-owned`，并在所选 `display`／`image`
控制器中用 `memory_window` 引用该端点 ID，才会建立图形连接。配置拒绝 CPU-only
端点、未知引用和没有图形使用者的 `graphics-owned` 窗口。完整软件参考见
[`stm32f4_graphics_sdram/combined.toml`](../../tests/contracts/stm32f4_graphics_sdram/combined.toml)。
RM0386 Rev6 §2.1.8–9 说明 LTDC／DMA2D 到 FMC 的总线路径，§12.8 规定 SDRAM
控制器行为；芯片总线可达性本身不构成 Nexus 的权限或生命周期授权。

生成器为该 bank 建立一个静态私有连接，将原始稳定 window lease 与两个精确
controller 身份关联。每个图形控制器在冷态 prepare 时附着，首个附着取得 lease，
最后一个成功冷态 close 才归还。生成的区域只向具名 LTDC 授予读取，向具名 DMA2D
授予读写；其他控制器和芯片全局 DMA 事实不变。图形请求继续检查完整容量、对齐、
格式、步长和别名，且核对原 System／时间代次、bank 状态和选定映射。

Platform 先启动 SDRAM，再准备图形控制器；冷停止按反序先退出图形硬件、IRQ 和
保留引用，再尝试停止 bank。请求达到 SETTLED 不代表控制器完成冷态 close。公共
`display.stop`／`image.stop` 只有在所选 provider 完成硬件／IRQ 冷退出并返回 SUCCESS
后才解除附着；BUSY 或错误继续保留 bank 责任。仍附着的控制器使 bank 停止及
Self-refresh 被拒绝；部分失败保留原连接和 lease，重试必须完成真实冷退出。

该模式保留 geometry 查询，但公共 CPU window acquire／release 不可用，不能借其
factory face 取得或提前归还私有图形 lease。这是配合式 provider 准入合同，不是
MPU 或 BusMatrix 防火墙，不能物理阻止任意原始 CPU 指针。参考装配没有实装 SDRAM
型号、PCB 或面板；带宽与物理验证边界见
[图形 provider 合同](official-stm32-provider.md#f479-ltdc-scanout-and-dma2d-memory-operations)。

## F479 间接 QuadSPI 与串行 NOR

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
[PIO 装配](../../tests/contracts/stm32f4_qspi/f479ni-qspi.toml) 与
[编程 DMA 装配](../../tests/contracts/stm32f4_qspi/f479ni-qspi-dma.toml)。

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

软件参考分别为 [Quad PIO 装配](../../tests/contracts/stm32f4_qspi/f479ni-qspi-quad.toml)
和 [Quad Read＋program DMA 装配](../../tests/contracts/stm32f4_qspi/f479ni-qspi-quad-dma.toml)。
它们不启用 quad program、QPI／DDR、dual Flash、映射或 XIP。

ES0321 §2.4.5 进一步要求
整个 provider 生命周期都禁止访问 `0x90000000` aperture，包括其它 CPU、IRQ、
DMA 和调试访问；仅不返回 mapped pointer 不足以满足约束。精确物理验收保持
`not_executed`，模型与 ARM 软件证据不替代电气／时序／实物验证。实现见
[QuadSPI 私有合同](../../soc/stm32/stm32f4/private/stm32f4_qspi.h)。

## F479 CPU_READ 映射端点

`mapped_storage/nor-mapped-cpu` 是独立端点，固定同一 `STM32F479NIH6`、`W25Q128JVSIQ` 和六个已审 pad。控制器仍选择 `qspi-indirect-sdr`，不与 program-DMA 组合；读取采用 6Bh、24 bit 地址和八个 dummy 时钟。该模式不导出 Flash／FlashAsync 旁路 face，也不授予 XIP、DMA reader、DDR、QPI、quad program 或写窗口。原间接 Flash 模式继续禁止所有 aperture 访问。

配置显式提供 `parameters.read_slots=1..8`。生成器仅为选中实例分配静态 composite、通用 owner 和确切数量的 reader slots，并通过 `nx_factory_mapped_storage_ref` 返回既有公共 face。未选模式不增加这些对象、组件头或链接依赖。CPU reader 只能访问 loan 授权的范围，必须先完成读取与 release；最后一个 release 证明映射退出后，有限间接请求或冷停止才可继续。失败保留 loan 和清理责任，不能提前重用 storage。

release 失败后，调用者已完成的 CPU 读取不得继续；保留 loan 只表示清理责任仍在，
不授予继续访问。仅原 System `time_epoch` 仍有效、受支持时钟条件恢复后才可重试
release；若 epoch 已改变而旧 loan 未 join，必须保持隔离并由产品恢复或复位，不能
回滚 epoch 使旧 token 复活。平台不承诺自动恢复。

ES0321 §2.4.3 要求先完成状态读取、将 AR 置零，并在前一次 indirect read 后证明 ABORT／BUSY／FIFO 完整退出；此后不再写 DR，才进入映射。§2.4.4 的 timeout counter 和 timeout IRQ 始终禁用，最后一个 CPU reader 退出后经同 CPU 屏障和有界 ABORT 撤回映射，并读回实际 FMODE。§2.4.5 的间接阶段 aperture 禁令保持。调用者必须排除其他 DMA、调试器及外来 master；reader 标签不构成总线防火墙。软件参考见 [映射装配](../../tests/contracts/stm32f4_qspi/f479ni-qspi-map.toml)，不证明实装器件、PCB 或 HIL。
