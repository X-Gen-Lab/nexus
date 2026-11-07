# XPT2046 raw touch

`Nexus::XPT2046` is an optional allocation-free C11 component for the exact
XPT2046 differential 12-bit protocol. It consumes one live synchronous SPI
endpoint, an active-low live pen predicate and an identified monotonic clock.
The selected endpoint must provide mode zero, MSB-first, complete 24-clock
transactions and actual settling/rate appropriate to the module. The Qiming
connector needs a software-SPI endpoint because PA5/PB1/PB2 do not form one
hardware SPI route. Board/assembly owns those facts and the PF11 pen connection.

The user identified the fitted module as ILI9341 + XPT2046. This component does
not bind or reset the LCD, claim another GPIO lease, configure an interrupt,
calibrate a display, rotate axes, filter samples or start a polling worker.
The caller serializes device, endpoint and pen access and joins observers before
releasing any of their storage. Select exactly one GPIO face with a one-bit
mask, or an EXTI face with mask zero. The EXTI provider must implement the
optional live electrical-level method; missing support rejects before the first
SPI write. This permits PF11 to have one EXTI owner instead of a conflicting
second GPIO owner. Buffered events are never consumed to invent a live level.

## Exact command and lifetime contract

Initialization completes one `0x90 00 00` conversion. Raw sampling checks the
actual pen level, then sends independent `0xD0 00 00` (X/channel101) and
`0x90 00 00` (Y/channel001) transactions. Optional pressure additionally sends
`0xB0` (Z1/channel011) and `0xC0` (Z2/channel100), each with two zero data bytes.
Every command selects differential 12-bit mode and PD1:PD0=00: reference off,
power-down between conversions and PENIRQ enabled when conversion finishes.
There is no undocumented alternate controller or ADS7846 fallback.

Each three-byte return contains one null bit, twelve conversion bits and three
trailing zeros; incomplete or malformed framing returns IO. The driver checks
pen again after the complete sequence and publishes a copied sample only when
both observations were low. Pen-up returns EMPTY without changing output.
Individual conversions are sequential; neither two low pen observations nor
the IRQ proves simultaneous conversion or a stable physical contact. Z values
are raw numbers, not calculated resistance. The output timestamp is software
completion in the supplied nonzero domain/epoch with unknown uncertainty.

One finite caller deadline covers the entire call. Each independent 24-clock
exchange, including initialization and stop's PD=00 command, also has a checked
local deadline at the earlier of the caller deadline and its start plus
1600 microseconds. Completion at or beyond that local boundary is rejected.
This conservative full-exchange envelope is stronger than the vendor's
page26 sample-and-hold warning for interrupted 16-clock conversions; it includes
command, acquisition and transfer overhead instead of inferring the internal
hold instant. It does not impose a generic maximum edge gap on other SPI users.
Separate X/Y/Z conversions each receive their own bound; they need not all fit
inside one 1600-microsecond interval. The selected endpoint receives the local
deadline, and a fresh observation after its return rejects late SUCCESS even
if the endpoint ignored it. The endpoint's actual error remains primary.
Clock regression/exhaustion and a nonfinite local-deadline sum reject.
Every transport return must end access to the local arrays, including failure;
only the SPI owner can retain its own pin recovery responsibility. A failed or
short transfer closes sampling and retains the same device binding as uncertain.
Recover the SPI owner if needed, then retry initialization on that same immutable
config. Stop sends another complete PD=00 conversion and unbinds only on success;
a separate cleanup deadline may be necessary. No timeout or GPIO-looking idle
state is treated as converter shutdown proof. A different config cannot replace
an uncertain binding. All callbacks, including initial clock reads, are guarded
against reentry.

## Sources and verification

The actual XPTEK document **XPT2046 Touch Screen Controller**, copyright2007,
30 pages, describes channel selection in Tables4/5, 24-clock transfers on
page21, control fields on page22 and PD/PENIRQ behavior on pages24/25:

- <https://files.waveshare.com/upload/9/98/XPT2046-EN.pdf>
- PDF SHA-256: `d01b61c1f08dc74963ac73d996c93ed0aed485ba33dcb09960adbbb629e9d515`

The document is a vendor-authored PDF obtained from a module supplier mirror;
it is not an assertion of a live manufacturer-hosted source or fitted silicon
identity. The component does not identify the chip or prove module presence.
Page26 explicitly requires conversion completion within 1.6 ms to avoid
sample-and-hold droop when byte transfers are interrupted. Page27 Table9 gives
minimum DCLK high/low times; the local full-frame bound does not turn those
minimums into a claimed measured clock rate or waveform guarantee.

GoogleTest/GoogleMock models verify the production command sequence, power-down,
12-bit decoding, pressure choice, pen-up/mid-sequence release, partial/framing
failure, unchanged error output, monotonic deadlines, callback reentry and
retained cleanup responsibility, and GPIO/EXTI source/alias rejection. Physical
acquisition settling, pen behavior,
SPI waveforms, coordinate accuracy and complete module HIL remain unexecuted.

## 中文合同

本组件只维护已确认 XPT2046 的差分 12 位协议，读取原始 X/Y 和可选 Z1/Z2，不处理
屏幕坐标校准、旋转、滤波、手势或任务调度。引脚、SPI 时序、PEN 电平和资源归 Board
与外部装配负责；本组件不重复取得 GPIO 租约。PEN 必须明确选择 GPIO 或具有实际
电平读取方法的 EXTI face，两者只能选一个。缺少 EXTI level 支持时在 SPI 操作前
拒绝，不消费边沿队列假装取得当前触点电平。

每次转换均为独立的 24 时钟区间，采用 PD=00，转换结束后进入掉电并启用 PENIRQ。
初始化和成功停止也必须完成该命令；失败保留原绑定和恢复责任，不能用超时或更换
端口假装已经停止。SPI 同步返回后不保留局部数组；总线 GPIO 撤回失败由总线 owner
继续持有，先恢复总线再恢复器件。

调用者有限 deadline 限制整次调用；每个独立 24 时钟交换另取“调用者 deadline 与
交换开始后 1600 微秒”的较早值，以检查过溢出的有限加法计算。初始化和停止的
PD=00 命令同样受限，到达或超过该边界即拒绝。此完整交换上限包含命令、采集和传输
开销，保守地强于原厂第 26 页针对可中断 16 时钟转换的 1.6 ms 采样保持泄漏限制，
不猜测内部保持的起点，也不修改其它 SPI 器件的边沿间隔合同。X／Y／Z 每次转换各自
取得上限，不要求整组同时落在 1600 微秒内。端点接收局部 deadline，返回后再独立
观察，因此忽略 deadline 而返回成功的过慢端点也不能发布结果；端点真实错误仍优先。
时钟回退、失效或局部 deadline 加法不能得到有限值均拒绝。所有失败保留原绑定和
uncertain 清理责任，不用超时冒充掉电成功。第 27 页 Table9 的时钟高／低最短时间
不是已测频率或波形保证；物理时序与模拟精度仍需 HIL。

采样前后均读取实际低有效 PEN 电平，中断只作提示。仅完整序列、正确帧格式且最终
PEN 仍低时发布结果；所有错误与松手均不修改输出。时间戳是软件观察，不代表同时
采样或精确物理触点时间。原始压力值不推导电阻。寄存器模型和软件测试不能替代本板
波形、采样建立时间、坐标准确度及实物验收。
