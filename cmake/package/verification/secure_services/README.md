# 安装 SDK 的安全 API 消费参考

这两个公共场景属于唯一版本化消费计划，不启动 broker、socket、下载或工具安装。
只提供合成公开 CA，不含 client identity 或私钥；信任、时间与 provisioning 归产品。

## Native 场景

Linux 通过 `getrandom(GRND_NONBLOCK)` 取随机字，并使用真实 `CLOCK_MONOTONIC`。
熵不可用／中断在有限 deadline 下返回 BUSY，其他错误 IO，不使用替代随机数。
fixture UTC 不是产品时间权威。流程打开 PSA／runtime／TLS，观察一次有界 handshake
WANT，初始化并析构 MQTT、abort TLS、stop runtime。软件 MAC 保留正常有界 lwIP
池，执行 adapter start／service／stop，但不接纳 TX、不取得 RX、不建立 link。
它不证明认证 TLS 连接或 broker 交互。

## F407 场景

该场景来自独立仓库 ARM service fixture，使用已审启明 F407ZG `hse8-pll168`
和唯一 typed RNG 选择；同一 Platform 生命周期持有 RNG 与时钟依赖。普通应用调用公共 TLS／MQTT，组合层提供精确静态存储及默认
有界 lwIP 池。transport 未连接，UTC 不可信。SDK 检查只编译、链接，核对实际
CPU／ABI／C11、资源、配置身份和存活符号，不在设备上执行 RNG／TLS／网络。
预算为 Flash 512 KiB、static RAM 112 KiB、main stack 8 KiB、libc heap 0，超限失败。

两场景将普通 API caller 与 storage composition 分离。仅 composition 选择
`Nexus::TLSStorage`、`Nexus::CoreMQTTStorage` 和 lwIP 布局，普通调用者不继承厂商
头或 profile 宏。输入、compile row、镜像和 SDK prefix 绑定实际结果；链接不测量
栈峰值、延迟或物理行为。

独立 N09 host fixture 才验证认证 TLS／mTLS 和真实 broker 交换，其源码／结果
身份独立；本安装消费场景不继承该资格。
