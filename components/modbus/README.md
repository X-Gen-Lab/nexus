# 有界 Modbus RTU 与显式 RS485 端口

`Nexus::ModbusRTU` 提供 task 中串行推进的 RTU 协议机制，不创建 UART、IRQ 队列、
worker、全局寄存器表或分配器。产品提供 register callbacks、RS485 transport 和
同一单调时间域的到达时间。入口见
[modbus_rtu.h](include/nexus/components/modbus_rtu.h)及 `src/modbus_rtu.c`。

实例静态保存至多 256 字节 ADU 与至多 125 个 register value。明确地址为 1～247，
0 仅作 broadcast；baud、每字符位数、TX／abort 有限预算由产品给出，不猜测电气。

## 协议和产品边界

外部 task feed 时间戳字节，再 poll 处理 silence 和有限同步响应。IRQ capture／queue
在外部，overflow 必须通过 rx_error 显式通知，不能把漏字节拼成有效帧。所有 feed／
poll／recover 串行，时钟与输入事件保持同域、单调。

register read 返回 coherent snapshot；write_atomic 要么提交所有 value，要么不改。
authorize_write 必须存在，对 broadcast 同样检查。关键写入的产品授权与互锁须在
并发产品状态下再次原子核对；RTU 和 CRC 本身不认证 peer 或 request。

RS485 port 显式控制 DE，write 可用 DMA，但 drain 必须观察最后 stop bit。write／
drain 失败后 abort_and_settle 成功前，必须停止 DMA 并结清全部 callback；失败会
锁存 FAULTED、保留 TX 存储，DE 继续保持发送直到 recover 结清。不是普通 timeout
即可释放的情况。成功 drain／abort 后端口不得保留 bytes，direction(false) 须建立
receive mode。

初始化先请求 receive mode，失败如实报告；故障 TX 借用未结束前不能重新 init。
回收实例前先执行对应恢复／退出并会合全部调用者。波形、收发器电压、串口 timing、
现场网络与产品控制安全仍需具体板卡验证。
