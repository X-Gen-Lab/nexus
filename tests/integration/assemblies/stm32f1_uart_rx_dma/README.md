# F103RB USART2 有限 DMA 接收软件装配

`f103rb-rx-dma.toml` 选择 HSI8、PA3 和 DMA1 通道 6。应用通过强类型
factory 获取 RX 接口，提交调用者持有的 `nx_stream_t`，在任务中推进 `service`。
驱动确认通道与两个中断源退出后才交还有效块。调用者持有的块需要显式归还。

满块和应用停止独立于 IDLE。ES096 §2.9.2 说明偏离名义频率时可能漏报 IDLE。
块间重新挂载存在接收间隙。该装配使用单独的 RX 接口，I2C1 TX 不能同时占用通道 6。

## USART2 同时收发

[`f103rb-duplex.toml`](f103rb-duplex.toml) 的软件参考使用 PA2 TX、PA3 RX，
HSI8 和 115200 8N1。一个强类型 UART 实例编排 DMA1 通道 6 RX、通道 7 TX
和 USART2 中断。应用开启调用者持有的流，再提交独立的 TX 请求，定期调用
`nx_uart_port_service`。RX 池与 TX 请求及数据的存储范围须分离。

TX 等待真实 USART TC 才报告成功。RX 按已观察到的 IDLE 或满块结算。
停止 RX 后仍可排空 TX。最终停机还需消费者归还已借出的 RX 块。
两通道与后续 I2C1 DMA 模式排他。块间存在重新挂载的空窗，外部线路与
发送端 IDLE 时序尚无实板验收。
