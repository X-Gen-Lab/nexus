# Complete Native client network chain

This opt-in Linux integration runs the actual pinned Nexus Ethernet/lwIP adapter,
TCP byte stream, TLS client and coreMQTT client together against the immutable
Mosquitto broker used by the [POSIX integration](../mqtt_tls_interop/README.md).
It is an unqualified host fixture. Production components are unchanged.

```text
Client process                          Relay process             Real broker
coreMQTT
  Nexus TLS 1.2 -------------------------------------------------> Mosquitto TLS
    Nexus TCP -> lwIP -> Ethernet IPC -> lwIP raw TCP -> OS socket -> MQTT engine
```

The AF_UNIX datagram channel contains complete Ethernet frames, not application
messages. Each process initializes its own real lwIP global state once. The relay
copies opaque TLS ciphertext between an official raw TCP connection and a
nonblocking loopback socket. It does not parse MQTT, terminate TLS, generate
protocol replies or add a production socket abstraction. TLS authentication,
hostname `server.test`, trust validation, client certificate checks and MQTT
ACKs terminate at the actual broker. There are **two TCP connections** joined
by this test relay; this is not packet routing or one end-to-end TCP connection.
No TAP, network-administration capability or additional firmware stack is used.

## Execution and bounds

The Native Google TCP-stream target has a nested build dependency on
`nexus_lwip_mqtt_interop_client`. The helper is not a registered Google test or
an aggregate entry. Ordinary hooks compile it without starting Docker. Run the
broker suite separately after building that maintained target:

```sh
python tools/dev/dev.py build --preset native-debug \
    --target nexus_network_tcp_stream_test
python -B -m tools.testing.mqtt_tls_interop \
    --transport lwip-test-relay \
    --client /absolute/path/to/nexus_lwip_mqtt_interop_client \
    --output-root /absolute/path/to/new-output-directory
```

Use the existing runner's exact cached broker image, proxy/CA configuration,
fixture generation and owned-container cleanup. Its default `posix` selection
keeps the original three socket cases. The new selection runs five real cases:

- `clean`: mutual TLS 1.2 ECDHE-ECDSA/AES-GCM, MQTT CONNECT, SUBACK, QoS1
  publish/self-echo/PUBACK, UNSUBACK and DISCONNECT. The client sends its TLS
  close alert, authenticates the broker's alert, then detaches its TCP context.
- `backpressure`: the same sequence through 17-byte relay chunks and an initial
  40 ms pause. Positive pause counters and complete ACKs are required. This
  establishes progress under fragmentation/throttling; it does not claim a full
  TCP receive-window test. The separate TCP-stream cases test that condition.
- `fault`: a real official `tcp_abort` in the relay sends RST after the session
  is established. MQTT must latch I/O failure and reject subsequent operations
  without further provider calls or automatic replay.
- `deadline`: the relay stops forwarding after SUBACK. A new 150 ms absolute
  deadline must expire; MQTT remains failed and rejects replay. Pending TCP
  data still belongs to the client until explicit abort.
- `missing-client`: the actual broker must reject the omitted client identity
  with its TLS fatal alert, rather than an unrelated timeout or entropy error.

Every owner loop services at most four Ethernet frames per invocation. The relay
retains one accepted pbuf chain with an offset; another callback returns ERR_MEM
without consuming the stack's borrow. It credits `tcp_recved` only after the OS
socket accepts those bytes and frees the chain exactly once. The reverse path
has one 113-byte buffer (17-byte chunks in the throttle case), uses official
`tcp_write(COPY)` and preserves an unaccepted tail. Each loop performs at most
one send, one receive and one copied TCP submission. Socket buffers are requested
explicitly at 4096 bytes; Linux may double them. Ethernet IPC buffers are bounded
and all production lwIP pools use the unchanged fixed profile.

Each process has one serialized executor. The host monotonic elapsed clock
supplies adapter/TCP/TLS/MQTT deadlines and lwIP milliseconds. It advances in real
time; the existing virtual-time TCP cases remain separate. Protocol operations
have five-second absolute deadlines, finite MQTT I/O-call budgets and bounded
poll waits. The runner has an independent 20-second case watchdog. Host UTC and
Linux `getrandom` are explicit fixtures, not MCU trusted-time or entropy claims.
The production TLS profile remains client-only; no server profile is enabled.

## Withdrawal and evidence

A successful TCP close means this context and its callbacks are detached. FIN,
TIME_WAIT and global timer retirement are not established by that return. The
fixture aborts remaining owned PCBs, releases its accepted pbuf chain, settles
MAC requests and calls adapter stop before joining the relay process. The
`client_loans_clear` and `loans_clear` observations cover this fixture's MAC
TX/RX references, not global lwIP shutdown. Official netif removal can itself
abort active/bound PCBs for that local address and does not drain all timers.

No stack timeout/service call runs after adapter detachment. Network and clock
storage remain alive until process exit; the relay uses `_exit` before owner
storage is unwound. Process termination retires the isolated stack domain. This
does not establish that an MCU may destroy its global clock, reinitialize lwIP
or reclaim global state after stopping one adapter.

The runner retains binary identity, immutable image/broker identity, test
credential hashes, broker logs, raw command receipts, explicit MQTT results,
Ethernet/ciphertext counters and both withdrawals. Exit zero alone is refused.
Source/compile-context manifests bind a particular execution; normal and
ASan/UBSan are separate fresh executions. Any first successful run is integration
validation of existing components, not invented TDD evidence. Tool contract
changes retain genuine RED/GREEN regressions. This suite grants no Source SDK,
release, PHY, board, timing, power or hardware-entropy qualification.


## Bounded raw Ethernet captures

Each full-chain case owns a new mode-0700 directory at
`evidence/cases/<case>/`. `client.pcap` and `relay.pcap` contain the exact complete
Ethernet bytes accepted by each successful AF_UNIX datagram send. The test hook
copies them before TX publishes SETTLED and retains no request or payload span.
Offered, EAGAIN, failed and deliberately discarded sends are not captured. These
files cover the lwIP-to-lwIP IPC hop; they do not capture the relay-to-broker
loopback TCP connection or physical PHY/DMA traffic.

Files use ordinary little-endian PCAP 2.4, Ethernet link type 1, no FCS and a
1,514-byte snapshot limit. Record timestamps are elapsed host CLOCK_MONOTONIC
microseconds from the case's shared pre-fork origin, not UTC or hardware packet
timestamps. A reader may display their zero epoch as January 1970. Each file is
bounded to 1,024 frames and 1 MiB including headers. Exclusive file creation and
no-follow directory traversal reject aliases and overwrites. Exhaustion, partial
writes, invalid clocks or failed final sync/close latch a failed case; a truncated
prefix never becomes complete evidence. This host-only fixture adds no firmware
trace API, worker, registry, source configuration or selected MCU state.

The existing runner binds each capture's retained parsed bytes, SHA256, size,
direction and exact frame count to the completed case command receipt, raw stdout
identity and actual client executable hash. It retains observations of the test
source files before/after execution; actual compiler/source closure belongs to
the separate build evidence and is not inferred from a live source hash.

The bounded reader checks complete PCAP EOF, Ethernet/IPv4/TCP structure, the
reviewed single IPC connection, consistent retransmitted bytes and TLS record
framing. Clean and backpressure cases require complete application records in
both directions. Failed protocol cases may explicitly report a partial trailing
TLS record while every captured Ethernet record must remain complete. Known
fixture MQTT topic/payload bytes must not appear in application-record bodies.
This check does not decrypt TLS or prove cryptographic secrecy; authentication
and MQTT outcomes remain the real broker/client observations. No TLS key log is
created. Captures contain host test traffic and establish no board, throughput,
wire timing, power or long-load qualification.

### 中文

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
