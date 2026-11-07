# 有界 coreMQTT 适配器

本组件将官方 MQTT 3.1.1 coreMQTT 2.3.1 接到 Nexus 字节流与显式产品执行者。
组织仓库为 `X-Gen-Lab/coreMQTT`，整理提交为
`f4445aa70cb211504bca2c9e4177cd74609f5fba`；上游 release parent 为
`2beef04725328923e05e576b884212d53ec97af7`，tree 为
`c216ae5b6848f60d43b498aea71701d9ac5a5159`。组织补丁只移除未使用的主机 CMock
Gitlink／元数据并记录来源；三个生产 C 文件、四个公共／默认头、transport interface
及 MIT LICENSE 保持原始字节，CMock 不初始化也不是依赖。MQTT 5 release 的 API
不同，不属于此 profile 的自动升级。

## 目标、依赖与装配

无参数 `nexus_add_coremqtt()` 消费已配置 assembly 的
`Nexus::Config.NEXUS_SOURCE_DIR`，产品不传 vendor 或 include 路径。helper 选择
该平台下精确组织源码，按 source lock 验证仓库或已安装 SDK 身份，并将实际观察文件
登记为 configure 依赖后创建目标。源码锁、SDK 文件图和配置选择继续由父仓库现有权威
管理；不获取依赖、不创建 source cache override。同一 build 重复选择是幂等的，
私有双路径实现不是产品接口。

厂商源继承所选 Nexus ABI、优化、section、cache／LTO 与 target policy；自有
adapter 额外使用严格告警。实际 helper 生成的 Native／M4F 编译命令检查这些属性。
未选择 helper 就没有相应目标或运行态数据。

`Nexus::CoreMQTT` 只开放 Nexus API；`Nexus::CoreMQTTStorage` 在装配处显式开放
精确锁定的后端布局。两者都不承诺二进制 ABI。已有源码检出使用父 Gitlink 与组织
origin；安装版使用已验证 manifest 和精确依赖记录，均不接受调用者指定后端目录。
普通消费者看不到供应商／私有 profile 头。

```cmake
find_package(Nexus 0.2.1 EXACT CONFIG REQUIRED)
nexus_add_coremqtt()
nexus_add_firmware(my_device SOURCES main.c
    LIBRARIES Nexus::CoreMQTTStorage)
```

此片段展示已有配置工程的显式目标装配，不是完整产品。构建／可见性检查不建立
broker 会话、网络或 Board 资格。TCP／TLS／MQTT 的主机参考关系见
[FIG-32](../../docs/assets/diagrams/lwip-tcp-tls-mqtt-host.svg)；它不是生产 socket 适配器。

## 能力与状态归属

首个 profile 提供 clean-session CONNECT、QoS0／1 PUBLISH、每次一个 filter 的
SUBSCRIBE／UNSUBSCRIBE、PUBACK／SUBACK／UNSUBACK 关联、接收 publish、DISCONNECT
及调用者推进的 keepalive／process。topic、凭据、授权、重连、离线队列、语义重试
数据、持久化和 broker 选择属于产品。QoS2、持久 session 恢复／重放、last will、
MQTT 5 与自动重连不在此范围。

输入须满足 MQTT UTF-8；adapter 不增加 Unicode codec，也不扩大固定库验证之外的
认证承诺。基本长度、NUL／topic wildcard 约束和 filter 位置在 I/O 前检查。

调用者分配清零的 `nx_mqtt_t`、一个 network buffer、官方入站／出站 QoS records
和有限 control-ACK slots。数量和存储显式，无 heap、最大尺寸池、worker、订阅
注册表或强制 OS 锁。数组互不重叠并持续独占借用到 deinit。初始化不连接也不调用
transport。同一 client 的全部调用与回调由一个串行 task executor 执行，重入 guard
不是跨任务 mutex。

官方 `MQTT_InitStatefulQoS` 拥有 publish 状态。control slots 只关联该 client
未完成的单 filter SUBSCRIBE／UNSUBSCRIBE，不复制官方 QoS 状态机。Packet ID
避开未完成 control 和 publish records；选择成本取决于显式 slots 数量，不能称为
恒定时间。

## 端口与执行进度

共享 `nx_byte_stream_t` 由两个只读方法和 context 组成。非空写入只有复制出正数
接纳字节才成功；读取返回正数数据，或以零表示有序 EOF。BUSY／零字节是暂态背压。
回调返回后不保留 MQTT 输入 buffer；TLS 重试由 TLS 自己的稳定 stage 负责。
stream 成功只表示本地接纳，不是对端交付。

每次操作提供有限绝对微秒 deadline 和非零 transport-call budget。
`nx_mqtt_port_now_us()` 为全部 clients 和 transports 提供同一个稳定单调时间域；
上游无 context 的 32 位毫秒计时从它派生，不引入 active-instance global。
`nx_mqtt_port_assert()` 是必须提供的永不返回产品故障钩子，不能凭断言回收 DMA
或仍被借用的存储。

已审 polling profile 为 1 ms，adapter 另行限制 transport 调用次数，停止的时钟
不能令失败循环无限执行。可选有界 idle 回调可在调用者上下文推进下层或等待；
SUCCESS／BUSY 允许重试，其他错误使 session 失败。没有默认 wait 或 scheduler。
Connect 为同步调用，产品须按真实网络延迟安排预算和推进端口；process／keepalive
只在显式调用时执行。官方 keepalive 内部消费 PINGRESP，不生成应用事件。应用回调
本身也须有界，deadline 在回调返回后检查，不会抢占回调。

## 借用、错误与关闭

CONNECT 字符串、出站 topic／payload 只在同步调用期间借用，不隐式保留或重放。
QoS1 发送成功返回 Packet ID，官方 ACK record 继续占用，后续 PUBACK 事件才确认。
产品保留恢复所需语义数据；clean-session 失败可能留下交付不确定性，业务重试可能重复。

入站 event／topic／payload 仅在回调内有效，有显式长度而非 NUL 终止；后续使用须
自行复制。回调不能重入同一 client 或销毁其存储。另一个独立 client 不共享其活跃
状态或 transport 路由。

非法准入、I/O 前容量耗尽和调用前已过期不会产生线上效果。部分发送、transport
错误／EOF、调用中 timeout、预算耗尽、时钟倒退和协议错误会保留为 session 故障，
后续通信拒绝到 deinit，不能悄悄重试可能已经发送的帧。被拒绝订阅以权限事件报告，
本身不会破坏合法 session；未知 control ACK 按失败关闭。一次成功的显式
clean-session reconnect 清除前一 session 的 pending controls，官方引擎同样重置
自己的 QoS records。产品必须先排空／替换旧底层 stream，adapter 不重连 transport。

DISCONNECT 只表示 MQTT 字节本地接纳，不证明 TLS／TCP 排空。deinit 是无 I/O 的
本地动作，只有全部调用与回调返回后才结束 MQTT 借用。创建另一 session 前，产品
分别关闭并排空底层 stream，也负责机密 buffer 的保护与擦除；本组件不提供凭据
存储或迁移策略。

## 源码与验证入口

`include/nexus/mqtt/client.h` 定义公共合同，`src/client.c` 组合真实官方引擎，
`port/` 和 `CMakeLists.txt` 定义精确 profile 与编译边界。

定向 GoogleTest／GoogleMock 以维护的告警／优化上下文将三个真实官方 C 文件和
adapter 编译为 C11；脚本化 transport bytes 覆盖序列化、QoS 状态、ACK、部分推进
与失败。它们不是实际 broker、MCU 连接、TLS 集成资格或物理测试。最初未支持 stub
RED 与后续六项失败回归保留原始源码、二进制和报告。根 hooks、安装 SDK、发布资格
及物理 HIL 按精确源码和范围分别验收。
