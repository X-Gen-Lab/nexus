# Optional bounded coreMQTT adapter

This component adapts the official MQTT 3.1.1 coreMQTT 2.3.1 implementation,
organization repository `X-Gen-Lab/coreMQTT`, curated commit
`f4445aa70cb211504bca2c9e4177cd74609f5fba`. Its upstream release parent is
`2beef04725328923e05e576b884212d53ec97af7` (tree
`c216ae5b6848f60d43b498aea71701d9ac5a5159`). The organization patch removes the
unused host CMock gitlink and metadata and records that provenance; the three
production C files, four public/default headers, transport interface and MIT
LICENSE remain byte-identical. CMock is not initialized or required. MQTT 5 releases use a different API and are not
automatic upgrades of this profile.

The zero-argument `nexus_add_coremqtt()` helper consumes the configured
assembly's `Nexus::Config.NEXUS_SOURCE_DIR`. Products pass no vendor or include
paths. The helper selects the exact organization source below that platform,
uses the maintained source lock to verify repository or installed-SDK identity,
and registers the observed files as configure dependencies before creating
targets. Source-lock, SDK file-map and configuration selection remain with the
parent repository's existing authorities. It does not fetch dependencies or
create a source cache override. Repeated selection in one build is idempotent;
the private two-path implementation is not a product interface. It requires the selected
Nexus ABI, build options, strict owned warnings and target policy. Vendor sources
receive the selected ABI, optimization, section and cache/LTO policy; the owned
adapter additionally receives strict warnings. These properties are checked on
actual helper-generated Native and M4F compilation commands. An unselected
helper defines no target or runtime state. `Nexus::CoreMQTT` exposes
Nexus API types; `Nexus::CoreMQTTStorage` additionally exposes the exact pinned
backend layout only at the composition site. Neither is a binary ABI promise.

```cmake
find_package(Nexus 0.2.1 EXACT CONFIG REQUIRED)
nexus_add_coremqtt()
nexus_add_firmware(my_device SOURCES main.c
    LIBRARIES Nexus::CoreMQTTStorage)
```

An ordinary source checkout uses its parent Gitlink and organization-origin
contract. An installed source SDK automatically uses its verified manifest and
exact dependency records. Neither product path accepts a caller-supplied
backend directory. `Nexus::CoreMQTT` keeps supplier and private profile headers
out of generic consumers; only the explicit storage composition target exposes
the pinned backend layout. These build/visibility checks do not qualify a broker
session, network or Board.

## Scope and ownership

The first profile supports clean-session CONNECT, QoS0/1 PUBLISH, one filter per
SUBSCRIBE/UNSUBSCRIBE call, PUBACK/SUBACK/UNSUBACK correlation, received publishes,
DISCONNECT and caller-driven keepalive/process steps. Topic policy, credentials,
authorization, reconnect, offline queues, semantic retry data, persistence and
broker choice belong to the product. QoS2, persistent-session resumption/replay,
last will, MQTT 5 and automatic reconnection are outside this profile. Inputs
must meet MQTT UTF-8 requirements; this adapter does not add a Unicode codec or
claim certification beyond the pinned library's validation. Basic lengths,
NUL/topic-wildcard restrictions and filter placement are checked before I/O.

The caller allocates zeroed `nx_mqtt_t`, one network buffer, official incoming
and outgoing QoS records, and finite control-ACK slots. Counts and buffers are
explicit; there is no heap, maximum-size pool, worker, subscription registry or
mandatory OS lock. Arrays are mutually disjoint and remain exclusively borrowed
until deinit. Initialization does not connect or use the transport. All calls and
callbacks for one client use one serialized task executor. The reentry guard is
not a cross-task mutex.

Official `MQTT_InitStatefulQoS` owns publish state. Control slots only correlate
the client's outstanding single-filter SUBSCRIBE/UNSUBSCRIBE requests; they do
not duplicate the official QoS engine. Packet IDs avoid both outstanding control
requests and publish records. Selection cost depends on explicit slot counts;
this cold-path search is not advertised as constant-time.

## Ports and progress

The shared `nx_byte_stream_t` is two readonly operations plus context. Nonempty
writes succeed only with a positive accepted count copied out of the caller's
buffer. Reads succeed with positive data, or zero for orderly EOF. BUSY with
zero bytes is temporary backpressure. Neither callback retains the MQTT input
buffer after returning. TLS retry ownership belongs to TLS's own stable staging;
a stream success is local acceptance, not peer delivery.

Each operation supplies a finite absolute microsecond deadline and nonzero
transport-call budget. The product hook `nx_mqtt_port_now_us()` supplies the same
stable monotonic domain for every client and transport; upstream's context-free
32-bit millisecond timer is derived from it without an active-instance global.
`nx_mqtt_port_assert()` is a required nonreturning product fault hook. It does
not authorize reclamation of DMA or borrowed storage.

The reviewed polling profile is 1 ms and the adapter separately bounds transport
calls, so a stopped clock cannot make a failed transport loop forever. A bounded
optional idle callback may service lower layers or wait in the caller's context;
SUCCESS/BUSY permit retry, other errors fail the session. No default wait or
scheduler is installed. Connect is synchronous: allocate an appropriate budget
and progress port for real network latency. Process and keepalive run only when
the product calls them. Official keepalive consumes PINGRESP internally; it is
not an application event. An arbitrary application callback must itself be bounded;
a deadline is checked after its return, not by preempting it.

## Loan and failure boundaries

CONNECT strings and outgoing publish topic/payload are borrowed only during the
synchronous call. The adapter performs no hidden message retention or replay.
QoS1 send success returns its packet ID and leaves the official ACK record in
use; only a later PUBACK event confirms it. Products retain whatever semantic
message data they need for recovery. Clean-session failure may leave delivery
uncertain, so a business retry can duplicate a message.

Incoming event/topic/payload views are valid only during the callback and carry
explicit lengths rather than NUL termination. Copy any data needed afterward.
Callbacks cannot reenter the same client or destroy its storage; another
independent client does not share its active state or transport route.

Invalid admission, capacity exhaustion before I/O and an already expired call
have no wire effects. A partial send, transport error/EOF, mid-call timeout,
budget exhaustion, clock regression or protocol failure is sticky. Further
traffic rejects until deinit; no retry silently replays a possibly sent frame.
A refused subscription is reported as a permission event and does not by itself
poison a valid session. Unknown control ACKs fail closed. A successful explicit
clean-session reconnect clears pending control requests from its predecessor,
just as the official engine resets its own QoS records. The product must first
drain/replace the old underlying stream; the adapter never reconnects it.

DISCONNECT means that MQTT bytes were accepted locally, not that TLS/TCP drained.
Deinit is local and performs no I/O; it ends MQTT borrows only after all calls and
callbacks have returned. Products separately close/drain the underlying stream
before creating another session. They own secret-buffer protection and wiping;
this component does not supply credential storage or migration policy.

## Validation boundary

Focused GoogleTest/GoogleMock tests compile the three actual official C files
and the adapter as C11 with the maintained warning/optimization context. Scripted
transport bytes exercise real serialization, QoS state, ACK handling, partial
progress and failures. They are not a real broker, MCU connection, TLS integration
qualification or physical test. Initial unsupported-stub RED and later six
failure regressions retain original source, binaries and reports. Full root
hooks, installed SDK consumption, release qualification and physical HIL remain
separate acceptance work.
