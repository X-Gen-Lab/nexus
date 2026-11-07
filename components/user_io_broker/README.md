# 用户 I/O 代理机制

`Nexus::UserIoBroker` 提供由内核调用的可选窄代理。scalar GPIO 路径按显式
principal／device／mask 策略接纳一条消息。组件不预建实例、最大借用池、worker、
分配器或隐式 OS 依赖；provider 状态和构造保持私有，集成内核提供 broker、call slot
和受信内存端口的存储。

[FIG-25 复制 TX 所有权](../../docs/assets/diagrams/user-io-copied-ownership.svg)
展示 kernel、现有 owner 与调用者的复制和退出责任，适用于该精确可选机制。

## GPIO 与映射责任

公共 wire message 仅含 scalar device／operation／mask，principal 来自内核身份
端口。两个 user span 都必须先成功 pin 地址空间，才调用有界 typed GPIO。pin 成功
即转移相应责任，即使之后撤权或清理失败，也不能抹除它。

精确 private loan ticket 要求 provider 真正结清且 observer 已会合，之后才 unpin。
排空或 unpin 失败保留 slot 和 mapping；清理重试不重放 GPIO。

本机制不提供 user-mode 入口或权限提升。FreeRTOS SVC 路由、任务身份、实际 MPU／
TrustZone、用户故障约束和内核 pinner 仍由集成内核提供。软件测试不建立物理隔离或
电气资格。完整权威说明见[用户 I/O 代理合同](../../docs/design/user-io-broker.md)。

## 复制 UART TX 的可选组合

同一 target 还通过 `user_io_async.h` 与受信的调用者存储
`user_io_async_storage.h` 提供显式复制 UART TX。它复用已有 BusOwner／UARTOwner
队列，在接受前复制 user bytes，之后只持有特权存储。

只有精确的受信 session／slot／epoch、真实 provider drain 和 owner 发布者会合
完成后才能消费结果。关闭后的 privileged reap 还要求实际 outer gateway 已排空；
最终 join 才释放外部独占 role。复制错误、取消、隔离或证明失败都不允许重放或提前
重用存储。

此通用机制不提供 SVC／CMSE 入口、内核身份／复制／session authority、MPU／
TrustZone 隔离、NS RTOS 或物理资格。IO-39／SOC-36 的完整原退出条件继续开放。
