---
id: design.f407-fsmc
type: reference
audience: maintainer
source_language: zh_CN
---

# F407 FSMC 外部存储器

当前软件实现限定 STM32F407VET6（LQFP100）与 STM32F407ZGT6（LQFP144）。
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

`nx_factory_flash_async()` 返回已有 NOR 端点；`nx_factory_fsmc_nor_flash()` 使用同一个
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

几何统一以字节表达；16 位总线只改变列地址和每次数据总线单位。512 字节的小页与
大页使用各自的地址/确认序列。ID 与 status 是独立的同步 raw primitive，不能据此
静默替换声明几何。写保护和状态 fail 位向请求返回明确错误。

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
