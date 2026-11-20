---
id: reference.api.components.protocols.network
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.network)=

# Byte transport, TCP, TLS and MQTT

The selected byte transport and protocol profile have explicit owners and
storage. Ordinary consumers use opaque Nexus faces; exact supplier-backed
storage headers belong only to the static composition translation unit.

`byte_stream.h` defines immediate partial reads and copied writes with finite
absolute deadlines. SUCCESS reports local byte acceptance, not peer delivery;
BUSY with zero bytes requires explicit owner progress. No caller buffer remains
borrowed after a stream operation returns. The optional `Nexus::LwIPTCP` client
implements this face over the pinned NO_SYS IPv4 stack. Products still own the
executor, peer selection, trusted time, entropy and shutdown/reconnect policy.

| Consumer | Selected target and header | Boundary |
| --- | --- | --- |
| Generic Ethernet adapter caller | `Nexus::LwIP`, `nexus/network/lwip_adapter.h` | Nexus types; no supplier headers or profile macros |
| Generic TCP byte caller | `Nexus::LwIPTCP`, `nexus/network/lwip_tcp.h` | Opaque client and the existing readonly byte face |
| Static netif composition | `Nexus::LwIPStorage`, `nexus/network/lwip_storage.h` | Exact pinned netif/adapter layout, composition only |
| Static TCP composition | `Nexus::LwIPTCPStorage`, `nexus/network/lwip_tcp_storage.h` | Exact pinned PCB/pbuf client layout, composition only |

The composition initializes lwIP once and supplies a live network adapter,
caller-owned storage and a positive receive-frame budget. All raw stack APIs,
callbacks, network service and clients share one serialized NO_SYS executor.
Call facade data/control outside raw callbacks; nested cross-facade mutation is
rejected with BUSY. No socket layer, worker, scheduler, lock, name registry or
maximum opaque pool is created. A successful TCP start admits CONNECTING; it
does not establish the connection. Its first finite connect deadline cannot be
extended. The existing byte face is a stateful handle, not a readiness proof.

TCP writes use official copied admission: a positive partial count releases the
caller input immediately, while queued data remains bounded stack storage. Later
output failure cannot undo that acceptance. Diagnostics continue to report that
failure, and subsequent data/service calls fail until explicit abort. RX holds
at most one accepted pbuf chain; refused chains stay lwIP-owned. Only consumed
bytes receive credit. Buffered bytes precede orderly FIN, and peer half-close
still permits writes. A frame budget bounds acquisition, not the total wall time
of arbitrary raw callbacks. Each nonzero byte attempt may perform one explicitly
budgeted network pass.

Official PCB allocation shares the bounded stack pool and its reclamation
policy. It may progress pending closes or reclaim TIME_WAIT, LAST_ACK, CLOSING
and lower-priority active connections, invoking other clients' error callbacks
even if the new start later fails. Local rejection retains no new references for
that client; it does not promise a globally unchanged stack.

After producers join, the owner consumes buffered RX and explicitly progresses
and retries close on BUSY under the first finite close deadline. That deadline
cannot be extended. Timeout requires explicit abort while storage stays live;
abort discards local data and detaches callbacks. Successful close proves this
client context's final stack access. The official stack may still own deferred
FIN and TIME_WAIT state. Preserve the network adapter, global pools, timers and
unchanged clock domain until the product's complete stack shutdown/restart
procedure permits withdrawal. Client detachment does not prove global stack
shutdown or MAC/PHY drain.

Ordinary TLS/MQTT consumers include `tls.h` or `client.h` and link `Nexus::TLS`
or `Nexus::CoreMQTT`. Their public session types are opaque and expose no
supplier headers or profile macros. Only the static composition translation unit
selects `Nexus::TLSStorage` with `nexus/tls/storage.h`, or
`Nexus::CoreMQTTStorage` with `nexus/mqtt/core_mqtt_storage.h`, to allocate the
exact pinned backend layout. Keep explicit storage targets local to composition;
fields are implementation state, not business control APIs or binary ABI
promises. Products explicitly call `nexus_add_tls()` or `nexus_add_coremqtt()`
to create the selected targets.

The TLS profile is one serialized TLS 1.2 client/PSA domain using ECDHE-ECDSA
P-256 and AES-128-GCM/SHA-256. It requires explicit DER trust, trusted UTC and a
reviewed entropy-quality assertion. Its 64 KiB caller arena and 512 B TX stage
exclude session metadata, transport memory and stack. Join facade users, abort
the session, then stop the runtime; failed or BUSY cleanup retains its
resources. The MQTT 3.1.1 profile provides clean sessions and QoS0/1; callbacks
borrow incoming views only until return. Explicit DISCONNECT and TLS close
report local progress and do not replace lower-layer drain or peer
acknowledgement. The component contracts detail
[network](../../../../../components/protocols/network/README.md),
[TLS](../../../../../components/protocols/tls/README.md) and
[MQTT](../../../../../components/protocols/mqtt/README.md) ownership and fixed
profile limits.

For both Ethernet adapter profiles, successful link output reports frame
admission. A failed SETTLED TX outcome remains observable once through
`nx_lwip_adapter_service`; until that observation, another link output cannot
overwrite or reuse the TX buffer. Observing the error clears only the settled,
unborrowed request. It adds no error queue or instance storage and does not
reset the MAC, replay a packet or choose a product recovery policy. An ACTIVE or
QUARANTINED borrow remains retained. The TCP byte facade keeps its existing
connection-failure policy; frame-error observation does not promise recovery.

## Selected lwIP RX payload loans

Select one singleton NO_SYS stack profile per image. The default `Nexus::LwIP`
and `Nexus::LwIPTCP` profiles retain copied RX and their existing storage
layouts. The explicit targets below select RX payload loans; CMake rejects an
image combining copied and loaned profiles, including a copied TCP target
combined with loaned adapter storage. These targets create no second stack,
netif, executor, worker or allocator.

| Consumer | Selected target and header | Boundary |
| --- | --- | --- |
| Loaned RX adapter caller | `Nexus::LwIPLoanedRX`, `nexus/network/lwip_loaned_rx.h` | Opaque adapter; explicit loaned start |
| Loaned RX TCP byte caller | `Nexus::LwIPTCPLoanedRX`, `nexus/network/lwip_tcp.h` | Existing copied byte-stream contract |
| Static loaned RX composition | `Nexus::LwIPLoanedRXStorage`, `nexus/network/lwip_loaned_rx_storage.h` | Exact adapter, frame-slot and custom-pbuf storage |
| Static TCP composition over loaned RX | `Nexus::LwIPTCPLoanedRXStorage`, `nexus/network/lwip_tcp_storage.h` | Exact selected TCP layout; also provide loaned adapter storage |

The owner initializes lwIP once and attaches with
`nx_lwip_loaned_adapter_start`. Supply zero-initialized, disjoint adapter, TX,
fragment-view, frame-slot and custom-pbuf-piece storage. Select a nonzero
`slot_count` and at least `slot_count * base.rx_capacity` pieces, with fragment
capacity sized for the actual provider geometry. MAC payload spans must also be
disjoint from adapter, TX/view and slot/piece storage. Invalid acquired spans
are released without delivery. All raw APIs, reference operations, callbacks,
service and stop share the same serialized, nonreentrant task owner. Live or
reentrant attachment cannot erase retained references or tokens.

```text
Nexus::LwIPLoanedRXStorage -> adapter + caller slots/pieces
MAC exact RX loan -> copied REF header[136] -> immutable ROM tails
final pbuf_free -> slot ready -> owner service/stop -> exact MAC release
```

Each frame slot copies at most 136 B into its own writable `PBUF_REF` prefix.
That prefix covers the 14 B Ethernet header, maximum 60 B IPv4 header and
maximum 60 B TCP header, including the fields that official TCP input updates in
place. The prefix can include payload; a frame of at most 136 B is copied
entirely. Only bytes beyond the prefix reference the original immutable MAC
fragments through `PBUF_ROM` tails. Raw consumers must treat those tails as
read-only and clone before writing. This is RX payload-tail zero-copy;
whole-frame and TX zero-copy are not provided. ICMP echo uses the official
copy-for-reply path. VLAN delivery, IPv6, DHCP, DNS and IP reassembly remain
unselected.

A retained head or tail pbuf reference keeps its frame slot and exact MAC loan
live. The final custom-pbuf free only marks pieces returned; it never calls the
provider or rearms DMA. Subsequent owner service or stop releases the exact loan
after every piece's final reference is gone. A failed release retains the token
and storage and closes further adapter admission. Products first close PCBs and
join raw users, callbacks, queues and observers. Stop returns BUSY while any
pbuf reference remains and detaches only after exact RX release and the existing
MAC/TX withdrawal obligations. Failed drain never revokes a loan.

Slot count is metadata capacity, not a provider capability or an ACK-progress
promise. Concurrent acquisition also requires independent provider loans and
enough descriptors for the actual frames. The maintained enhanced F407 MAC
engine supports independent held RX heads and reverse-order release in software
models; this fact does not establish universal capacity, lossless operation or
progress under arbitrary retained traffic. Products budget and release loans for
their selected frame geometry and workload.

The selected loaned profile keeps two spare 512 B `PBUF_POOL` payload blocks;
the copied profile keeps its existing 24 blocks. The 16 KiB arena and other
fixed protocol pools remain unchanged. RX custom-pbuf metadata, 136 B prefixes,
MAC buffers and the copied TX buffer are additional caller-supplied storage. TX
and ICMP reply fallback may use the existing bounded RAM allocator. These counts
require the curated organization fork's narrow exemption of the explicit
custom-RX/custom-pbuf profile from the stock `PBUF_POOL`/`TCP_WND` sizing
heuristic. Every other stack sanity check remains active. This exemption does
not establish stock-upstream admission or complete SRAM fit. Nominal pool counts
are not a complete SRAM measurement.

### Reference software-host interoperability chain

![FIG-32: Reference software-host data flow with two independent TCP legs and
one end-to-end TLS
session](../../../../assets/diagrams/lwip-tcp-tls-mqtt-host.svg)

FIG-32 is a reference host-test data flow, not a product architecture or MCU
network qualification. Solid bidirectional edges carry the same ciphertext over
two independent TCP connections: software Ethernet frames between the Nexus
client and a test-only lwIP peer, then a host socket between the transparent
relay and local Mosquitto. The dashed edge identifies the one TLS session, which
terminates at the Nexus TLS client and Mosquitto; the relay does not decrypt or
terminate TLS. Each lwIP process owns its own serialized stack and clock.
[Editable DOT](../../../../assets/diagrams/lwip-tcp-tls-mqtt-host.dot) and the
[network contract](../../../../../components/protocols/network/README.md) and
[maintained relay
fixture](../../../../../tests/contracts/lwip_mqtt_interop/README.md) define this
figure's source and lifetime boundaries.

The host fixtures exercise actual pinned stack and protocol implementations with
software frame ports and controlled services. Direct POSIX TLS/MQTT fixtures and
the full lwIP/TCP relay fixture retain their own execution evidence. Neither
model Ethernet nor a real host broker proves Board wiring, physical PHY/DMA, MCU
entropy quality, measured resource peaks, installed SDK acceptance or HIL.

## Exact-profile static composition

The storage headers below expose the selected supplier layout only to trusted
static composition. They are not ordinary portable consumer faces or stable
binary layout promises. Keep their explicit storage targets and supplier
requirements private to that translation unit.

## Public declarations

### byte_stream.h

```{doxygenfile} components/protocols/network/include/nexus/network/byte_stream.h
:project: nexus
```

### lwip_adapter.h

```{doxygenfile} components/protocols/network/include/nexus/network/lwip_adapter.h
:project: nexus
```

### lwip_loaned_rx.h

```{doxygenfile} components/protocols/network/include/nexus/network/lwip_loaned_rx.h
:project: nexus
```

### lwip_loaned_rx_storage.h

```{doxygenfile} components/protocols/network/include/nexus/network/lwip_loaned_rx_storage.h
:project: nexus
```

### lwip_tcp.h

```{doxygenfile} components/protocols/network/include/nexus/network/lwip_tcp.h
:project: nexus
```

### lwip_tcp_storage.h

```{doxygenfile} components/protocols/network/include/nexus/network/lwip_tcp_storage.h
:project: nexus
```

### tls.h

```{doxygenfile} components/protocols/tls/include/nexus/tls/tls.h
:project: nexus
```

### client.h

```{doxygenfile} components/protocols/mqtt/include/nexus/mqtt/client.h
:project: nexus
```

### lwip_storage.h

```{doxygenfile} components/protocols/network/include/nexus/network/lwip_storage.h
:project: nexus
```

### storage.h

```{doxygenfile} components/protocols/tls/include/nexus/tls/storage.h
:project: nexus
```

### core_mqtt_storage.h

```{doxygenfile} components/protocols/mqtt/include/nexus/mqtt/core_mqtt_storage.h
:project: nexus
```
