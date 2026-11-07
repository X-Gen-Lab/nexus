# 可选映射存储所有权

`Nexus::MappedStorage` 提供有界、单执行者的映射存储机制。每个只读接口共享方法表，
引用独立的调用者状态。调用者提供精确 read-loan slot、请求、backend context、
aperture 和单调时钟；没有分配、注册表、队列、worker 或 OS 锁。

[FIG-23 共享机制所有权](../../docs/assets/diagrams/io-extension-ownership.svg)说明
读借用、唯一 writer 与外部平台证明的边界，不是实板执行证据。

## 读写互斥与生命周期

map acquire／release 和 writer service 执行常量 metadata 工作，backend service
按其显式有限预算推进。任何活跃 CPU、XIP 或 DMA-read 借用都会排除整个 aperture
上的所有 writer。reader tag 必须由所选 backend 支持，它本身不证明可执行性或 DMA
可达性。消费者先退出会合，再以精确 slot／epoch 释放；最后一次 release 撤销映射，
撤销失败继续保留借用。slot 和 writer epoch 不回绕。stop 关闭准入，等待 reader 和
当前 writer 结束，不强制撤销。

PROGRAM／ERASE 使用 Core 请求发布合同。准入在读输入或触碰硬件前检查几何、
地址溢出，以及 owner、slot、backend、aperture 存储重叠；拒绝不保留引用。接受后
请求 metadata 和 program 输入保留到 acquire 观察 `SETTLED`。cancel／deadline
开始撤源，不释放存储；失败排空使请求隔离，后续 service 可重试。部分效果可见但
不会重放。所有 command／cache／DMA writer 必须经过唯一 backend；物理适配器
必须证明 aperture／cache／DMA／IRQ 撤回和 XIP 执行排除。

## Native 模型与接入边界

`Nexus::NativeMappedStorage` 只在 Native 平台图存在，保留调用者初始字节，按 NOR
1→0 编程，擦除为 `0xff`，每次 service 最多改变所配置字节量。取消在进一步模型
变更前锁存停止，drain 结束对输入的访问。显式故障和保留排空控制用于中断回归。
这是 coherent CPU 模型，拒绝 XIP／DMA reader tag，不提供持久化或物理存储驱动。

本组件对应 IO-37 的一个软件增量。SDMMC block、通用 factory／资源装配，以及完整
H743 QUADSPI／串行存储 provider、实际 cache／DMA／XIP 与 HIL 退出仍按原合同
分别验收；不推导 Board 接线。独立芯片参考不自动提升通用组件的完整支持范围。
