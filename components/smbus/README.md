# 可选 SMBus packet PEC 适配器

`Nexus::SMBus` 在已有异步 I2C endpoint 上组合有限 write、read 和 write/read 的
packet error code（PEC）。它使用公共 Nexus 类型、调用方存储和一个串行执行者；
未选择组件时，普通 I2C 不增加字段、缓冲区或任务。

本组件提供包级 CRC8，不声明硬件 SMBTYPE、SMBus 时钟超时、ALERT／ARA、ARP、
Host Notify、动态 block count 或完整 SMBus 电气／协议符合性。CRC8 用于发现传输
错误，不提供身份认证。命令、寄存器、重试及设备策略属于外部产品。

## 绑定与构建边界

公共合同在 [smbus.h](include/nexus/components/smbus.h)，实现为 C11。
`nx_smbus_binding_t` 是同一 endpoint、其 controller 和准确七位 wire address 的
只读组合；生成集成应从同一 endpoint 事实取得这些字段。手工集成负责地址准确，
初始化的 `endpoint_on_port` 身份检查不能从 opaque context 验证地址。

在 assembly 中显式选择 `components = ["smbus"]` 后，包含生成的
`nexus_smbus_factory.h`，通过 `nx_factory_smbus_binding_ref` 与现有
`NX_I2C_DEVICE_REF_*` 获取绑定；该只读对象直接引用同一组静态接口，不复制
endpoint、controller 或 ops。只有七位地址 `0x08`～`0x77` 的异步 I2C endpoint
具有绑定；轮询、十位地址和无效 ID 返回 NULL。普通 factory 头不依赖本组件。
组件是可选 CMake 目标 `Nexus::SMBus`；未选择时不生成 companion 或静态绑定。

底层必须支持异步 submit、cancel 和显式 service，并由独立 owner 预先配置。
初始化本组件不启动总线，不取得引脚，也不调用 controller stop。应用链接可选组件，
提供持久 owner／binding／request，并在同一未屏蔽 Task 中推进；不自动创建执行者、
队列或 OS 锁。既有 [I/O 所有权](../../docs/explanation/ownership.md) 区分 ACCEPTED、
排空、QUARANTINED 和 acquire-observed SETTLED；本组件分别保留外层包与内层 I2C
请求，不能以一层的取消替代另一层的结清。

## Payload、wire storage 与 PEC

每个非空方向为 1～32 payload 字节，至少一个方向非空。PEC 使用多项式 `0x07`、
零初值、不反射、不做 final XOR；它包含准确的地址方向字节及整个包的 payload。
32 字节上限包含调用方提供的 command、count 等 framing 字节，因此不承诺
command、count 之外另有 32 字节 block data；动态 block count 仍由外部协议处理。

| 包形式 | TX 存储 | RX 存储 | I2C 请求 |
| --- | --- | --- | --- |
| write | const payload 后紧接一个明确授权的可写 PEC 字节 | 空 | 一个 write message，长度为 payload+1 |
| read | 空 | capacity 至少为 payload+1 | 一个 read message，末字节为 PEC |
| write/read | const payload，不追加 TX PEC | capacity 至少为 payload+1 | 一个 write 和一个 read，中间为 repeated START，PEC 仅在最终 RX |

write-only 的 `tx.pec` 必须指向同一个实际连续 wire allocation 的 payload 尾部；
adapter 不去除 const，也不复制或改写 TX payload。write/read 的 `tx.pec` 必须为
NULL。整个 RX capacity、TX wire span、request、owner、binding 和公共 transport
元数据必须分离；opaque provider control 的完整范围由外部集成保证不重叠。

prepare 不访问 payload 或修改 PEC。submit 完成校验并接受外层请求后，才写入明确的
TX PEC 尾部并提交内层 I2C。内层在此之后拒绝时，外层仍按 ACCEPTED 返回 SUCCESS，
随后以该错误 SETTLED；调用方始终读取最终请求结果判断包是否成功。

## 推进、停止与结果

应用初始化 request base，再 prepare、submit，并调用 service 直到 acquire 观察
SETTLED。成功计数是 TX 与 RX payload 字节之和，排除地址和 PEC。错误计数为零；
这不撤销可能已经发生的远端效果或部分 RX 写入，也不允许自动重放。

只有内层 I2C 已真实 SETTLED、wire 计数完整且 RX PEC 匹配，包才以 SUCCESS 和
`NX_SMBUS_PEC_VALID` 结束。短成功、NACK、仲裁丢失或 PEC 不符均不发布有效 RX；
调用方不得消费失败响应。write 的 PEC 状态保持 `NOT_CHECKED`，因为 ACK 不证明
对端采用了何种 PEC 校验策略。

cancel、deadline 或 stop 不撤销 wire storage 借用。内层 QUARANTINED 时，外层保留
请求、绑定、消息及全部数据，等待原 provider 显式恢复并继续排空。stop 关闭本组件
的新准入并推进自己的请求；成功后重复 stop 不调用底层总线。应用等所有消费者停止
后，再由独立 controller owner 归还硬件。重新初始化递增 owner generation，旧 READY
请求不能跨代提交；销毁或复用 owner 存储前必须加入所有使用者。

## 验证范围

[GoogleTest／GoogleMock 合同](../../tests/google/smbus_test.cpp) 执行生产 C，模拟
外部 I2C 和时钟，覆盖固定 PEC 向量、repeated START、错误、部分完成、取消、隔离、
陈旧 generation 和回调重入。新目标的维护构建、ARM、SDK 和实板结果分别记录；源码
或主机模型本身不建立硬件 SMBus 模式或物理资格，physical HIL 为 `not_executed`。
