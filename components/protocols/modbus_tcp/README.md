# 有界 Modbus TCP 会话

`Nexus::ModbusTCP` 管理一条连接的 MBAP/PDU 字节边界，不拥有 socket、任务或寄存器表。
产品为每条连接提供一个静态 `nx_modbus_tcp_t` 和既有 `nx_modbus_registers_t` 回调。
当前支持 holding-register read（03）、single write（06）、multiple write（16）。

`feed` 可接收任意分片，只消费到一条完整请求。成功后 `response` 借出稳定响应，
后续输入返回 BUSY 且消费零字节；发送方复制或结清借用后才能 `release`。因此慢客户端
不会使相同写请求被重复执行。流水线剩余字节由连接 owner 保留，不能丢弃或覆盖。

事务 ID 原样回送，protocol ID 必须为零，length 必须为 2..254，unit 必须匹配产品
配置的 1..247。错误 MBAP 锁存失败，产品关闭连接并会合所有响应借用后再重建；
不从任意负载搜索“可能正确”的下一条头。合法 PDU 的错误通过标准 exception 回复。

所有调用和 bounded register callbacks 在同一串行 task 中执行；重入被拒绝。
`authorize_write` 每次执行写入前检查，`write_atomic` 承诺全部提交或全部不变，
仍需在并发产品状态下原子复查授权。TCP/Modbus 不认证对端，访问控制属于产品。

组件不等待；真实 TCP owner 决定有限 deadline、RX/TX 容量、连接数与关闭策略。
这不是通用 socket server，也不承诺网络吞吐或物理 HIL。
