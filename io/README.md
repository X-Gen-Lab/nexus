# Typed I/O contracts and shared mechanisms

Public C11 interfaces cover GPIO, UART, SPI, I2C, physical Flash, watchdog,
interrupt/timer/acquisition mechanisms, CAN, USB, Ethernet and selected RTC/LPTIM
contracts. The [derived capability matrix](../docs/delivery/capabilities.md)
identifies actual provider modes and reviewed routes. An interface header alone
does not provide every operation on every chip or authorize PCB wiring.

| Source group | Responsibility |
| --- | --- |
| `include/nexus/io/` | Stable public Nexus types and operation/ownership contracts |
| `src/api/` | Checked dispatch through the selected immutable face |
| `src/mechanisms/` | Shared stream, DMA drain, wake, transaction and observation rules |
| `src/controllers/` | Shared CAN, USB and Ethernet controller engines selected by SoC targets |
| `private/` | Private engine structures, hardware boundaries and direct provider templates |
| `../soc/native/drivers/` | Independent host models implementing the same public faces |
| `../soc/<vendor>/<family>/drivers/` | Exact MCU provider and mode implementations |

Every selected instance has a readonly face containing shared readonly methods
and an independent private context. On a 32-bit target the face occupies eight
bytes of readonly storage; method tables, mutable state, selected buffers and
linked code have separate costs. Generated instances use exact static storage;
explicit constructors use storage provided by their owner. Public consumers
receive no vendor register layout or SDK state type.

Public operations validate the face and dispatch `ops(context, ...)`. Providers
can use direct private calls within their register hot paths. A missing optional
method reports `NX_ERROR_UNSUPPORTED`. Different provider/mode tables coexist
without replacing public symbols. IRQ entries are fixed to the selected concrete
instances and do not look up a factory for each byte.

Generated factory getters return existing identities. Lookup, Platform start,
request admission, service, recovery and stop remain separate actions. Controller
and endpoint identities stay distinct for SPI/I2C buses. New products should use
the generated strong-reference header; see the
[factory contract](../docs/explanation/factory.md) for headers and ID lifetime.

The direct path has one serialized executor in each operation's allowed context.
Requests and transfer buffers remain caller-owned through acquire-observed
SETTLED. Cancel, deadline and failed drain never revoke a borrow. A provider
withdraws hardware, IRQ and deferred references before its final release
publication; QUARANTINED storage and cleanup responsibility remain retained.
The [ownership table](../docs/explanation/ownership.md) distinguishes direct
calls, fixed IRQ work, cross-task adapters, waiting and shutdown.

`nexus/io/diagnostic.h` composes copied request/stream observations and explicitly
known lifecycle, owner admission, service-time and observer-join facts. The
result retains independent finding bits and unknown fact groups. An unchanged
request does not prove a missed service; a service-gap finding requires explicit
timestamps and a declared budget in the same monotonic domain. Neither this
helper nor a readable reason key touches live hardware or changes ownership.
SETTLED does not prove successful transfer or joined observers. An outstanding
stream count includes producer, ready and consumer loans, and a busy ordinary
cursor does not describe a private preposted cursor. Unknown facts remain
unknown; an empty finding mask does not establish health.

Stream reserve, publish, acquire, release, abort and stop access a constant amount
of metadata independent of block count. Stop checks the outstanding count and does
not scan or revoke borrowed blocks under an interrupt mask. Cold storage-overlap
validation examines the complete selected block set; it has a different cost.
These software complexity contracts do not establish physical IRQ latency.

DMA memory completion, peripheral wire completion and request settlement are
separate facts. Providers check exact DMA reachability, selected domains, Cache
ownership and real source withdrawal. Finite and continuous modes have distinct
loan/rearm/backpressure contracts. Default paths add no mandatory OS locks,
hidden driver tasks or universal buffer pools. The
[DMA and stream contracts](../docs/design/io-evolution.md) own exact scope.

`nx_dma_storage_validate` checks a caller's explicit whole reservation separately
from the transfer payload. For example, an aligned 64-byte reservation may carry
a 17-byte payload while exclusively owning two 32-byte cache lines. Address and
capacity must already fit the declared alignment, complete DMA-visible domain
and optional whole-line geometry; no rounding, allocation or hidden bounce
buffer occurs. The caller supplies reviewed geometry and exclusive ownership.
The provider still validates the selected mode and actual cache/DMA capabilities
and performs necessary maintenance. Cancel or deadline expiry never permits
storage reuse before drain and acquire-observed settlement.

The optional [USB activation contract](../docs/design/usb-device.md) keeps a
separate immutable activation face and the existing provider context. Its
selected nonzero endpoint setter is a bounded controller action; external
Chapter 9 policy owns configuration, rollback and ordinary packet admission.
It adds no default worker or configuration transaction. The public declaration
retains its context, partial-effect and full-stop obligations.

Native, production-register models, selected ARM links and physical HIL establish
different evidence. Independent instance state and source-directory organization
do not establish electrical behavior, measured timing or losslessness under
arbitrary load. MCU physical HIL remains `not_executed`.
