# Optional epoch-safe AMP shared-memory lane

`Nexus::AMPIPC` supplies a bounded unidirectional lane. The public
`nexus/components/amp_ipc_storage.h` header defines caller-owned endpoint,
configuration and local-slot storage; `amp_ipc.h` defines operations and tickets.
One sender and one receiver own separate offer/ACK cache lines. Each endpoint has
one serialized privileged Thread executor. Two lanes provide bidirectional
communication. There are no allocated objects, registry, interrupts or workers.

The integration supplies the exact shared payload window, slot count, nonzero
boot epochs, coherent atomic control-word operations and cache-maintenance port.
Cache granules are explicit powers of two from 1 through 64 bytes. Each granule
divides the fixed 64-byte wire isolation unit. All shared objects and payload
spans have complete aligned cache lines; sender payload loans cannot overlap.
The component validates address overflow, typed alignment and storage/output
aliasing before admission. It sends shared offsets and lengths, never pointers.

The unique-role claim callback grants the hardware owner and atomically rejects
every reused or decreasing local boot epoch, including after a normal join.
The authoritative boot owner retains that history across resets and fails closed
at exhaustion. The endpoint never reconstructs live or joined storage. Per-slot
32-bit sequences increase without rollover; the final sequence retires its slot.
Every copied ticket/control requires its original endpoint storage to remain alive.

A successful send retains its payload until poll acquire-observes an ACK with
the exact sender epoch, receiver epoch and slot sequence. Cache failures and peer
epoch changes retain outstanding loans. A mismatch permanently closes that
endpoint; restoring stale boot words cannot reopen it. Old-boot offers are inert
and cannot block fresh offers in later slots. Successful receiver admission
requires offer then payload invalidation. The caller joins all readers before
finish; a valid finish consumes the caller view even if ACK clean/publication
cannot complete. Failed ACKs retain endpoint storage and may be retried.

Close blocks new sends/receives. Ordinary join waits for exact sender ACKs and
receiver finishes. Reset join additionally requires the port to prove that the
exact old peer CPU, DMA, IRQ and observers can never access that physical lane
and payload again. Receiver views must already have been consumed by finish.
An observed new epoch, cancellation or timeout does not supply that proof.
Only successful join relinquishes the unique owner. Endpoint/control storage
remains alive until all copied ticket/control observers have joined.

This is common software mechanism, not a selected AMP hardware integration.
An exact multicore target, shared-region reachability/attributes, resource
partition owner, control-word atomicity, cache ordering and trustworthy reset
proof remain unselected. STM32H743 is single-core and is not an AMP target.
Host GoogleTest/GoogleMock execution establishes this algorithm's software
behavior; no physical cache/reset/IPC HIL has executed. IO-40's original unique
hardware-owner and remote-reset/cache exits remain open.

## 可选的 epoch 安全 AMP 共享内存通道

`Nexus::AMPIPC` 提供有界单向通道，公开 storage 头文件定义调用者提供的端点、
配置和本地 slot。发送方与接收方独占不同的 offer／ACK cache line；每端只有一个
串行特权 Thread executor。双向通信显式使用两条通道，无堆、注册表或隐藏任务。
配置明确指定共享 payload、容量、非零 boot epoch、cache granule 与原子发布端口。
granule 是 1 到 64 的二次幂，必须整除固定 64 字节隔离单元。软件检查地址溢出、
自然对齐和配置／输出重叠；wire 只传 offset／length，不传跨核指针。

独占 claim 端口同时拒绝重复或递减的本地 boot epoch，普通 join 后也不能重用。
硬件 boot owner 跨复位保留历史，耗尽时关闭准入。slot sequence 不回绕，最终身份
永久退休。发送借用只有精确 epoch／sequence 的 ACK 被 acquire 观察，或旧 peer
停止证明通过后才结束。cache 失败、epoch 变化和超时都不撤销借用；旧 boot offer
不会阻塞后面的新 offer。接收前依次 invalidate descriptor 和 payload。调用者先
join 本地 reader，再 finish；有效 finish 即消费本地 view，即使 ACK 失败仍须保留
端点／slot 存储并重试或恢复。复制的 ticket／control 的观察者全部 join 前，原端点
存储始终存活。

close 关闭准入；普通 join 等待精确 ACK／finish。reset join 必须证明指定旧 peer
的 CPU、DMA、IRQ 和 observer 永远不再访问该物理通道及 payload，且接收 view 已
finish。仅观察新 epoch、取消或超时都不是停止证明。当前未选择确切多核芯片、共享
内存属性、唯一硬件资源 owner、cache／原子性及可信复位端口。STM32H743 为单核，
不作为 AMP 目标。主机测试只证明软件算法，物理 HIL 未执行，IO-40 原始退出条件
仍未完成。
