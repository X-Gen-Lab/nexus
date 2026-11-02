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
