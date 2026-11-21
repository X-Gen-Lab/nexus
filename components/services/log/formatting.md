# 可选格式化与 UART 记录

`Nexus::LogFormatter` 沿用既有字节 logger 的 sink 和生命周期。调用方提供独占 scratch；
有界无堆 renderer 仅处理已准入的整数、指针、字符和字面精度字符串。format 在 256
字节内结束，字面 width／precision 最大 1024；拒绝动态宽度、浮点和 `%n`。参数类型、
可读字符串范围及输入与 scratch 分离仍由调用方保证。

只有完整记录进入 `nx_log_write`；容量不足返回 OVERFLOW，不发布前缀。没有隐式换行、
堆、全局 scratch 或后台执行者，severity 仍在格式化前过滤。接口保留 Log 的任务上下文
限制。Shell 的固定诊断文本复用同一 private renderer，不经传输中转，不调用 libc
formatter；Native 独立参考比对与真实 ARM 符号闭包分别验证行为和无堆成本。

`Nexus::LogUART` 在现有 `nx_log_sink_port_t` 后使用一条明确的调用方 staging buffer
和真实 UART request。Write 在准入前复制完整记录，返回后不再访问 logger 输入。
SUCCESS 只表示私有副本已接受，不证明线上完成；pending 时拒绝下一条，绝不覆盖被
借用字节。唯一调用方执行者独占 UART TX，并显式调用 service／flush。

Flush 只推进一次，不等待，也不替换已接受记录的原始绝对 deadline，保留实际结清结果。
Stop 关闭准入并只请求一次取消；timeout、cancel、QUARANTINED 都不释放 request 或
staging。持续推进到 acquire-observed SETTLED，并会合 publisher 后才能回收存储。
适配不停止共享 UART RX，也不创建任务。
