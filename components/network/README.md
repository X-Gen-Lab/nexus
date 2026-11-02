# Optional lwIP Ethernet adapter

This optional component integrates lwIP 2.2.1, commit
`77dcd25a72509eb83f72b033d219b1d40cd8eb95`, tree
`260547a417890e813eaac769f4fcbdaec75686b2`, through the reviewed
`X-Gen-Lab/lwip` organization gitlink. Source admission does not establish a
qualified network platform, a new software candidate or physical Ethernet
acceptance.

CMake defines the explicit `Nexus::LwIP` target but does not build or link the
stack into firmware unless the external composition selects that target.
Configuration verifies the exact clean checkout, organization origin, parent
index gitlink and fixed closure of 310 source/header/license files through
`tools/maintenance/lwip_lock.py`. The verifier rechecks observed source bytes;
a dirty file cannot be hidden merely by an unchanged Git status. Configuration
never downloads dependencies or writes to the upstream checkout.

Source SDK consumption uses the same fixed byte closure after the existing SDK
manifest verifier has admitted the installation. It does not invent Git
metadata for an installed payload. Explicit upstream experiments may select
`NEXUS_LWIP_SOURCE` with `NEXUS_LWIP_DEVELOPMENT=ON`; those results remain
unqualified development and cannot replace the production organization origin.

## Execution and resources

The selected profile is `NO_SYS=1`, with one serialized stack owner and one
netif. The owner may be a product FreeRTOS task or a cooperative application
loop. Every raw API call, callback, adapter operation and timer operation uses
that same nonreentrant execution context. No worker, scheduler startup, OS lock,
socket API or Netconn API is created by the component.

The fixed reference profile provides Ethernet II, ARP, IPv4, ICMP, UDP and TCP.
DHCP, DNS, IPv6, VLAN delivery and IP fragmentation/reassembly are not enabled.
Optional Nexus TLS and MQTT components use the byte interface above this profile;
the lwIP built-in TLS/socket integrations are not selected. Address, mask, gateway, MAC identity, PHY observation and reconnect
policy are supplied by the external composition. The adapter does not discover
a PHY, negotiate a link or establish Board electrical facts.

The reference stack has a 16 KiB internal arena, 24 fixed 512-byte pool
payloads, four UDP PCBs, four TCP PCBs, two listening TCP PCBs, 32 TCP segments,
eight ARP entries and eight timer entries. lwIP additionally uses fixed protocol metadata;
these numbers are not the complete `.bss` size. There is no libc allocation or
unbounded storage growth. The arena is a bounded optional stack allocator, not
an allocation-free guarantee for lwIP internals. Pool/arena exhaustion remains
observable and must be handled by the product. The complete chosen ELF, not a
sum of nominal pool payloads, establishes its resource cost. Arena alignment
is eight bytes on Native64 and four bytes on the maintained 32-bit MCU ABI.

The public adapter API uses Nexus types. Generic users link `Nexus::LwIP` and
inherit neither vendor include paths nor the stack configuration macros. Only
the composition site explicitly links `Nexus::LwIPStorage` and includes
`lwip_storage.h` to allocate the exact `netif` and adapter layout.
This backend storage header depends on the pinned lwIP profile. It avoids a
maximum-size opaque object and does not enter the generic I/O interface.

## Startup, clocks and hooks

The composition initializes lwIP once with `lwip_init()` before attaching an
adapter. Stack-global pools and timers are not per-adapter objects; a second
simultaneous netif is rejected. Adapter attachment does not initialize or start
the hardware controller. The product first starts its reviewed platform and
then supplies stable frame and clock faces, TX storage and RX fragment views.

The adapter requires three explicit product hooks:

- `nx_lwip_port_now_ms()` supplies wrapping monotonic milliseconds throughout
  the lifetime of all lwIP timers. It must not silently change domains.
- `nx_lwip_port_random_u32()` supplies admitted random data. Source failure
  enters a nonreturning product fault path; no timestamp, fixed-seed or libc
  fallback is supplied. A deterministic host fixture is not MCU entropy proof.
- `nx_lwip_port_assert()` is a nonreturning invariant failure path. It must not
  reclaim DMA or borrowed storage merely because a stack assertion occurred.

The separate `nx_clock_t` in the adapter configuration supplies microseconds in
the actual MAC provider deadline domain. TX timeout must be finite and nonzero;
clock exhaustion rejects submission. A successful adapter stop does not end
global stack timers. Changing or withdrawing their clock requires the product's
complete stack shutdown/restart procedure, not a replacement numerical epoch.

## Frame ownership and progress

`nx_lwip_adapter_service()` progresses the MAC and due lwIP timers before
acquiring at most its explicit receive budget. RX overload cannot indefinitely
starve protocol timers. The receive budget bounds
acquired frames. It does not bound the total duration of arbitrary stack or
application callbacks. Choose and measure a service interval for the actual
arrival rate, pool capacity and workload.

```text
MAC RX loan -> copy into bounded pbuf storage -> exact RX release -> lwIP input
lwIP pbuf  -> copy into caller TX buffer -> MAC ACCEPTED -> observed SETTLED
```

The adapter supplies one contiguous TX buffer of at least 1514 bytes. A busy
borrow returns lwIP backpressure without overwriting that buffer. Stack pbufs
are not retained by the MAC. A successful link output means admission, not
delivery to a peer; the provider's later wire failure remains packet loss for
the selected protocol. TCP retransmission and product recovery retain their
normal responsibilities.

RX fragment views are caller-owned and sized for the selected descriptor
geometry. Insufficient view capacity leaves the hardware frame queued. After
acquisition, invalid length/VLAN or pool exhaustion releases the exact loan and
returns an error. Release failure retains the token, discards only the new
software copy, closes further adapter progress and never feeds the same frame
into the stack. No held descriptors are forcefully rearmed.

## Shutdown and recovery

The product first closes its raw PCBs, application callbacks, timers, pending
queues and external observers. `nx_lwip_adapter_stop()` then closes adapter
admission, brings its netif down, retries any held RX release and requests the
actual provider stop. It detaches the netif only after provider success and
acquire-observed TX settlement.

Provider failure, BUSY or an unfinished TX preserves the adapter and every
remaining buffer/token. The caller may perform the provider's explicit
recovery, then retry stop. There is no automatic reset or replay. Successful
detachment is idempotent; it does not claim the global lwIP stack or every
connection has shut down. Keep product stack resources live until their own
callbacks and timers have exited.

## Current validation boundary

Focused GoogleTest execution links the actual pinned lwIP C implementation.
It covers ARP parsing/reply, ICMP echo reply, UDP transmit and receive, TCP SYN,
fragment copying, RX release failure, TX borrowing, stop retention, fixed pool
exhaustion, timer progress during overload, Native pointer alignment, invalid
MAC/provider identity, duplicate attachment and corrupted IPv4 input. Python
tests cover
the source lock, changed bytes, wrong commit/origin, dirty source, unsafe paths
fixed closure enforcement, changes during observation, production origin and
gitlink rejection, source-SDK scope and symbolic CMake selector rejection.

TCP client model tests additionally cover copied queue admission, receive
backpressure, peer FIN, close/abort, bounded progress, deadline and shared-pool
reclamation. The separate
[complete host network fixture](../../tests/contracts/lwip_mqtt_interop/README.md)
connects the actual lwIP/TCP/TLS/MQTT client to real Mosquitto through a test-only
transparent ciphertext relay. One TCP connection crosses software Ethernet frame
ports between two independent lwIP processes; the second crosses a nonblocking
host socket from the relay to the broker. TLS/mTLS and MQTT acknowledgements
terminate at the Nexus client and broker. The relay does not decrypt TLS or
provide a production socket adapter.

That host fixture retains separate normal and sanitizer execution identities.
Its local TCP close only detaches the client context; FIN/TIME_WAIT and global
clock retirement are separate. It aborts remaining owned PCBs, releases accepted
pbufs, settles MAC loans and stops its adapter before ending service. Network and
clock storage remain live until process exit, which retires the isolated test
stack domain. An MCU cannot infer permission to destroy its global clock or
reinitialize lwIP merely from adapter stop.

These are development results. Full software-plan and installed-SDK acceptance,
physical PHY/DMA, network performance and MCU trusted-time/entropy qualification
retain their own source-bound evidence. Model frames and a real host broker do
not prove Board wiring, physical connectivity or HIL.


## Optional copied TCP byte client

`Nexus::LwIPTCP` provides the existing readonly `nx_byte_stream_t` contract over
one official raw TCP connection. Ordinary consumers include
`nexus/network/lwip_tcp.h` and receive Nexus types. A static composition unit
explicitly links `Nexus::LwIPTCPStorage` to allocate the exact reviewed PCB/pbuf
adapter layout. The component adds no socket registry, worker or maximum opaque
object pool. Unselected firmware does not link this target.

```mermaid
flowchart LR
    Product[External serialized executor] --> MQTT[Optional MQTT client]
    MQTT --> TLS[Optional TLS client]
    TLS --> Byte[Readonly immediate-copy byte face]
    Byte --> TCP[Nexus raw TCP client]
    TCP --> Stack[Reviewed lwIP TCP and bounded pools]
    TCP --> Network[Explicit budgeted network adapter progress]
    Stack --> Network
    Network --> Frames[Nexus Ethernet frame loans]
```

The caller supplies zeroed client storage, a live network adapter, one positive
receive-frame budget and an IPv4 address/port. Configuration and address are
copied. `start` success accepts CONNECTING; it does not prove ESTABLISHED.
The first finite connect deadline is retained. Connection failure is sticky;
there is no implicit retry, fallback, DNS or reconnection.

One serialized NO_SYS executor owns the stack, adapters and all clients. Call
facade data/control outside raw callbacks. A shared in-call guard prevents nested
mutation across facade instances; it holds no instance registry or active-client
pointer. The network adapter, its storage, clock epoch and global stack timers
outlive every connection, including detached FIN/TIME_WAIT state. Millisecond
stack timers and microsecond API deadlines use the same unchanged time domain.

Each nonzero byte attempt may perform one explicitly budgeted network service
pass so a synchronous caller such as coreMQTT can receive packets without a
hidden thread. Held RX is consumed first, avoiding an allocation dependency while
its pbuf pool is exhausted. Timer callback cost remains subject to the selected
stack profile and its raw users; a frame count is not a hard wall-clock bound.

Writes use official `tcp_write(..., TCP_WRITE_FLAG_COPY)`. Positive partial
acceptance copies bytes into lwIP's existing bounded storage; the source may be
reused immediately. It promises queue admission, not delivery. The caller bounds
delivery using its protocol acknowledgement policy. A fatal output observed
after copied admission cannot undo acceptance; diagnostics and subsequent calls
remain failed until explicit abort. Would-block/capacity produces BUSY/count zero.

RX holds at most one accepted pbuf chain and a partial-read offset. Returning
ERR_MEM for another chain retains lwIP ownership of that refused chain. Only
bytes copied to the caller receive `tcp_recved` credit. The accepted chain is
released exactly once after complete consumption or explicit abort; the error
callback has already freed its PCB and never frees it twice. Buffered data
precedes orderly TCP FIN, and a peer half-close still permits writes. TLS makes
its separate close-notify authentication decision.

After producers join, `close` shuts write admission and waits for held/refused RX
and queued/unacknowledged TX. Its first finite deadline cannot be extended.
Timeout or BUSY retains the context. Callback references detach before official
`tcp_close`; only an error permits touching the PCB again. Success may represent
upstream deferred FIN allocation and still-running FIN/TIME_WAIT timers, so it
proves this context's final stack access rather than global/PHY drain. Explicit
`abort` discards data, detaches callbacks and frees any still-owned PCB/pbuf;
MAC frame loans remain the network adapter's independent withdrawal duty.
Products stop all known clients before draining/removing the adapter.

The fixed stack currently budgets four shared TCP PCBs, 24 shared 512-byte pool
blocks and the explicitly configured stack arena. One held chain can consume a
receive window; refused/out-of-order data also consumes shared stack resources.
Applications must schedule timely reads and progress. Capacity refusal and
connection state do not provide a physical network or performance qualification.

PCB allocation deliberately retains the pinned official reclamation policy.
`tcp_new_ip_type` uses normal priority. If its first pool allocation fails,
`tcp_alloc` first progresses pending closes, then attempts to reclaim TIME_WAIT,
LAST_ACK, CLOSING and lower-priority active connections. Other connections can
therefore receive error callbacks while a new client starts; even a subsequently
rejected start does not promise a globally unchanged stack. Local rejection
means this context retains no new PCB, pbuf or configuration reference. Products
budget the shared pool and handle every connection's error; this adapter adds no
second allocator or hidden reservation pool. The four-CLOSED-PCB rejection test
has no reclaimable victim and does not establish a universal no-reclaim rule.
