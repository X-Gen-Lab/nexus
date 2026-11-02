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
DHCP, DNS, IPv6, VLAN delivery, IP fragmentation/reassembly, TLS and MQTT are
not enabled. Address, mask, gateway, MAC identity, PHY observation and reconnect
policy are supplied by the external composition. The adapter does not discover
a PHY, negotiate a link or establish Board electrical facts.

The reference stack has a 16 KiB internal arena, 24 fixed 512-byte pool
payloads, four UDP PCBs, four TCP PCBs, two listening TCP PCBs, 32 TCP segments,
eight ARP
entries and eight timer entries. lwIP additionally uses fixed protocol metadata;
these numbers are not the complete `.bss` size. There is no libc allocation or
unbounded storage growth. The arena is a bounded optional stack allocator, not
an allocation-free guarantee for lwIP internals. Pool/arena exhaustion remains
observable and must be handled by the product. The complete chosen ELF, not a
sum of nominal pool payloads, establishes its resource cost. Arena alignment
is eight bytes on Native64 and four bytes on the maintained 32-bit MCU ABI.

The public adapter API uses Nexus types. Only the composition site explicitly
includes `lwip_storage.h` to allocate the exact `netif` and adapter layout.
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

These are development results. A TCP handshake/data/close matrix, full software
plan, installed SDK consumption, physical PHY/DMA behavior, network performance,
TLS and MQTT qualification remain separate work. Model MAC frames do not prove
Board wiring or physical connectivity.
