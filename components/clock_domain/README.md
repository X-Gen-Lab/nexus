# 显式时钟准入与退出屏障

本组件以调用者提供的精确 consumer token 跟踪时钟使用，不分配内存，不创建任务、
固定最大池或全局电源注册表。它服务于平台生命周期，不自行实施功耗策略。

特权 owner 关闭新准入，分别驱动每个 consumer 停止，再检查绑定 epoch 的零借用
屏障。操作在保存并恢复输入状态的 Arch 短临界区内管理 metadata。

## 所有权与上下文

存储与构造属于私有装配接口。token 必须保持存活和不变，直到该原对象成功释放；
复制字段不能释放被保留的对象。容量约束准入和输出别名扫描的工作量。

生命周期只有一个串行特权 Thread owner；consumer acquire／release 也可由同 CPU
可配置 IRQ 调用。NMI、fault handler、SMP 和宿主 signal 不属于此 guard 合同。

`close` 不取消 I/O，`quiescent` 不证明未登记 consumer 已停止。本组件不停止 Tick、
修改 PLL、重置 deadline 域或替 OS 作决策。调用与失败责任见
[时钟域和睡眠合同](../../docs/design/clock-domain-sleep.md)。
