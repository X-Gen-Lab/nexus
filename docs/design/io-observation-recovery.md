# I/O observations, capacity and retained recovery

本轮 IO07、IO09、IO10 使用显式、调用方持有的诊断对象。默认 request、stream、slot
和 owner 的字段不变，平台不建立全局采样器、隐藏线程或自动恢复设备的策略。诊断
观察实际状态；deadline、错误或诊断对象的 detach 都不撤销已经接受的借用。

## Request 的稳定身份观察

Core 的 `request_observer.h` 仅依赖 Core。调用方将一个清零的
`nx_request_observer_t` 绑定到稳定 slot 和非零 epoch。sample 先核对仍然存在的
同一 incarnation，再访问 request；不会缓存 request 指针。slot 被释放或复用后，
旧 observer 返回 STATE。epoch 不循环复用，达到上限后由既有 slot 合同拒绝继续接入。

调用方排除 sample 与 slot bind/release、request prepare/reuse 以及 observer 本身
的并发修改，并保证绑定存储始终有效。Core 不替调用方增加 IRQ mask、OS 锁或多核
同步。普通设备的执行者仍可推进 request；只有 acquire 观察到 SETTLED 后才读取最终
result/count。借用期间输出 result=BUSY、count=0，避免把尚未发布的结果当成有效值。

时间戳与 request deadline 处于同一单调时钟域。逆跳返回 IO、保留输出与最后有效时间，
诊断计数饱和。max_gap 和 transitions 只描述两次采样之间的观察差异，不是硬件完成
时间、所有内部状态转换或 WCET。detach 只解除观察绑定，允许在 QUARANTINED 时执行，
但随后仍必须排空实际设备借用。

输出必须与完整 descriptor、payload 和控制存储分离。公共 Core 仅知道 request 基类
大小，不能验证派生 descriptor 的全部区域，也不能探测已经释放的 C 指针。拥有者通常
会在 outer SETTLED 前释放自己的控制 slot；此后观察该 slot 得到 STATE。若需要在
最终结果发布后继续采样，调用方应持有自己的独立稳定 slot，并在自身读者退出后释放。

## Owner 的显式进度诊断

可选 `Nexus::OwnerObserver` 由唯一 owner 执行者调用。每次调用恰好执行一次真实
`nx_bus_owner_service`，原样返回服务结果。BUSY 单独计数，其他错误另计；观察时钟
逆跳写入 diagnostic_result，不覆盖 service_result，也不阻止继续 drain。

随后在已有短 metadata guard 内复制 active ticket、request state 和 idle；不调用
硬件、不遍历槽数组。复制结果不保留 request/payload 引用。quarantines 是观察次数，
不是失败操作数量。服务 SUCCESS 也不等于当前 operation 最终 SUCCESS；最终结果仍通过
acquire-observed request result 获取。它不会从 void device service 推测不存在的错误。

observer、owner、slots 和 guard/executor/clock 必须保持同一绑定及生命周期。生产者
继续使用原 owner guard；执行者不会因接入诊断增加一层设备热路径 vtable。detach 后
owner 继续拥有所有未排空工作。诊断输出也不得覆盖已接受的 operation 或 payload。

## Stream 快照与容量预算

`nx_stream_observe` 在一次短 IRQ guard 中复制 maintained aggregates，工作量与 block
数量无关，不扫描 slot 或读取 payload。free_blocks 表示总空闲数；next_free 表示普通
stream producer cursor 对应槽是否可用。乱序 release 可能使总空闲不为零而当前 cursor
仍被占用。preposted producer 有独立 cursor，其下一目标必须用对应 reservation API
判断，不能把 next_free 当成连续 DMA 的下一目标许可。

`nx_stream_budget_calculate` 是冷路径的纯检查，调用方声明 rate、block units、每 unit
字节、最长 FIFO frontier release delay、硬件目标和 spare 数量。其计算为：

```text
retained = ceil(rate * release_delay_us / (block_units * 1000000))
required_blocks = hardware_targets + spare_blocks + retained
required_payload_bytes = required_blocks * block_units * unit_bytes
block_fill_us = floor(block_units * 1000000 / rate)
```

rate 和 block 使用相同单位。例如多通道 ADC 可使用完整 scan 作为单位，unit_bytes
显式包含每 scan 的全部通道。两次 uint64 乘法以及 size_t 加法/乘法均检查溢出；非法
输入或溢出保留旧输出并返回 INVALID/EXHAUSTED，绝不饱和为一份貌似通过的预算。
configured_blocks >= required_blocks 才满足声明容量；目标换址 latency 必须严格小于
向下取整的 fill 时间。该结果不自动创建 backpressure，也不消除 FIFO 堵塞、CT ABA 或
实际时钟/IRQ 延迟。最终物理资格仍要求对应板卡的实测速率和最坏服务间隔。

## 外部应用的隔离恢复策略

`nexus-examples/apps/uart_owner_runtime/quarantine_policy.c` 保留旧 SDK pin，直接组合
已有 BusOwner API。真实 main 的 owner progress 位点均接入该策略，故服务错误会进入
可观察的故障阶段，而非被 `(void)` 静默丢弃。BUSY 作为正常等待处理。

实际错误关闭 admission，发布 FAULT_OBSERVED，并建立明确的有限清理时间/步数预算。
时钟逆跳、deadline 加法溢出或预算耗尽发布 ESCALATED_RETAINED。后续调用仍然执行真实
service；没有 kill worker、reset、自动重放或归还未排空 buffer。实际 idle 最终发布
LOAN_DRAINED，但只证明 owner 借用已退出；producer join、IRQ detach、硬件 stop、
observer join 和通知销毁仍须按原生命周期次序完成。

故障 sink 在 guard 外执行，必须有界且不重入。应用保存阶段和故障状态并唤醒协调者；
产品的安全输出、设备复位和现场处置仍归外部产品工程。这份示例策略不建立安全等级。

## 存储与验证边界

以下是实际 Cortex-M4F GNU Arm 14.3、`-fshort-enums` ABI 的 sizeof fixture，数组符号
由实际 ARM 编译并用 nm 读取。它只量显式对象布局，不冒充整镜像代码大小或物理周期。

| 显式对象 | 字节 |
|---|---:|
| request observer / copied observation | 48 / 40 |
| owner observer / copied observation | 96 / 72 |
| stream copied observation | 32 |
| stream budget input / result | 48 / 32 |

测得默认 request 24、request slot 16、stream 40、stream slot 40、BusOwner 84、owner
slot 48 字节。默认结构没有新增字段；未选择 observer 时不创建上述 sidecar。其他
编译器、enum ABI 和 pointer width 必须重新测量，不能直接套用这些数值。

新增 Google suite 为 15 项 observer/budget 与 7 项 descriptor/provider state-machine。
其中 Native UART 执行 32 条序列、SPI 16 条序列、ADC 4 条消费/停止序列；这些循环不是
额外 Google case 数。I2C 的公共 dispatch 序列 mock 外部异步 provider 依赖，证明公共
借用/重试合同，不宣称 Native I2C 异步硬件实现。外部示例 28 项实际 owner/lifecycle
Google case 中新增 6 项 quarantine policy。首次真实 stub RED 和最终 GREEN 原始日志、
fresh XML、命令与退出码保存于
`tools/testing/evidence/io-observer-iteration/`。ARM32 容量检查曾触发 type-limits 严格
编译错误；同一实际生产 ABI 的 RED/GREEN 与极端容量边界回归一起保留。

这些是开发软件证据。clean source SDK、外部消费者、完整候选矩阵由统一发布流程再次
绑定最终源码。实板未连接，物理 DMA 换址、错误注入、串口时序和安全输出均未执行。
