# ILI9341 settled 16-bit parallel controller

This optional C11 component owns the ILI9341 command protocol, not the Board,
parallel controller, reset rail, backlight, framebuffer, task or scheduler.
`Nexus::ILI9341` is explicit and unselected code adds no firmware residency.
All state and pixel storage belong to the caller. Immutable configuration holds
a stable public `nx_parallel16_endpoint_t` and clock; instances share readonly
operations tables and the component imports no SoC or vendor type. Factory
lookup returns the existing face without acquiring a bank. The settled port completes
every memory access and releases every span before returning, including errors.
It must never be backed by an asynchronous submit that retains a stack span.

The selected controller profile is native 240 by 320 RGB565; MADCTL MV exchanges
logical dimensions. Only MY/MX/MV/BGR are admitted. Rectangle writes require an
exact packed halfword count and in-bounds coordinates; no silent clipping, copy,
conversion, framebuffer or heap is created. The returned completed pixel prefix
describes bus writes, not visible pixels or tear-free presentation.

Initialization checks ID4, then issues software reset, sleep-out, pixel format,
orientation and display-on. It does not drive shared NRST or guess panel-specific
power/gamma settings. Supply and external reset stabilization precede use. An
unknown/warm controller can already be in Sleep Out: conservatively wait 120 ms
after SWRESET before SLPOUT, then 120 ms after SLPOUT. `service` advances one
transition and never sleeps or spins. The finite initialization deadline covers
both intervals and all transport accesses. A failed reset/sleep-out attempt
still establishes a quiet interval; stop/recovery cannot issue early commands.

Before bus access, bad inputs leave the output untouched. Once a command is
attempted, transport, count or time failure retains the same binding as
UNCERTAIN. Explicit stop sends display-off after quiet time; success releases
only this component's binding. The caller then joins/releases the bank endpoint
and stops the shared bus. Timeout never authorizes another owner to reclaim its
transport. Backlight and power policy remain external.

The private F407 FSMC owner supports a post-start NE3 one-MiB SRAM window and
NE4 command/data port. It uses one AHB3 gate receipt and the enclosing Platform's
single GPIO snapshot lease. Exact selected pad unions are 20 for LCD, 40 for
SRAM and 41 for both. Banks retain independent declared read/write timings.
The separately selected reset-only SRAM plan can initialize explicit external
DATA/BSS before the C runtime. Adoption verifies the original Flash descriptor
and actual register profile; boot-owned pads have no ordinary restore snapshot.
A statically initialized selected boot receipt protects globals before Platform
startup and survives rejected runtime adoption. Boot backing is permanent until
reset: even with no endpoint loan, bus stop
returns BUSY before touching banks/gates. A generic whole-bank runtime loan is
rejected in that mode because it would overlap placed globals. Plain post-start
SRAM CPU accesses have no virtual call per load. CPU accessibility does not prove
DMA reachability or physical fitting.

The 16-bit LCD route uses NE4 and external A12: command is `0x6c000000`, data is
`0x6c002000`, because the external address is shifted relative to byte addresses.
The bounded PIO span checks its finite monotonic deadline every 32 words and
after its final word; deadline observation can be delayed by one such quantum.
No DMA, continuous image stream, optical timing or physical support is inferred.

Protocol reference: ILITEK *ILI9341*, commands 01h, 11h, 28h, 29h, 2Ah/2Bh/2Ch,
36h, 3Ah and D3h; software-reset and sleep-out recovery restrictions. The source
uses documented controller facts; the reference PDF is not redistributed.

## 中文合同

只读配置保存稳定的公共 `nx_parallel16_endpoint_t` 和时钟，实例共享只读操作表；
组件不依赖 SoC 或厂商类型，factory 查询不取得 bank。

此可选 C11 组件只维护 ILI9341 协议，不拥有板卡、并行控制器、复位电源、背光、
帧缓冲、任务或调度器。状态和像素均由调用者提供；同步端口在每次返回前结束访问，
所有错误返回也不保留传入描述符或缓冲。不得把异步提交伪装成此同步端口。

维护原生 240×320 RGB565，MADCTL 的 MV 交换逻辑宽高，只接受 MY/MX/MV/BGR。
矩形必须完全在界内且为精确的连续半字数量，不隐式裁剪、转换或分配帧缓冲。
返回像素前缀仅说明总线写入，不证明可见结果或无撕裂。

初始化读取 ID4，随后发送软件复位、退出休眠、像素格式、方向和显示开启；不驱动
共享 NRST，也不猜测模块的电源或 gamma 参数。调用前确保供电及外部复位稳定。
控制器初态可能已退出休眠，因此 SWRESET 后保守等待 120 ms 才发送 SLPOUT，
再等待 120 ms。service 每次推进一个阶段，不睡眠或空转。有限绝对 deadline
覆盖全部阶段；失败的复位／退出休眠尝试也保留安静时间，stop／恢复不能提前发命令。

访问前的输入拒绝不更改输出；尝试发命令后的传输、数量或时间错误保留相同绑定为
UNCERTAIN。显式 stop 在安静时间后发送关闭显示，成功只释放组件绑定。调用者之后
会合并释放 bank 端点，再停止共享总线。超时不撤销其它 owner 的借用。

私有 F407 FSMC owner 维护运行期 NE3 一 MiB SRAM 窗口和 NE4 显示端口，单独持有
一份 AHB3 门控收条，GPIO 原值只由外层 Platform 保存一次。LCD／SRAM／两者的
实际引脚并集分别为 20／40／41，各 bank 独立声明读写时序。另行显式选入的复位
方案可在 C runtime 之前初始化外部 DATA/BSS；运行期接管核对原始 Flash 描述符和
实际寄存器，复位持有引脚不保存普通恢复快照。启动 bus 的静态收条早于 Platform 启动，运行期接管拒绝也不能清除责任。
启动内存一直保留到复位，即便没有
端点借用，bus stop 也会在修改 bank／门控前返回 BUSY。该模式拒绝整块 SRAM
运行期借用，避免覆盖已放置全局对象。普通运行期 CPU 窗口不增加每次访问的虚调用；
CPU 可访仍不代表 DMA 或实物已验证。

16 位总线下外部 A12 对应 CPU 字节地址 bit13，因此命令／数据地址分别为
0x6c000000／0x6c002000。PIO 每 32 个半字和最终半字后观察时钟，deadline 的
观察最多滞后这一有界批次。未建立 DMA、连续图像、光学时序或实物资格。
