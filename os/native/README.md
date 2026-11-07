# Native：宿主线程与等待

本目录提供使用 pthread 的真实宿主适配，用于应用组合、并发和退出合同验证。
公开声明位于 `include/nexus/os/native.h`，实现在 `native.c`。选择 Native
完整平台时建立相应目标，仅 CPU Runtime 不因此自动探测或导入 POSIX 后端。

## 对象与上下文

调用者提供任务控制对象、栈、通知对象、队列缓冲区和深度。通知在同一互斥量下
重查序列，再用基于 CLOCK_MONOTONIC 的条件变量等待。公共等待流程在唤醒、超时
或错误后仍重查权威判定条件；通知不代表请求完成或内存归还。

任务使用调用者栈，但 pthread 库可以分配内部管理资源，宿主执行不证明 MCU
无堆或 RAM 预算。任务正常返回结束真实 pthread；仅 `pthread_join` 成功后才
回收栈、上下文和控制存储。Native 退出等待没有截止时间参数，不等同于 RTOS
有限退出等待。不得绕过适配器取消任务或执行 `pthread_detach`。

## 关闭与验证

关闭准入后保持资源责任者继续推进服务，结清请求和流块，等待全部相关任务退出，
撤销全部发布源，最后销毁通知、队列和保护区。当前不模拟硬件 ISR 或特权；
宿主通过不能推导 NVIC、DMA、真实实时性或信号处理安全。

[FIG-08：等待、唤醒与退出](../../docs/assets/diagrams/wait-wake-shutdown.svg)
说明上层流程；公共接口和后端差异见 [OS 总览](../README.md)及
[OS 合同](../../docs/design/os-contracts.md)。
