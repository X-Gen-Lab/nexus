# 有界 TLS 客户端

本目录提供显式选择的 TLS 客户端，连接产品执行者、只读字节流端口，以及已锁定的
官方 Mbed TLS／TF-PSA-Crypto 实现。产品拥有网络推进、可信时间、熵来源、信任根和
恢复策略。组件不创建工作线程、不发现 socket，不提供默认信任、操作系统熵回退或
逐连接重置分配器。

## 架构与目标

无参数 `nexus_add_tls()` 根据组织的精确 Mbed TLS 4.1.1／TF-PSA-Crypto source lock
创建可选 `Nexus::TLS`。仅定义 helper 不会配置或链接厂商库；Native Google 合同和
选择该组件的产品显式调用它。当前只有一个固定 profile 和一个串行 PSA 所有者。

产品执行者调用 Nexus TLS 客户端；客户端通过 `nx_byte_stream_t` 读写传输，官方
Mbed TLS／PSA CTR DRBG 使用调用者 arena 和显式熵端口。可信 UTC 与单调 deadline
时钟是独立端口。可参照 [FIG-32 主机互通图](../../docs/assets/diagrams/lwip-tcp-tls-mqtt-host.svg)
理解 TLS 在 TCP 与 MQTT 之间的位置；该图是两进程主机参考，不代表 MCU 网络资格。

公共 `tls.h` 只使用 Nexus 类型。普通消费者链接 `Nexus::TLS`，不继承厂商 include
或配置宏；静态装配编译单元显式链接 `Nexus::TLSStorage`，通过 `storage.h` 获得精确
后端布局和 profile。将这个选择限定在装配处，应用逻辑继续使用公共 API。

## 存储与所有权

调用者提供一个清零 runtime、一个清零 session、一个对齐的 65536 字节 arena 和
一个 512 字节 TX stage。传输／lwIP 内存和栈另计：64 KiB 是分配器容量，不是整个
子系统的 RAM 预算。不能从使用显式 arena 推导库内部完全不做子分配。

产品提供 DER 信任材料、hostname 及可选的匹配客户端证书／密钥。初始化将凭据与
hostname 复制进 arena。runtime、transport、其不可变方法及不透明 context、时钟
和 TX stage 存活到 session abort 完成。不透明回调状态不得与 runtime、session、
arena、stage 重叠；组件检查已知跨度，不能推断不透明 context 的大小或内部引用。

## 初始化、推进与字节合同

1. 启动 runtime 一次，初始化 session。
2. 在第一次有限 deadline 下反复调用 `handshake_step`。
3. 握手成功后才接纳应用字节。
4. 调用者显式调用 `service` 推进稳定 stage，并安排后续读取与关闭。

`try_write` 立即复制最多 512 字节。成功表示本地接纳，不是对端交付；调用者可以
立即复用输入。`service` 排空稳定 stage；WANT 重试保留相同后端指针与长度。
较短的新 read／write 不会消耗旧 stage 较晚的 deadline，而是无 I/O 返回 BUSY，
由 owner 显式推进已接纳写入。自动推进使用写入与关闭的有效最小 deadline；旧写入
排空后，新写入准入重新检查自己的 deadline。同一 MQTT 操作内的调用可以共用一个
有限 deadline 并继续部分推进。

超时／失败保留存储，要求显式 abort。读操作不保留调用者输出。经过认证的
close-notify 表示 EOF；传输层直接 EOF 是持久截断错误。`close_step` 排空已接纳
数据并发送本地 alert；传输关闭和对端应用确认仍属于外部责任。

## 停止与失败责任

先会合所有接口使用者，再 abort session，最后停止 runtime。正在执行 BIO、熵或
时钟回调时 abort 返回 BUSY，不释放任何存储；外层执行者在回调返回后重试。
abort 不重置 PSA。runtime stop 拒绝活跃 session，释放 PSA 后擦除整个 arena。
同一责任域中其他代码不得使用 PSA，或替换全局 Mbed TLS allocator／time hooks。

profile 禁用默认标准库平台函数：没有显式 arena 所有权时分配失败，文本格式化
钩子也没有隐式 stdio 回退。分配器不变量失败进入既有 Nexus fault observer；其返回
后仍停驻，不调用 libc `exit`，也不释放所持内存。产品可按 Core 合同提供另行审核的
永不返回故障动作。

## 时钟、熵与精确能力

单调绝对 deadline 限制准入、重试和熵等待，不能抢占同步密码运算。调用者安排进度，
并保证时钟在全部回调内持续属于同一时间域。可信 UTC 不明、无效或读取失败会关闭
准入。握手使用一致 UTC 快照，在日期回调和库步骤结束后复查有效性；第一次握手会
在有限熵预算内通过公共 API 重新播种 PSA。

官方毫秒时间端口读取同一个自有单调时钟。证书 UTC 转换使用所选 libc 的可重入
`gmtime_r` 与调用者存储；证书日期为 1970..9999，且须适合所选 `time_t`。
不选择厂商系统时钟或会分配存储的 `gmtime` 回退。已审核 ARM newlib `gmtime_r`
没有 heap 依赖。

熵端口提供原始、通过质量准入的源字节，不是 DRBG。固定 profile 先使用官方
entropy-driver API，再经库的调理和 CTR DRBG。runtime 拒绝默认／未知质量；
`FULL_SOURCE` 明确声明该上游 API 所需的完整熵额度。声明和软件健康检查都不能
证明物理 min-entropy；产品必须审核来源与调理，主机夹具不认证 F407 RNG 或其他硬件。

当前范围固定为 TLS 1.2 client、ECDHE-ECDSA P-256、AES-128-GCM／SHA-256、DER
凭据、强制 peer verification，以及带最大分片协商的 1024 字节 records。不提供
server 产品 API、TLS 1.3、renegotiation、session resumption、多 session 并发、
PEM／文件加载、证书策略或吊销服务；对端与证书链必须适合此 profile，不能当作通用
互联网 TLS 配置。实际预处理配置禁用 PQCP／MLDSA、内建平台熵和 external-raw-RNG
绕过。未使用官方 lwIP `altcp_mbedtls`，因为其旧私有头假设不兼容 Mbed TLS 4.1。

## 源码与验证入口

先读 `include/nexus/tls/tls.h` 的公共生命周期和失败合同，再看 `src/tls.c`；
`CMakeLists.txt`、`profile/` 和显式 Storage 头拥有具体后端选择。

隔离的 Native 证据覆盖与第二进程的真实 mutual TLS、partial BIO／WANT、复制
write／echo、close-notify、坏 hostname、错误根、过期证书、可信时间丢失、熵失败、
有限 deadline、截断、输出重叠拒绝、回调会合和顺序 session 复用。它建立精确源码与
profile 的主机行为；MCU 内存／链接、物理熵、PHY／HIL、吞吐与功耗仍需各自验收。
