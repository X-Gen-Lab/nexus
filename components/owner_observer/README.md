# 可选 owner 执行观察

`Nexus::OwnerObserver` 用调用者提供的侧车存储包裹一次真实
`nx_bus_owner_service`。它保留实际 service 返回结果，记录 BUSY、错误、观察到的
隔离状态与最大观察 service 间隔，不扫描全部 slot、不增加默认 owner 字段、不创建
worker，也不改变请求结清。

## 执行与存储

唯一执行者提供单调时间戳和独立输出。硬件 service 在已有 owner guard 外执行，
之后仅在短 guard 内取得 metadata 快照。观察时间回退是独立诊断错误，不能阻止必须
执行的 service 或排空。计数饱和不回绕；隔离次数是观察次数，不是不同 operation 数。

owner、slot、callback 和侧车必须保持存活，直到所有 wrapper 调用退出且 observer
已 detach。detach 只撤销侧车引用。timeout、隔离请求或 observer 失败都不允许重用
请求／缓冲区。复制输出包含 ticket 和状态，不携带借用的 payload 指针。

同一有界快照还报告 `accepting`、`queued` 和 `slot_available`，分别表达新准入和
保留工作。BUSY 本身不证明队列已满；仅在 `diagnostic_result` 成功时使用这些字段。
可将副本连接到 `nx_io_diagnostic_facts_t`，无需为 owner 引入 I/O 依赖或保留 live
request 指针。

可选 observation／sidecar 布局变化要求消费者重新编译，但不增加默认 owner 或
provider 字段。其 RAM 成本按所选 ABI 测量，不能假定为零。

[观察与恢复合同](../../docs/design/io-observation-recovery.md)统一说明 Core observer、
stream budget、可选存储测量和外部应用升级处置策略。
