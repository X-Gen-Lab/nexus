# Native：宿主线程与等待

本目录提供使用 pthread 的真实宿主适配，用于应用组合、并发和退出合同验证。
公共声明位于 `os/include/nexus/os/`，实现在 `native.c`。装配侧通过
`nexus/os/storage.h` 取得本目录选定的完整布局；业务只使用公共 opaque 引用。
选择 Native 完整平台时建立相应目标，仅 CPU Runtime 不因此自动探测或导入
POSIX 后端。普通操作不再公开 `nx_native_*` API。

## 对象与上下文

调用者首次使用前将控制对象清零，并提供任务栈、通知对象、队列缓冲区和深度。
原生字段由后端维护，业务不修改它们。通知在同一互斥量下
重查序列，再用基于 CLOCK_MONOTONIC 的条件变量等待。公共等待流程在唤醒、超时
或错误后仍重查权威判定条件；通知不代表请求完成或内存归还。

任务使用调用者栈，但 pthread 库可以分配内部管理资源，宿主执行不证明 MCU
无堆或 RAM 预算。任务正常返回结束真实 pthread。`nx_task_join_until` 先尝试实际
会合；需要等待时使用 GNU `pthread_clockjoin_np` 和同一单调截止时间，成功后才
回收栈、上下文和控制存储。显式 `NX_DEADLINE_NEVER` 才使用无限等待。超时保留
全部资源；自 join 返回 `CONTEXT`。不得绕过适配器取消任务或执行 `pthread_detach`。

Native 只有逻辑优先级 0，不模拟 MCU 抢占优先级。永久任务不提供 join，入口返回
终止宿主进程。`nx_os_run` 执行引导回调后停驻当前调用线程，保留调用者栈且不增加
worker；成功不返回。宿主进程退出由外部应用政策决定。

`nx_os_now_us`、等待、join 与 `nx_sleep_until` 使用真实 `CLOCK_MONOTONIC`，
不能混用 Native 外设模型的手动时间域。有限 sleep 不创建 timer 线程，`yield`
不保证公平。ISR 发布返回 `CONTEXT`；POSIX signal handler 不属于支持的调用域。

## 关闭与验证

普通 queue 不携带关闭状态；需要关闭广播时选择独立 channel。每个并发 channel
操作使用一个独占调用者 waiter，受明确最大并发数约束。关闭拒绝发送并唤醒阻塞者，
保留已有数据供排空，关闭且排空后的接收返回 `CANCELLED`。初始化或销毁的部分
失败保留内核资源，调用者只能继续销毁，不能重新初始化或回收对象。

关闭准入后保持资源责任者继续推进服务，结清请求和流块，等待全部相关任务退出，
撤销全部发布源，最后销毁通知、队列和保护区。当前不模拟硬件 ISR 或特权；
宿主通过不能推导 NVIC、DMA、真实实时性或信号处理安全。所选宿主必须真实编译、
链接所需 GNU join 能力；没有能力时拒绝，不退成无界 join。

[FIG-08：等待、唤醒与退出](../../docs/assets/diagrams/wait-wake-shutdown.svg)
说明上层流程；公共接口和后端差异见 [OS 总览](../README.md)及
[OS 合同](../../docs/reference/contracts/os/execution.md)。

文档链接面向完整源码仓库；SDK 未携带文档时，按其 manifest 记录的源码提交读取原件。
