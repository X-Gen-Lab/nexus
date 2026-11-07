# Bounded GPIO SPI

`Nexus::SoftwareSPI` is an optional C11, caller-owned synchronous SPI mode-zero
endpoint. It uses the public GPIO and clock faces. One selected configuration
owns SCK, MOSI, MISO and one active-low CS; those pins need not form a hardware
SPI alternate-function route. The component adds no worker, interrupt handler,
allocator, runtime registry or OS lock. Instances share immutable method tables.

The Qiming touch connector is one use: PA5 clock, PB1 MOSI, PB2 MISO and PB0 CS.
These are Board facts, not pin constants inside this component. The caller first
establishes the reviewed GPIO modes and clock, keeps their faces/configuration
alive, and stops all users before returning the GPIO leases. Software SPI never
changes a pin mode or starts/stops a shared clock.

## Execution and ownership

The selected task is the only executor. Transfers are MSB-first, 8-bit, CPOL=0,
CPHA=0, with one complete CS interval and 1..`max_bytes` bytes, at most 256.
Every transfer requires a finite absolute deadline in the configured clock
domain. Minimum half-period, CS setup and CS hold are explicit microseconds.
Each wait also has a finite clock-read limit: failure to observe enough elapsed
time returns IO instead of spinning indefinitely. Interrupts can lengthen any
phase. These values do not establish physical bus frequency, maximum pulse
length, CPU budget or device acquisition accuracy; validate those properties for
the actual task/interrupt load and wiring.

NULL TX sends 0xff; NULL RX discards input. At least one buffer must exist.
Exact in-place TX/RX is safe because one complete TX byte is copied before RX
publication. Partial overlap and known aliases with state, config, faces or the
count output reject. Opaque GPIO/clock context extents remain caller duties.
Only complete bytes update RX/count; an incomplete byte is not published.

A synchronous return ends every payload access. SUCCESS additionally requires
all bits, configured delays and inactive CS/SCK/MOSI withdrawal to complete.
Failure after select was attempted latches a fault even if idle cleanup succeeds.
No further transfer is allowed until explicit port recovery or stop succeeds.
A failed CS/clock GPIO write retains the same pin owner and binding; it does not
claim physical withdrawal, retain TX/RX pointers or replay a byte. Recovery only
retries the three idle outputs. Stop unbinds only after all three succeed; it
does not restore Board pin modes. Callback reentry is BUSY, including during the
first clock callback. Async submit, wake binding and DMA are unsupported.

## Verification boundary

GoogleTest wire models execute the actual production C source and inspect bit
order, rising-edge samples, CS intervals, minimum clock ticks, finite waiting,
known aliases, callback reentry, partial bytes, failed assertion/withdrawal,
recovery and instance isolation. Those observations do not qualify physical GPIO
waveforms, interrupt latency or the Qiming connector.

## 中文合同

这是可选、静态、单任务持有的软件 SPI mode 0 端点。四个 GPIO 不要求属于同一硬件
SPI 复用组；引脚模式、上拉、资源和时钟仍由外部 Board/装配负责。初始化不接管共享
GPIO 或时钟，不启动任务和中断，不分配内存。

一次调用覆盖完整 CS 区间，最多 256 字节，绝对 deadline 有限；半周期、CS 建立与
保持时间由调用者明确声明。每次等待还有有限轮询次数，时钟不前进时失败。中断可延长
波形，不能把配置的最短时间宣称为实测频率、最大延迟或器件采样精度。

所有同步返回都结束对 TX/RX 的访问。只有完整传输并成功撤回 CS/时钟等输出才返回
SUCCESS。选中尝试后的错误会锁存故障；无法撤回 GPIO 时继续保留相同引脚责任，必须
显式 recover/stop 成功后才可复用，不保留 payload 指针或自动重放字节。停止不归还
外部 GPIO 模式租约。回调重入返回 BUSY。IRQ/DMA 异步模式不在本实现范围内。
