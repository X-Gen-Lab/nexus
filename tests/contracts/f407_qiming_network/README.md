# Qiming F407 network software reference

This five-file opt-in contract fixture fixes IT-N01 to STM32F407ZGT6,
Qiming v3.1 reviewed source wiring, HSE 8 MHz / PLL168, baremetal and a TI
DP83848 at Clause22 address 1. It uses the real public factory Ethernet-frame
and MDIO faces, the actual DP83848 component, copied-RX lwIP TCP, Nexus TLS and
coreMQTT. It reuses the existing public secure-services application exchange;
there is no separate product repository or hidden worker. Source wiring review,
software Board admission, ARM linking and physical qualification are separate.

The production Qiming Board still has no Ethernet binding. This explicit Board
package admits only the reviewed `rmii-frames-a-c-g` AF11 route: PA1/2/7,
PC1/4/5 and PG11/13/14. The reviewed manual printed31/physical33 and BSP blob
`a35e981e8a549e1dbaf84bb4522b2f150bc5d51c` describe an external 50 MHz RMII
oscillator and P5/WiFi jumpers. Actual PCB revision, straps/jumpers, clocks,
magnetics and electrical behavior remain unqualified. No GPIO reset is invented;
only the bounded BMCR soft reset is called. This route does not admit VE100,
Sky or Liangshan Boards.

The exact slice is static IPv4 `192.0.2.2/24`, gateway/peer `192.0.2.1:8883`,
MTU 1500, locally administered MAC `02:00:00:00:00:02`, one client TCP/TLS/MQTT
connection, one serialized NO_SYS owner and no tasks, queues or scheduler.
The addresses are documentation fixtures and require explicit station setup.
The factory supplies two TX and two RX descriptors with 1536-byte RX storage
each. Caller composition supplies one aligned 1514-byte TX staging buffer and
two fragment views. This selects the ordinary copied-RX stack with 24 fixed
512-byte payload blocks, a 16 KiB arena and its unchanged protocol pools; it is
not the separate selected 2-block loaned-RX profile. TLS adds one 64 KiB arena,
one 512-byte stage and exact runtime/session storage. MQTT adds a 1024-byte
network buffer and two entries each for incoming/outgoing ACKs and controls.
The assembly reserves an 8 KiB MSP, permits at most 120 KiB total reserved SRAM
(including MSP) and 512 KiB Flash, and supplies zero libc heap. Actual ELF sections establish cost;
nominal buffer sums do not. Stack peak, throughput and latency are unmeasured.

Start probes the real PHY identity, performs observed BMCR reset, advertises
10/100 half/full autonegotiation, then waits at most five seconds for actual
link truth before applying it to MAC/netif. Each owner pass acquires at most two
frames. There is one finite TCP connect and the reused bounded TLS/MQTT exchange;
there is no reconnect/replay or continuing PHY renegotiation policy. All clock,
MAC, MDIO, adapter, TCP, TLS, MQTT and entropy deadlines use the same unchanged
Core monotonic microsecond domain; wrapping lwIP milliseconds derive from it.

The explicit F407 RNG instance is started with the exact 8/168 MHz facts and
feeds both lwIP randomness and TLS. There is no fixed, timestamp or libc fallback.
`FULL_SOURCE` is a software assumption, not physical min-entropy qualification.
UTC deliberately returns `PERMISSION`, so certificate admission fails closed;
this ARM reference cannot authenticate a server or firmware. A product must
supply its own trusted UTC and identity. The public CA is a test trust fixture;
no client private key is embedded. DHCP, DNS, IPv6, fragmentation/reassembly,
server sockets, RTOS, multiple connections, VLAN protocol delivery, PTP use,
checksum offload, jumbo frames and production trust/recovery policy are excluded.

Accepted resources remain static. TLS/MQTT join precedes TCP abort, then adapter
stop. Any incomplete join/withdrawal parks with storage and outer resources
retained. After `lwip_init`, the fixture makes no stack calls after adapter
withdrawal and preserves the Core clock/RNG/platform and global stack storage
until reset, since netif removal is not global timer retirement. Pre-stack failures
stop RNG before releasing platform resources. Software linking never executes
these physical paths.

Build separately with the prepared locked ARM tools, using one build job:

```sh
cmake -S tests/contracts/f407_qiming_network -B /absolute/new/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/nexus/cmake/toolchains/arm-gcc.cmake
cmake --build /absolute/new/build --parallel 1
```

`NEXUS_REFERENCE_ROOT` optionally selects the reviewed source root when this
fixture is staged separately. Actual C11/CPU/ABI/options, map, ELF/BIN hashes,
heap symbols, resolved input and resource receipts accompany each observation.
The default typed `Os` policy does not enable LTO merely because CMake IPO is ON.
A true LTO experiment must separately prove actual `-flto` compile/link flags.

This image is independent of the fixed MCUboot reference. Its 128 KiB slot
allows only 129536 payload bytes after the 512-byte prefix and 1024-byte suffix;
check the newly measured footprint before combining them. This fixture does not
change that fixed layout or establish boot authentication/handoff compatibility.

IT-N09 host protocol/PCAP evidence comes from the maintained
[`lwip_mqtt_interop`](../lwip_mqtt_interop/README.md) fixture, whose real broker
and two-TCP ciphertext relay are explicitly distinct from this ARM composition.
It supplies real host authentication, MQTT ACK/fault/deadline observations and
bounded Ethernet captures; it does not execute this Board/PHY/RNG or qualify
physical DMA, loss, throughput, long load, timing, power or EMC. All physical HIL
is `not_executed`. A first green build validates this new fixture of existing
behavior; it is not reconstructed historical TDD evidence.
