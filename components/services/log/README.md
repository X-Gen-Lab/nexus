# 实例级有界日志端口

`Nexus::Log` 是调用者持有的同步日志入口。实例只保存 sink、最低 severity 与短
序列化状态；没有全局 logger、格式化、动态分配、后台 worker 或 transport 发现。
接口见 [log.h](include/nexus/components/log.h)，实现为 `src/log.c`。

调用者提供同步 sink 的 write／可选 flush。每次返回，包括 timeout／错误，都结束
字节缓冲访问；异步／DMA sink 必须先有自己的已结清 adapter。有限绝对 monotonic
deadline 由 sink 理解，日志组件不建立新的 clock 或完成线程。

## 调用与退出

severity 在触碰 sink 前过滤。task 可并发尝试 write，非阻塞序列化使并发或递归
调用返回 BUSY，由调用者选择 drop／retry。M0／M0+ metadata 使用同 CPU 特权
Thread 的 saved-PRIMASK 排他，恢复 mask 后才调用 sink。IRQ、NMI／HardFault、
跨安全域 writer 不在合同内；flush 使用同一上下文前提。

stop 拒绝新 write，sink 仍在执行时返回 BUSY，不强制取消。确认没有 active sink，
并会合所有未来可能发布的用户后，才回收 logger／sink 存储。write SUCCESS 可能只是
被过滤，不表示物理发送；flush 的持久化或线上完成含义由显式 sink 合同决定。
