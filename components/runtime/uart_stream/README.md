# UART 到复制字节流

`Nexus::UARTByteStream` 将既有 `nx_uart_port_t` 的 TX 借用转换为
`nx_byte_stream_t` 的立即复制合同，用于 AT／模块等普通 task 消费者。
调用者静态提供 `nx_uart_stream_t` 和精确 TX buffer；context 内只有一个稳定
`nx_uart_tx_request_t`，没有队列、worker、锁或新的 controller owner。

## 取得、运行与退出

1. 外部 TOML／factory 选择并启动实际 UART byte profile，交给唯一执行者。
2. `nx_uart_stream_init` 连接既有 face、同域 clock 和稳定 TX copy buffer。
   所选 provider 在实际 submit 时继续检查 DMA 可达性、对齐和资源准入。
3. 上层 write 复制至多 capacity 并提交一个 inner request；返回成功表示本地接纳。
4. 外部 executor 显式调用 `nx_uart_stream_service`。只有 acquire 观察 SETTLED
   并消费真实结果后，才能覆盖 TX buffer 或提交下一次写入。
5. 关闭上层准入并 join 所有 byte-face 调用者，再调用 `nx_uart_stream_stop`。
   它取消／推进原 request；BUSY、失败或 QUARANTINED 继续保留原存储。
6. stop 成功仅结束这个 adapter 的借用。产品随后停止并会合 UART controller、
   RX／IRQ 和时钟，再释放这些外部对象。

取消成功、deadline、DMA TC 或 task 退出均不能替代实际 settlement。stop 使用原
request，不给它新 deadline；clock/协议失败后仍允许继续实际排空。

## 接收与时间

read 消费同一个既有 RX byte 队列，允许与 pending TX 在同一执行者中交错。
EMPTY／零字节映射为 BUSY；UART 队列没有可推导的 orderly EOF。RX OVERFLOW 或
错误可能已经复制过前缀，本 adapter 将该次 count 置零并锁存失步，避免上层 parser
跨丢字节边界继续运行。TCP EOF 由 ESP 等实际协议端点的 CLOSED 提供。

clock 与 UART/Core deadline 属于同一活跃时间域。数值回退会关闭准入；相同／更大
数字的新 epoch 仍由已有 ClockDomain／产品的 stop、join、rebind 流程隔离，不能
依靠数字判断恢复旧 deadline。provider、clock、table、context 和缓冲都保持存活。

`uart_byte_stream_test` 执行生产 adapter、UART wrapper 与 Core request，模型只在
UART 硬件边界。覆盖复制隔离、ACCEPTED 后的 inline/迟到失败、quarantine、取消、
RX loss、时钟和无 borrow 拒绝；ESP 测试另有一条真实双组件组合路径。
