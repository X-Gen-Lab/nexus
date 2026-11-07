# 显式有界总线 owner

本目录将直接 I/O 组合成多 task producer、单 executor 的有界队列。`Nexus::BusOwner`
提供通用 metadata 机制，`Nexus::SpiOwner`、`Nexus::SPIAsyncOwner`、
`Nexus::SPIDMATransactionOwner` 与 `Nexus::UARTOwner` 连接具体传输。target 在
[组件 CMake](../CMakeLists.txt)中定义，没有默认任务、最大池或全局设备查找。

## 架构与资源

[FIG-08 等待与退出](../../docs/assets/diagrams/wait-wake-shutdown.svg)区分权威
谓词、通知与调用者会合。代码入口是 [bus_owner.h](include/nexus/components/bus_owner.h)
和 `src/bus_owner.c`；各传输头说明独立 operation 与 inner request 存储。

调用者提供精确数量 slot、短 guard、实际 executor callbacks、单调 clock 和可选
wake。guard 串行化 producer 与 executor 的 metadata，不能覆盖硬件 start／service／
cancel、等待、payload copy 或 notification callback。Arch guard 必须恢复输入 mask。

slot 复用空闲链／FIFO link，准入与结清仅处理固定量 metadata。每个取消 ticket
是 slot index＋epoch，不向另一 task 传可能回收的 request 指针。epoch 耗尽永久
退休该 slot，不回绕。

## 准入与推进

submit SUCCESS 表示接受，request、operation 和 payload 保持不变直到 acquire
观察 `SETTLED`。拒绝不保留引用。排队时间计入原绝对 deadline；接受后的 notification
错误不能改报为拒绝，executor 必须周期复查或继续推进。

executor start 的 BUSY／错误必须零保留、零硬件效果；部分 start 已生效时应接受并
通过后续 service 报结果。service SUCCESS 才证明硬件／IRQ／延后访问全部结束并
结束 operation 借用，BUSY 或其他 service 错误继续保留并要求恢复。callbacks 在
外部调度的唯一 executor 中、guard 外执行。`terminal_authority` 允许实际 provider
保留自己先锁存的完成／deadline 事实，不能被迟到取消改写。

UARTOwner 使用独立 inner provider request，先结束 inner 访问再结清 outer。
各 SPI adapter 还须核对 endpoint 与实际 controller 身份，不把独立 CS 当作新控制器。

## 停止和回收

stop 关闭新准入并请求已接受项停止，不等于结清。保持 executor service／drain，
生产者消费结果并退出会合；还要 join service publisher、取消控制者和 wake 发布者。
确认 idle 后，才释放 owner、slot、guard、通知与 operation 存储。timeout、cancel
或 observer detach 不允许提前释放隔离请求。

这里是软件组合合同；Native 并发模型不证明实际 IRQ 延迟、bus 电气或物理吞吐。
