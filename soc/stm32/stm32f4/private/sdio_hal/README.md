# F407 SDIO 私有传输与协议边界

SDIO 的硬件 owner 是 [`sdio.c`](../../drivers/sdio.c)，锁定的官方
`stm32f4xx_ll_sdmmc.c` 负责命令、数据路径、FIFO 和控制器时钟字段。DMA 通过官方
`LL_DMA_ConfigTransfer` 明确选择 flow：TX 使用 `LL_DMA_MODE_NORMAL`，RX 使用
`LL_DMA_MODE_PFCTRL`。Nexus 维护准入、所有权和实际回读，不另写 PFCTRL setter，
也不把它描述成官方 LL 缺失的能力。
本目录只为选中的 LL 对象提供最小 HAL 编译面，不调用全局 HAL 初始化或 HAL tick。

## 静态选择和请求生命期

SD memory/MMC 的 `sdio-poll` 使用普通 `block` face 同步访问。`sdio-irq` 和
`sdio-dma` 在同一个 controller ID 上提供可选 `block_async` face；普通 face 仍拥有显式 activate、
sync 和 stop。生成器只创建精确状态、配置和固定 IRQ 连接，不发现卡，不创建线程。
factory 查询不启动硬件。1 位和 4 位均保留同一六引脚 socket；只有显式 MMC
8 位模式使用完整十引脚路线，不虚构卡检测线路或开发板已有八线接线。

IRQ 每次最多处理八个 FIFO 字。DMA 固定使用 DMA2 stream3 channel4，缓冲区要求
16 字节对齐，并完整位于选中的 DMA 可达域。CCM 和 FSMC 的 CPU 窗口不能仅因 CPU
可访问而成为 DMA 存储。DMA TC、SDIO DATAEND、CRC 的 DBCKEND、主机 idle 和卡片
编程 ready 分别观察。SD memory/MMC 写入只有在卡编程结束后才确认整个 block。
已知内部 Flash、OTP 和 system ROM 不能作为接收目的地或可变请求；CPU-only CCM
仍可用于 CPU 路径，但不会因此获得 DMA 权限。

请求和整个 payload 从 ACCEPTED 持续借用到 acquire 观察 SETTLED。取消和 deadline
只开始排空。拒绝关闭 DMA、DPSM、IRQ 源或迟到活动 handler 时保留 QUARANTINED，
继续保留同一请求、缓冲区和冷态收条；后续 service 可重试清理。先结束所有硬件和
IRQ 引用，最后发布 SETTLED。排空没有隐藏 worker，外部执行者负责调用 service。

RX 使用 PFCTRL=1，DMA 在 EN 后把 NDTR 强制为 `0xffff`，不能以软件写入的请求
长度作为计数初值；稳定观察到的 words 为 `0xffff - NDTR`。TX 使用 DMA-controller
flow（PFCTRL=0），NDTR 为精确传输 word 数（memory 为 `N * 128`，CMD53 为 `unit_bytes / 4`），不靠 LAST 推断预取上界，也不要求
调用方提供未声明的 padding。软件 disable 同样可产生 TC，所以 TC 不是正常传输、
CRC 或卡片编程完成证明。RX 在 EN=0 后仍可将 FIFO 写入 RAM，须观察真实 TC、
FIFO_EMPTY，并完成 IRQ／地址 join 后才能归还借用。

TX 的 EN=0 与真实 TC 可以证明 RAM 访问结束，但 DMA FIFO 可能仍保留内部副本。
在其他引用也退出后可以结清 RAM 借用；未清副本仍由冷态 owner 保留，下一次 arm
或 close 返回 BUSY，不自动重放或丢弃副本。当前没有保证不影响其他 DMA2 stream
的局部 reset，因此普通 stop 不能宣称修好这种保留状态；上层须安排系统恢复。
DMA 仍只接纳 16 字节对齐和完整 burst，4／8／12 字节 CMD53 DMA 不开放。

## 介质与协议责任

SD memory 负责 CMD0/8、ACMD41、CID/CSD/RCA、选择、SCR、ACMD6 和 CMD16。
memory 单块使用 CMD17／24；2..max_blocks（上限 32）以一次 CMD18／25 传输，
DLEN 为整次 `N * 512`，packet 几何始终为 512 字节。多块结束显式 CMD12，再以
CMD13 观察 READY；写入还须确认卡编程完成。polling 与 async 共用该边界，不能把
每个 DMA／FIFO 完成当成整次介质成功。R3 没有有效 CRC 字段，其 CCRCFAIL 是正常
完成；具有 CRC 的响应严格检查。CRC、卡错误和超时不会自动重新发现或更换介质。

命令可能生效的收条只在无效果准入、deadline 和控制域等待通过后、首次可能写入
CPSM 之前建立。丢失响应保留协议 UNCERTAIN 与显式 repair 责任，不重放 payload。
RAM／IRQ 实际退出和介质恢复分别证明；CMD53 继续按 function unit 推进，不套用
memory 的 CMD12／13，也不把显式 sync 解释成更换卡或产品文件系统恢复。

`media_removed` 接收外部 Board 检测责任者已经建立的拔卡事实，不是检测或去抖策略。
拔卡立即撤销可查询几何，仍需排空主机才归还 payload；不等待不存在的卡返回 ready。
必须显式 stop 后再次 activate 才能接纳替换卡。超时本身不证明拔卡。

`protocol="mmc"` 显式选择 MMC，不从 SD 初始化失败自动回退。MMC 以 CMD1 协商电压和
sector 地址，主机分配 RCA；高密度从 EXT_CSD 的 SEC_COUNT 读取 512 字节扇区数，
低密度按 CSD 和 CMD16 使用字节地址。MMC 可显式选择 1／4／8 位，保持 legacy SDR，数据时钟不超过
12 MHz，不发送 SD 的 CMD55/ACMD41 或 ACMD6。4 位使用 MMC SWITCH 的 BUS_WIDTH=1；
8 位只接纳 CSD 的 SPEC_VERS=4（MMC 4.x／5.x 编码），以 CMD6 `0x03b70200` 写入
BUS_WIDTH=2。直接响应和随后 CMD13 都检查 MMC 的 SWITCH_ERROR bit7，且须观察
卡片 ready 后才写主机 WIDBUS8。切换失败不隐式降为四位，也不以主机字段写入替代
卡片确认。不启用 HS_TIMING、高速、DDR、HS200、boot partition、RPMB 或产品分区策略。

八线 MMC 的 pad 顺序为 PC8／9／10／11（D0–3）、PC12（CK）、PD2（CMD），再接
PB8／9（D4–5）、PC6／7（D6–7），全为 AF12。Board 必须明确授权完整路线。
各实例只生成六或十份 pad descriptor／快照，不扩大全局最大数组；冷态 pin 收条
覆盖十位，取消、排空和恢复失败仍保留精确未归还的 pin／clock 责任。新模式不会
使现有六线 SD socket 自动成为八线 MMC。软件参考见 `tests/contracts/mmc8_assembly/`。

## SDIO I/O functions

`protocol="sdio"` 在相同物理 controller ID 上导出独立只读 `sdio_io` face，普通
`block` face 继续负责 activate/stop；它不发布扇区几何或提供扇区读写。冷态准备和
factory 不发现 function。CMD5/R4 负责电压、ready、function count 和 memory-present
事实；R4 与 R3 一样没有有效 CRC 字段，CMD52/53 的 R5 必须严格检查 CRC、function
number、out-of-range 和 error 位。Combo 卡只描述 memory-present，不隐含同时维护
其 memory portion。

host 通过 CMD52 维护 CCCR 的 I/O reset、IOEx/IORx、IENx、bus width，以及各 FBR
block size 的写入与读回。直接 function-register 写入仅允许非零 function，避免
设备驱动绕过 host 修改这些共同字段。读取允许 function0，用于 CCCR/FBR/CIS；
inspect 返回标准 CIS 指针和接口码，CIS tuple 解释、设备寄存器语义、固件下载和
中断确认策略由外部 function driver 持有。七个 FBR 缓存槽是硬件 register facts，
不创建设备对象、运行期名称 registry、队列或对象池。

CMD53 字节模式精确支持 1..512 字节，512 使用标准零 count 编码。block 模式使用
显式配置的 1..2048 字节 power-of-two FBR 单位，逐单位发送 count=1 的有界请求；
不四舍五入地址、长度或部分结果。increment 与 fixed-address FIFO 语义分开。
PIO/IRQ 的最后一个 FIFO 字只复制真实尾字节。DMA 不接纳非完整四字 burst 的单位，
不分配 bounce buffer。请求复用 `nx_request` 和原有限引擎，整个外层 function request
与 payload 持续借用到 SETTLED。polling 通过显式 service 每次最多推进八个字，不设置
IRQ MASK，也不消费未取得的 NVIC pending；IRQ/DMA 使用同一固定 host/vector owner。

DATAEND、DBCKEND、DCOUNT、主机 idle 和 DMA drain 共同建立传输确认；function 写入
不发送 memory-only CMD13，也不把 CRC 通过当作设备内部应用处理完成。取消/错误后
host 先排空并保留 UNCERTAIN，显式 sync 使用 CMD52 function-abort、实际 IOEx/IORx
和 FBR 观察恢复；不回放 payload。FBR 两字节修改不是原子操作，失败使缓存失效，
调用者须显式观察和修复。

SDIOIT 只锁存 hint 并屏蔽 level source。调用者读取 CCCR pending，由 function driver
确认设备寄存器，再显式 rearm；成功数据完成保留尚未消费的 arm，错误、取消和 sync
均要求显式重新 arm。没有 ISR 用户回调或隐藏 worker。stop 先 abort 未知
传输、关闭 IENx/IOEx 并确认全部 IORx 清零。即使 I/O reset 在 RCA 分配之前失败，也
不能假定先前启用的 functions 已停止，清理失败会保留同一 host 收条。外部拔卡事实
可跳过不存在卡片的响应，但仍必须完成主机/IRQ/DMA 排空。

SDIO I/O functions 的当前范围为 3.3 V 单卡、1／4 位 normal/low-speed operation；
low-speed 卡保持严格低于 400 kHz，四位模式还检查 low-speed four-bit capability。该协议
不提供八位模式，未选 UHS/1.8 V、DDR、suspend/resume 或 ReadWait。参考配置见
[`sdio-functions.toml`](../../../../../boards/stm32f407zg_qiming_v41/assemblies/sdio-functions.toml)。

## 静态时钟与控制间隔

静态 ClockPlan 必须产生精确 48 MHz PLL48，并同时满足 RM0090 §31.2 的
`PCLK2 >= 3/8 * SDIO_CK` 与 ES0182 Rev19 §2.4.6 的按总线宽度计算的严格 FIFO
不等式。配置选择最小合法偶数 CLKDIV：正常数据最多 12 MHz，识别与 low-speed
严格低于 400 kHz；没有 8-bit 寄存器内合法解则拒绝，不饱和、不静默改协议。
默认 84 MHz PCLK2 可用 12 MHz；2 MHz PCLK2 时，1／4 位数据为 4.8 MHz，8 位为
2.4 MHz。每次实际 CLKCR 变更也核对同一约束，并要求 host idle、无借用和 DMA EN=0。
POWER 后 7 HCLK、时钟变更后的 3 SDIOCLK + 2 PCLK2 间隔由生成常量和微秒量化余量
计算，不通过固定 `hse8-pll168` 字符串或空循环估计。

## 验证边界

主机测试执行真实 C owner 和官方 LL，仅替换寄存器、FIFO 和 CPU 边界。ARM ELF
负责实际源码、资源和链接保留；两者都不证明插卡、电气、时序、吞吐或硬件资格。
物理 HIL 保持 `not_executed`。启动 SRAM 的永久 owner 和其 DMA 授权边界见
[FSMC 合同](../fsmc_hal/README.md)。
