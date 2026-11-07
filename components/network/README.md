# 可选 lwIP 以太网与 TCP 适配器

本目录将官方 lwIP 的固定 profile 接到 Nexus Ethernet frame 接口与产品执行者。
组件管理软件协议栈推进和明确的帧借用，不发现 PHY、不创建 worker、不启动调度器，
也不替产品决定地址、链路恢复或生命周期。

## 来源、构建与接口边界

固定 lwIP 2.2.1 组织 Gitlink 为 `X-Gen-Lab/lwip`，commit
`1f53f34b4aa7c6583540c0f4c35142fa66616c78`，tree
`7e65d6e65cefeffa2be44be0bb0ee51136626f1d`，基于上游 commit
`77dcd25a72509eb83f72b033d219b1d40cd8eb95`。唯一上游 C 变更是
`ext/lwip/NEXUS_CURATED_SOURCE.md` 记录的容量条件例外。源码准入不建立已认证的网络
平台、新软件候选或物理以太网资格。

CMake 定义显式 `Nexus::LwIP` target，只有外部组合选择它才编译／链接进 firmware。
但配置阶段会立即使用 `tools/maintenance/lwip_lock.py` 检查精确干净检出、组织
origin、父 index gitlink，以及 311 个 source／header／license／整理说明文件的
固定闭包。验证实际文件字节，不能靠未改变 Git status 隐藏脏文件；配置不下载依赖，
也不写上游 checkout。配置依赖与实际链接裁剪是两件事，冷态最小依赖缺口见
[问题 #52](https://github.com/X-Gen-Lab/nexus/issues/52)。

安装 Source SDK 先通过已有 manifest 校验，再使用同一固定字节闭包，不伪造 Git
元数据。显式上游实验可设 `NEXUS_LWIP_SOURCE` 与 `NEXUS_LWIP_DEVELOPMENT=ON`，
只形成未取得资格的开发结果，不能替换生产组织 origin。

普通 `Nexus::LwIP` 公共 API 仅使用 Nexus 类型，不传播 vendor include 或 stack
配置宏。静态装配处显式链接 `Nexus::LwIPStorage`，通过 `lwip_storage.h` 分配精确
netif／adapter 布局。这个后端存储头依赖锁定 profile，避免最大 opaque object，
不进入通用 I/O 接口。

## 执行者、协议与资源

固定配置是 `NO_SYS=1`，一个串行 stack owner、一个 netif。执行者可为产品
FreeRTOS task 或协作主循环；全部 raw API、回调、adapter 和 timer 操作都在同一个
不可重入上下文。不创建 OS lock、socket API 或 Netconn API。

参考 profile 提供 Ethernet II、ARP、IPv4、ICMP、UDP、TCP，不启用 DHCP、DNS、
IPv6、VLAN delivery 或 IP fragmentation／reassembly。可选 Nexus TLS／MQTT 使用
上层 byte interface，不选 lwIP 内建 TLS／socket 集成。产品提供 address、mask、
gateway、MAC identity、PHY 观察和重连策略；adapter 不协商链路或建立 Board 电气事实。

复制 RX 配置包含 16 KiB 内部 arena、24 个固定 512 字节 pool payload、四个 UDP
PCB、四个 TCP PCB、两个 listening TCP PCB、32 个 TCP segments、八个 ARP entries
和八个 timer entries，另有固定协议元数据，不能把这些数值当成完整 `.bss`。无 libc
分配或无界存储增长，但 arena 仍是有界的可选栈分配器，不代表 lwIP 内部完全无分配。
产品必须处理 arena／pool 耗尽；完整选定 ELF 才确定资源成本。arena 在 Native64
对齐八字节，在维护的 32 位 MCU ABI 对齐四字节。

## 启动、时间与产品钩子

装配先调用一次 `lwip_init()`，再 attach adapter。栈全局 pools／timers 不属于
逐 adapter 对象，同时第二个 netif 会被拒绝。attach 不初始化或启动硬件；产品先
启动已审 Platform，再提供稳定 frame／clock faces、TX 存储与 RX fragment views。

三个显式产品钩子缺一不可：

- `nx_lwip_port_now_ms()`：在全部 lwIP timers 生命周期提供可回绕单调毫秒，不得
  静默换时间域。
- `nx_lwip_port_random_u32()`：提供已准入随机数据；来源失败进入产品永不返回故障
  路径，没有 timestamp、固定 seed 或 libc 回退。确定性主机夹具不证明 MCU 熵质量。
- `nx_lwip_port_assert()`：永不返回的不变量失败路径；断言不允许回收仍被 DMA 或
  provider 借用的存储。

adapter 的独立 `nx_clock_t` 使用实际 MAC provider deadline 时间域的微秒。
TX timeout 必须有限且非零，时钟耗尽拒绝提交。adapter stop 成功不结束全局 stack
timers；换域或撤销其时钟需要产品完整 stack shutdown／restart，不能只换数值 epoch。

## 帧借用与执行进度

`nx_lwip_adapter_service()` 先推进 MAC 与已到期 lwIP timers，再按明确预算获取
RX。过载不能无限饿死协议 timers。RX budget 限制获取帧数，不限制任意 stack／应用
回调总耗时；按实际到达率、容量与工作负载选择并测量 service 间隔。

```text
MAC RX 借用 → 复制到有界 pbuf → 精确归还 RX → lwIP input
lwIP pbuf   → 复制到调用者 TX buffer → MAC ACCEPTED → 观察 SETTLED
```

adapter 使用一个至少 1514 字节的连续 TX buffer。仍被借用时返回背压，不覆盖
buffer；MAC 不保留 stack pbuf。link output 成功表示接纳，不证明对端交付。provider
后续线上故障是所选协议的 packet loss；TCP 重传与产品恢复继续承担各自职责。

最终 TX 失败保留到 `nx_lwip_adapter_service` 精确报告一次。已接纳传输可能 inline
失败并 SETTLED，原 admission 仍返回 `ERR_OK`；错误观察前，新 link output 返回
`ERR_IF`，不复制或接纳新帧。成功 inline 完成可以立即复用，不额外等待 service。
仍被借用请求以 `ERR_MEM` 背压。service 在 MAC、timer 和有界 RX 推进前后均检查
结果，因此本次推进产生的 inline 失败也会在该次调用报告。

acquire 观察失败 SETTLED 后，service 将已有且无借用 request prepare 回 READY，
不关闭 adapter、重置 MAC、重放字节或因单帧丢失强制 stop／restart。产品按真实
provider 状态决定恢复，持续健康错误仍可独立报告。同次调用另有 MAC／RX 错误时，
已结清 TX 错误优先；不保存聚合错误历史。typed TCP 保留既有策略：BUSY／EXHAUSTED
暂态，其他 network-service 错误使该连接失败。帧错误可观察不保证连接透明恢复。
ACTIVE／QUARANTINED request 不会被结果观察清除，TX 路径不新增错误队列或实例字段。
这是错误可观察性的完善，早期 best-effort 丢包合同没有承诺此通知。

RX fragment views 由调用者按所选 descriptor geometry 提供。容量不足时硬件帧仍
排队；获取后长度／VLAN 非法或 pool 耗尽，精确归还 loan 并报错。归还失败保留
原 token，仅丢弃新软件副本，关闭后续 adapter 进度，不能把同一帧交给 stack，
也不能强制重臂仍被持有的 descriptors。

## 停止、恢复与全局责任

产品先关闭 raw PCBs、应用回调、timers、pending queues 和外部 observers。
`nx_lwip_adapter_stop()` 随后关闭 adapter 准入、将 netif down、重试 held RX release
并请求真实 provider stop。只有 provider 成功且 acquire 观察 TX SETTLED 后才 detach。

provider 错误、BUSY 或未完成 TX 保留 adapter 与全部剩余 buffer／token。调用者可
按 provider 显式恢复，再重试 stop；不自动 reset／replay。成功 detach 幂等，但不能
证明全局 stack 或全部连接已停止。产品资源须存活到其回调与 timers 真正结束。

## 可选复制式 TCP 字节客户端

`Nexus::LwIPTCP` 在一条官方 raw TCP connection 上实现既有只读
`nx_byte_stream_t`。普通消费者包含 `nexus/network/lwip_tcp.h`，获得 Nexus 类型；
静态装配显式链接 `Nexus::LwIPTCPStorage` 分配精确 PCB／pbuf adapter 布局。
不增加 socket registry、worker 或最大 opaque pool，未选择的 firmware 不链接它。

运行调用为：外部串行执行者 → 可选 MQTT → 可选 TLS → 即时复制 byte face → Nexus
raw TCP → 固定 lwIP TCP／pool 与显式网络推进 → Nexus Ethernet frame loans。
[FIG-32](../../docs/assets/diagrams/lwip-tcp-tls-mqtt-host.svg) 展示独立的主机互通参考：
两条 TCP 连接、一条端到端 TLS，不能将测试 relay 当作生产 socket adapter。

调用者提供清零 client、live network adapter、正数 receive-frame budget 和 IPv4
address／port；配置与地址会复制。`start` 成功只接纳 CONNECTING，不证明 ESTABLISHED。
第一次有限 connect deadline 保留，连接故障持续存在；没有隐式 retry、fallback、
DNS 或 reconnect。

同一个串行 NO_SYS 执行者拥有 stack、adapters 和全部 clients；facade data／control
不得从 raw callback 内调用。共享 in-call guard 拒绝不同 facade 实例间嵌套变更，
没有实例 registry 或 active-client pointer。adapter、存储、clock epoch、全局 timers
存活到每条连接结束，包括已 detach 的 FIN／TIME_WAIT。毫秒 timers 与微秒 API
使用同一不变时间域。

每个非零字节尝试可执行一次显式预算的 network service，使 coreMQTT 等同步调用者
无隐藏线程也能接收。先消费 held RX，避免 pbuf pool 耗尽时再依赖分配。timer 回调
耗时由 profile 与 raw users 约束，帧数不是硬墙钟时间上界。

写入使用官方 `tcp_write(..., TCP_WRITE_FLAG_COPY)`。正数部分接纳将字节复制进
现有有界 stack 存储，源可立即复用；这只是队列接纳，产品以协议 ACK 限制交付时间。
复制接纳后观察到致命 output 不能撤销已接纳事实；诊断与后续调用保留失败到显式
abort。would-block／容量不足返回 BUSY，count 为零。

RX 最多持有一个已接纳 pbuf chain 及 partial offset。拒绝另一个 chain 返回 ERR_MEM，
其所有权仍在 lwIP。只有复制给调用者的字节才增加 `tcp_recved` credit。完整消费或
abort 后精确释放一次；error callback 的 PCB 已由库释放，不能再 free。缓冲数据
先于有序 FIN 返回，对端半关闭仍可写；TLS 独立判断 close-notify 认证。

生产者 join 后，`close` 关闭写准入，并等待 held／refused RX 与 queued／unacknowledged
TX，第一次有限 deadline 不能延长。TIMEOUT／BUSY 保留 context。先 detach callback
再调用官方 `tcp_close`，只有错误返回才允许再次触碰 PCB。成功可能只是上游延后
分配 FIN，FIN／TIME_WAIT timers 仍运行，因此只证明本 context 对 stack 的最后访问，
不是全局或 PHY drain。显式 `abort` 丢弃数据、摘除回调并释放仍自有的 PCB／pbuf；
MAC loan 由 network adapter 独立撤回。产品先停全部已知 clients，再排空／移除 adapter。

固定 stack 共享四个 TCP PCB、24 个 512 字节 pool blocks 和显式 arena。一个 held
chain 可占满 receive window，refused／out-of-order 数据也占全局资源。产品须及时
读取和推进；容量拒绝和连接状态不是物理网络或性能资格。

PCB 分配保留官方回收策略：`tcp_new_ip_type` 使用普通 priority，首次分配失败时
`tcp_alloc` 先推进 pending close，再尝试回收 TIME_WAIT、LAST_ACK、CLOSING 和
低优先级 active connections。因此新 client start 期间其他连接可能收到 error
callback，即使新 start 最终拒绝，也不承诺全局 stack 不变。局部拒绝只表示本 context
不保留新的 PCB、pbuf 或配置引用。产品预算共享池并处理每条连接错误，adapter 不建
第二 allocator 或隐藏预留池。四个 CLOSED PCB 的拒绝测试没有可回收对象，不证明
普遍“不回收”规则。

## 可选 RX 负载借用

[选定 RX 借用合同](../../docs/reference/api/components.md#lwip-loaned-rx-contract)
是装配与生命周期权威。选择 `Nexus::LwIPLoanedRX` 或 `Nexus::LwIPTCPLoanedRX`，
仅在静态装配处使用各自 `Storage`。上述既有目标和 24 块描述属于复制 RX；每镜像
只允许一个 singleton stack profile，CMake 拒绝复制／借用混合。

每个调用者 frame slot 提供可写 136 B 复制前缀和 custom-pbuf metadata。只有剩余
只读 `PBUF_ROM` payload tails 引用 MAC 字节，raw 用户写前须 clone。提供非零精确
slots 和至少 `slot_count * base.rx_capacity` 个 pieces。引用操作与 adapter 调用
共享串行 NO_SYS owner。最终 pbuf free 只标记 release-ready，后续 owner service／
stop 才精确归还 MAC。失败关闭准入并保留存储；pbuf 引用未结束时 stop 保持 BUSY。
增强 F407／GD470 provider 的独立 held loans 不表示通用容量或 ACK 保证。

此配置保留两个备用 512 B pool payload、既有 16 KiB arena 和其他不变固定池。
外部 slots、pieces、prefixes、MAC buffers、TX 字节另计；TX 与 ICMP 回复回退仍用
有界 RAM。pool 计数不能证明完整 SRAM 适合。C11 link-only 外部开发探针在 F407／
GD470 使用两 frame slots、四 pieces、2048 B main stack、无 libc heap，通过
32 KiB 预算，实测链接 SRAM 分别为 24344 B／24316 B；探针不提供真实 MAC rings
和 payload buffers。源码／安装 SDK 资格另行建立，不新增 DMA／Cache、PHY、吞吐或
物理 HIL 资格。组织 fork 只对此显式 custom-RX／custom-pbuf profile 豁免原版
`PBUF_POOL`／`TCP_WND` 容量启发式，保留其他全部 sanity checks；不证明原版上游准入。

## 源码入口与验证范围

先读 `include/nexus/network/` 的 adapter／TCP 公共合同，再看 `src/` 的单执行者
实现；`Profiles.cmake` 与 `port/lwipopts.h` 维护固定目标／资源，锁文件校验属于
`tools/maintenance/lwip_lock.py`。

定向 GoogleTest 链接真实锁定的 lwIP C：覆盖 ARP 解析／应答、ICMP 回显、UDP 收发、
TCP SYN、分片复制、RX 释放失败、TX 借用、停止失败后的责任保留、固定池耗尽、
过载时的定时器推进、Native 指针对齐、非法 MAC／provider 身份、
重复 attach 和坏 IPv4。Python 检查 source lock、改字节、错 commit／origin、dirty
source、不安全路径、固定闭包、观察期间修改、production origin／gitlink 拒绝、
Source SDK 范围及 symbolic CMake selector 拒绝。

TCP client 模型另覆盖复制队列准入、接收背压、peer FIN、close／abort、有界推进、
deadline 和共享池回收。[完整主机网络夹具](../../tests/contracts/lwip_mqtt_interop/README.md)
通过测试专用透明密文 relay 将真实 lwIP／TCP／TLS／MQTT client 接到真实 Mosquitto：
一条 TCP 通过两个独立 lwIP 进程的软件 Ethernet ports，另一条通过 relay 至 broker
的非阻塞 host socket。TLS／mTLS 和 MQTT ACK 终止于 client／broker，relay 不解密
也不提供生产 socket adapter。

夹具保留普通与 sanitizer 的独立执行身份。局部 TCP close 仅 detach client context，
FIN／TIME_WAIT 和全局 clock retirement 独立。它 abort 剩余自有 PCB、释放已接纳
pbuf、结清 MAC loans 并 stop adapter 后才结束 service。network／clock 存储持续到
进程退出，由此退休隔离测试 stack 域。MCU 不能从 adapter stop 推导可销毁全局时钟
或重新初始化 lwIP。

以上为各自精确源码下的开发证据。完整软件计划、已安装 SDK、物理 PHY／DMA、性能、
MCU trusted time／entropy 各有独立验收；模型帧和真实主机 broker 不证明 Board 接线、
物理连通或 HIL。
