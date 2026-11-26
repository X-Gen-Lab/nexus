# Native 完整网络客户端链路

本 Linux opt-in 夹具把锁定 Ethernet／lwIP adapter、TCP byte stream、TLS client、
coreMQTT 一起接到 [POSIX 夹具](../mqtt_tls_interop/README.md)的不可变 Mosquitto。
生产组件不变，结果保持未资格化。

```text
客户端进程                        relay 进程                 真实 broker
coreMQTT
  Nexus TLS1.2 ------------------------------------------> Mosquitto TLS
    TCP → lwIP → Ethernet IPC → lwIP raw TCP → OS socket → MQTT engine
```

AF_UNIX datagram 传完整 Ethernet frame，不传应用消息；每进程只初始化一次自己的
lwIP 全局域。relay 在官方 raw TCP 与 nonblocking loopback socket 间复制不透明 TLS
密文，不解析 MQTT、不终止 TLS、不合成 reply、不加生产 socket API。
`server.test`、trust、client cert、MQTT ACK 由真实 broker 终止。测试连接两条TCP，
不是 packet routing／单条 end-to-end TCP，无 TAP、network-admin capability 或
新增固件 stack。

## 构建、场景与资源

Native `nexus_network_tcp_stream_test` 嵌套依赖 helper
`nexus_lwip_mqtt_interop_client`，helper 不是注册 Google 用例或聚合入口。普通 hook
只构建，不启动 Docker。显式另跑 broker：

```sh
python tools/dev/dev.py build --preset native-debug \
    --target nexus_network_tcp_stream_test
python -B -m tools.testing.mqtt_tls_interop \
    --transport lwip-test-relay \
    --client /absolute/path/to/nexus_lwip_mqtt_interop_client \
    --output-root /absolute/path/to/new-output-directory
```

复用精确缓存 broker image、proxy／CA、凭据生成、owned cleanup。默认 posix 保留
3例，本模式5例：

| 用例 | 真实检查 |
| --- | --- |
| `clean` | mTLS1.2 ECDHE-ECDSA／AES-GCM，CONNECT／SUBACK／QoS1 echo／PUBACK／UNSUBACK／DISCONNECT，发送并认证双方 close alert 后 detach TCP |
| `backpressure` | 同流程，relay17 B chunk、初始40 ms pause，正 pause counter 与完整 ACK；不冒充完整 TCP receive-window 测试，后者另测 |
| `fault` | session 建立后官方 tcp_abort 发 RST，MQTT sticky IO，后续零 provider call／无 replay |
| `deadline` | SUBACK 后停转发，新150 ms absolute deadline 到期，MQTT 保持失败；待发数据归 client 到显式 abort |
| `missing-client` | broker 以真实 TLS fatal alert 拒绝，不能用 timeout／entropy 错误冒充 |

每 owner loop 最多4帧。relay 持一条 accepted pbuf chain／offset，其他 callback
返回 ERR_MEM 不消费 borrow；OS 接收后才 tcp_recved，chain 恰 free 一次。反向
用113 B buffer（节流17 B），官方 tcp_write(COPY)，保留未接纳 tail。每loop至多
一 send、一 receive、一 copied submission。socket请求4096 B，Linux 可加倍，
IPC 与真实 lwIP pool 都有界且原profile不变。

每进程唯一 executor，真实 host monotonic elapsed 同时供各 deadline／lwIP millis，
不混用独立 virtual-time cases。operation5秒，MQTT call与poll预算有限，runner
另有20秒case watchdog。host UTC／getrandom 不证明 MCU 时钟或熵；TLS仍client-only。

## 撤回与证据

TCP close 只证明此context／callback detach，不证明 FIN／TIME_WAIT／全局 timer
退休。fixture abort 剩余 PCB，free其pbuf，settle MAC，stop adapter，再join relay。
`client_loans_clear`／`loans_clear` 只覆盖本fixture MAC引用，不是全局lwIP shutdown；
netif移除可abort此local address active／bound PCB，不排空全部timer。

adapter撤回后不再service／timeout；clock／network存储保留至进程退出，relay用
`_exit` 避免先unwind owner。进程结束退休隔离stack域，不授权MCU关闭全局clock、
重初始化lwIP或回收global state。

runner绑定binary、image／broker、credential hash、日志、原始命令、MQTT结果、
Ethernet／ciphertext counter、双方withdrawal，exit0不够。source／compile manifest
绑定每次执行，普通和ASan／UBSan独立fresh。首GREEN是既有组件集成，不伪造TDD；
工具变化另有真实RED／GREEN。不授予SDK／release／PHY／Board／时序／功耗／熵资格。

## 有界原始以太网抓包

每个完整链路用例独占 `evidence/cases/<case>/` 下新建的 0700 目录。
`client.pcap` 与 `relay.pcap` 保存两个方向实际成功送入 AF_UNIX 数据报通道的
完整以太网字节。测试 hook 在 TX 发布 SETTLED 前立即复制，不保留请求或缓冲区；
尚未发送、EAGAIN、失败和故意丢弃的发送不记入抓包。抓包仅覆盖两个 lwIP 之间的
IPC 跳，不包含 relay 到 broker 的 loopback TCP，也不代表真实 PHY／DMA 流量。

格式为小端 PCAP 2.4、Ethernet link type 1、不含 FCS，单帧上限 1,514 字节。
时间戳来自 fork 前共享起点后的主机 CLOCK_MONOTONIC 微秒，不是 UTC 或硬件
收发时刻；查看器可能把零点显示为 1970 年 1 月。每个文件最多 1,024 帧、含头部
1 MiB。独占创建和逐级禁止符号链接的目录访问会拒绝别名与覆盖。超限、短写、时钟
无效或最终同步／关闭失败均使本用例失败，不会把截断前缀标为完整证据。本工装只在
主机测试代码中存在，不增加固件 trace API、worker、注册表或 MCU 实例状态。

既有 runner 将真正解析的文件字节、SHA256、大小、方向和精确帧数绑定到本用例的
完成收据、原始 stdout 与实际执行文件身份。测试源码在执行前后观察并核对；二进制
对应的编译／源码闭包由独立构建证据建立，不能仅凭现有源码的散列推断。

有界读取器检查 PCAP 完整 EOF、Ethernet／IPv4／TCP 结构、唯一 IPC 连接、重复
发送字节一致性和 TLS record 边界。clean 与 backpressure 要求两个方向都有完整
application record；协议失败用例允许明确报告最后一个 TLS record 未完成，但
每个以太网抓包记录仍必须完整。application record 内不得出现已知测试 MQTT
主题或载荷明文。该检查不解密 TLS，也不证明密码保密性；认证和 MQTT 结果仍来自
真实客户端与 broker。不生成 TLS key log。抓包不建立实板、吞吐、线上时序、功耗
或长负载资格。
