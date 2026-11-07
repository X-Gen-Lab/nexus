# 显式有界 I3C controller 核心

`Nexus::I3CController` 是无动态分配的 SDR controller 软件核心，依赖 Core 和注入
的硬件／clock 端口。它不自动探测设备、创建 IRQ／worker／OS lock 或选择 Board。
公共 [i3c_controller.h](include/nexus/components/i3c_controller.h)定义 request、
endpoint、ticket、event，`i3c_controller_storage.h` 定义调用者存储与硬件边界。

## 装配与推进

调用者提供零初始化 controller、target／event 存储、策略、时钟和独占 claim 端口。
一个串行 Thread 执行全部 API，port／clock callback 不得重入、调用应用或修改保留
状态。固定硬件准备和整个 bus 的实际归属由外部集成证明。

一次只接受一个有限 DISCOVER、PRIVATE 或允许的 CCC。PRIVATE 为 1～65535 字节；
CCC 仅允许明确 ENEC／DISEC 和 GETMWL／GETMRL／GETPID／GETBCR／GETDCR／GETSTATUS
方向与长度，拒绝改地址、HDR、controller handoff。endpoint 绑定已确认 target 的
controller／index／epoch，不是 factory ID；tentative／retired target 不可取得。

service 每次推进一个有界 FIFO／event 阶段或一次 drain proof。接受后 request 和
payload 借用到 acquire `SETTLED`，transferred 仅表示交给 FIFO 的字节，不是 target
效果确认。cancel 使用稳定 epoch，错误不重放。部分 DAA、畸形观察或错误关闭准入，
保留全部已知和 tentative address；失败排空保留 `QUARANTINED`，继续 service 才可能
恢复。

IBI／HOT_JOIN 是复制事件，不调用产品 callback 或自动建 target。IBI 按确认 BCR
接纳最多 4 字节（含 MDB），HOT_JOIN 只提示调用者显式 discovery。overflow 和
rejected_ibi 饱和计数，不丢弃已保留事件，也不宣称采样时间。

## 关闭、复位和支持边界

close 只关闭准入，不改借用、地址或事件。fault 不能普通 reopen；whole-bus reset
须同时证明 wire／FIFO／IRQ 静止以及所有 target dynamic address 失效。仅 peripheral
reset／FIFO flush 不够，失败保留原借用和地址；成功后旧事件仍携带旧 epoch。

stop 有 active work／queued event 时返回 BUSY，调用者继续 service／消费事件；仍有
assigned／tentative address 或 fault 时需先有整 bus reset proof。最终静止后才归还
controller，复制 endpoint／ticket 的观察者结束前存储继续存活。

[权威合同](../../docs/design/i3c-controller.md)与 [FIG-26](../../docs/assets/diagrams/i3c-core-lifecycle.svg)
区分通用机制、H563 私有轮询参考和未完成的整 bus／SoC／Board／HIL 退出。不能由该
核心 target 推导完整 H563 平台或实物 I3C 支持。
