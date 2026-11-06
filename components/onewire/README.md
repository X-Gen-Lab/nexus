# Bounded GPIO one-wire

`Nexus::OneWire` provides one static, caller-owned standard-speed bus. Board and
assembly supply one already configured open-drain GPIO output, its external
pull-up, actual input readback and a live monotonic clock. Writing one releases
the wire. The component neither changes GPIO modes nor creates a task, queue,
allocator, device registry or global interrupt mask.

`nx_onewire_exchange` performs reset/presence, transmits 1–16 LSB-first bytes and
receives 0–128 bits. It always starts a fresh protocol transaction. Zero unused
bits complete the final receive byte. CPU buffer access ends on every return;
partial receive bytes may change on failure. Caller spans are disjoint from
known owner/configuration/face/operations storage. Complete opaque GPIO/clock
context extents remain the assembly's responsibility.

Every wait checks a finite absolute deadline and an explicit finite poll count.
The falling edge is bracketed: minimum pulse duration is measured from its
post-write observation, maximum from its pre-write observation. Read sampling
finishes inside a strict 14-us observed bound. Slot/recovery minima also include
post-edge observations. Delayed GPIO/clock callbacks and preemption cause errors
rather than a claim of successful timing. The fixed 500-us reset-low target and
960-us software ceiling are a conservative reference profile; 960 us is not the
external-VDD data sheet's maximum. Physical pull-up RC, clock uncertainty,
waveforms and WCET still require hardware measurement. No physical timing or
parasite-power capability is inferred from host mocks.

One serialized task owns calls and configuration. Never change the clock domain,
pins or pull-up while the owner remains open. Synchronous failures attempt one
immediate release/readback even after the transfer deadline. The primary error
and cleanup result are separate. Failed withdrawal retains the wire binding,
fault and GPIO lifetime, while retaining no CPU payload pointer. Use a fresh
finite budget with `nx_onewire_stop` before dropping outer GPIO/configuration
storage. Faulted stop holds the existing open-drain/clock binding for 500 us
from the first observed physical high and post-read clock, then verifies high
and deadline again: an aborted reset can trigger a later presence pulse. A short
budget or insufficient poll count retains the binding; clean stop adds no such
wait. A backward or nonfinite clock permanently faults this epoch; stop may
attempt immediate electrical release but cannot claim clean closure in that
invalid epoch. External reset or a separately reviewed handoff is then required.
No later numerical deadline silently repairs the epoch.

The DS18B20 component composes this transport. It does not use a stale read-bit
conversion context across unrelated transactions. Overdrive, ROM search,
multimaster operation, strong pull-up and parasite supply are absent. Source
review identities are in `references.json`; physical HIL is `not_executed`.

## 中文合同

本组件是调用者持有的标准速率 1-Wire 总线。Board 和装配明确提供开漏输出、外部上拉、
实际输入回读以及有效的单调时钟；写一表示释放线路。组件不配置引脚，不分配内存、
创建线程或队列，也不长时间屏蔽中断。每次事务都先复位并检测存在脉冲，再按低位优先
发送 1～16 字节、接收 0～128 位。

每个等待同时受绝对有限 deadline 和轮询次数约束。下降沿前后分别观察时钟，用较晚
时间保证最短低电平，用较早时间限制最长低电平；读取在观察到的 14 微秒窗口内完成。
时隙与恢复时间也从边沿后的观察推导。GPIO 回调、时钟读取或抢占超时均返回错误。
500 微秒复位目标、960 微秒软件上限是保守参考配置；960 微秒并非外部供电模式的
数据手册上限。模型不证明实物波形、上拉 RC、时钟误差或最坏执行时间。

所有同步返回都结束对 CPU 缓冲区的访问，失败可能留下部分接收数据。线路释放失败
继续保留总线、配置和 GPIO 的责任。停止需要新的有限预算，并成功确认线路释放后
才可释放外层资源。时钟回退或返回非有限值会永久终止原时钟域；即使尝试释放线路，
也不能用新的数值 deadline 假装修复，必须外部复位或经过单独审查的所有权交接。
故障后的停止会从首次物理高电平及其后时钟观察起继续保留开漏和时钟绑定 500 微秒，
再检查高电平和 deadline，避免复位失败后迟到的存在脉冲。预算或轮询次数不足时
继续保留绑定；正常停止不增加此等待。

本组件不提供寄生供电、强上拉、超速、ROM 搜索或多主机。实板状态仍为未执行；
DS18B20 在此机制之上实现明确器件协议。
