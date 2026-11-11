# epoch 安全的 AMP 共享内存通道

`Nexus::AMPIPC` 提供有界单向通道。公共 `nexus/components/amp_ipc_storage.h` 定义
调用者提供的 endpoint、配置和本地 slot 存储，`amp_ipc.h` 定义操作与 ticket。发送方
和接收方分别独占 offer／ACK cache line，每端只有一个串行特权 Thread 执行者。
双向通信显式使用两条通道；组件不创建堆对象、注册表、中断或 worker。

[FIG-16 当前 I/O 装配](../../docs/assets/diagrams/io-evolution.svg)说明平台接口与
可选软件机制的关系；本组件的 offer／ACK 与会合责任以下文为准。图不代表具体
多核硬件已接入或物理退出已通过。

## 端口、身份与布局

集成方提供精确共享 payload window、slot 数、非零 boot epoch、coherent 原子控制字
操作和 cache 维护端口。cache granule 是 1～64 字节的二次幂，必须整除固定 64 字节
wire 隔离单元。全部共享对象与 payload span 覆盖完整且对齐的 cache line；发送方
payload 借用不能互相重叠。准入前检查地址溢出、类型对齐和存储／输出别名。wire
只发送 offset／length，不发送跨核指针。

独占 role claim callback 授予硬件所有权，并原子拒绝重复或递减的本地 boot epoch，
普通 join 后也不能重用。权威 boot owner 跨复位保留历史，耗尽时关闭准入。endpoint
不重建 live 或已 join 的存储。每 slot 的 32 位 sequence 单调增加且不回绕，最终
sequence 后 slot 退休。复制的 ticket／control 仍要求原 endpoint 存储存活。

## 发送、接收与退出

发送成功后继续借用 payload，直到 poll 以 acquire 观察到 sender epoch、receiver
epoch 和 slot sequence 均精确匹配的 ACK。cache 失败或 peer epoch 变化保留现有
借用；epoch 不匹配永久关闭该 endpoint，恢复旧 boot word 不能重新开放。旧 boot
的 offer 不生效，也不能阻塞后续 slot 中的新 offer。

接收准入先 invalidate offer，再 invalidate payload。调用者在 finish 前会合所有
reader；有效 finish 即消费本地 view，即使 ACK clean／publication 失败也不能再次
使用该 view。ACK 失败保留 endpoint 存储，可重试完成发布。

close 阻止新 send／receive。普通 join 等待精确 sender ACK 和 receiver finish；
reset join 另外要求端口证明指定旧 peer 的 CPU、DMA、IRQ 和 observer 永远不能再
访问该物理 lane 与 payload，且 receiver view 已被 finish 消费。新 epoch、取消或
超时本身不是停止证明。仅成功 join 归还独占 owner；复制 ticket／control 的观察者
也全部会合后，才可释放 endpoint／control 存储。

## 软件与硬件边界

此目录提供通用软件机制，不选择具体多核硬件集成。共享区可达性与属性、资源分区
owner、控制字原子性、cache 顺序和可信 reset 证明必须由精确平台提供。STM32H743
为单核，不是 AMP 目标；独立芯片扩展也须按自身合同验收，不能继承通用算法的资格。
主机 GoogleTest／GoogleMock 证明实际执行范围的软件行为，未执行物理 cache／reset／
IPC HIL，IO-40 原始唯一硬件 owner 与 remote-reset／cache 退出保持开放。
