# 单执行者 AT 客户端

`Nexus::ATClient` 使用已有 `nx_byte_stream_t`，一个产品 task/main-loop 负责 submit、
service、callbacks 和 recovery。缓冲区、URC prefix 数组及 context 均由调用者持有；
无 UART 新 owner、worker、heap 或模块扫描。

submit 将无 CR/LF 的命令复制进显式 TX buffer 并追加 CRLF，调用后输入可复用。
service 在有限绝对 deadline 和非零 stream-call budget 下推进 partial TX 与 RX。
同一时刻只接纳一条 command。标准 OK、ERROR、+CME ERROR、+CMS ERROR 完成 command；
其余完整行作为 RESPONSE，匹配产品选定前缀的行作为 URC，回调 view 仅在当次有效。

模块通过已审核行声明精确 binary 长度时，产品可在回调内调用 expect_binary；后续
这部分字节按 BINARY chunks 报告，CR/LF/NUL 均保持为数据。没有默认模块型号、prompt
语法、socket 状态机或命令重放。回调除 expect_binary 外不能重入 client。

timeout、丢失连接、文本 NUL、line overflow 或 clock rollback 锁存 desynchronized。
产品先停止新命令，排空/重置模块到已知边界并会合传输，再显式 recover；recover 只
重置软件 parser，不证明硬件已恢复，也不会再次执行可能已生效的 command。

stream 的 write/read 返回后不能保留 caller buffer；更底层 UART/DMA adapter 自己
负责真实借用与 settlement。service 的 budget 限制 stream 调用次数，每次 RX 最多
64 bytes，不把它称为任意 callback 的硬实时墙钟上界。
