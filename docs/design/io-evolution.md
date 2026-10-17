---
id: design.io-evolution
type: explanation
audience: maintainer
source_language: en
---

(io-evolution)=

# I/O evolution: static resources and explicit data contracts

Use this page to extend an I/O mode without adding hidden execution, ambiguous
units or a second resource owner. It describes the current iteration's software
mechanisms and their boundaries. The [42-item execution ledger](io-evolution-execution.csv)
owns implementation, software and hardware status; this page does not replace
that ledger or declare the iteration complete. The sealed `c4249e98` candidate
retains its original scope. New code must establish its own execution evidence.

## Assembly and execution have different responsibilities

![FIG-16: current I/O assembly and runtime responsibilities; arrows identify generation, lookup, calls and copied observations](../assets/diagrams/io-evolution.svg)

**FIG-16 is a current mechanism view, not a hardware qualification diagram.**
The upper group shows build-time facts and generated objects. The lower group
shows runtime calls. Solid arrows carry their written meaning; dashed arrows
mark an explicitly optional mechanism. No arrow creates a scheduler, worker,
global device registry or implicit hardware discovery.

One authored TOML assembly selects reviewed SoC modes and Board routes. The
strict resolver checks fields, capacities and resource constraints before
publishing immutable typed IR and an atomic generated bundle. CMake remains
the only software target graph. An invalid input does not select a convenient
fallback or leave a usable partial configuration.

The generated factory returns an existing static face by typed identity. Lookup
does not allocate, initialize hardware, read silicon, acquire locks or start
tasks. After explicit startup, ordinary I/O calls the face's shared read-only
operations table with the instance's private context. Applications own execution
progress, scheduling and shutdown. Optional enhanced faces share their intended
controller owner; a second face does not grant a second hardware executor.

The diagram's source contracts are [factory and instances](../explanation/factory.md),
[ownership](../explanation/ownership.md), [configuration generation](build-system.md),
the [typed IR](../../tools/configure/ir.py) and the public headers linked below.
The [editable DOT](../assets/diagrams/io-evolution.dot) owns diagram topology;
locale labels do not change its edges.

## Describe resources before generating instances

The resource, trigger and DMA plans are compiler output. They let reviewers see
why a selected combination is legal; they do not program an additional hardware
mode or create a runtime resource manager.

| Plan | Responsibility | Boundary |
| --- | --- | --- |
| [Resource graph](../../tools/configure/resource_graph.py) | Physical IP aliases, personalities and hierarchical claims | Existing controller-exclusive modes remain exclusive; compatible sharing needs exact reviewed facts |
| [Trigger graph](../../tools/configure/trigger_graph.py) | Event source, route, sink, startup ordering and stop responsibility | Existing Timer-to-ADC-to-DMA routes are projected as facts; a new graph edge is not a new trigger provider |
| [DMA topology](../../tools/configure/dma_topology.py) | Separate execution lanes from requests; validate descriptor spans, memory, Cache and security facts | Validating a descriptor chain does not implement linked-list DMA execution |
| [I2C timing](../../tools/configure/i2c_timing.py) | Check explicit clock and electrical bounds using exact arithmetic | The calculated unstretched-master rate is not measured bus speed or an allowance for unsupported filter settings |

Startup dependencies must form a directed acyclic graph. A runtime event chain
may contain an explicitly autonomous cycle, but its declared stop owner must own
the whole cyclic component and its stopping action must cut every cycle. This
is a configuration proof about supplied facts, not proof that silicon has stopped.
IRQ, DMA, bus and request drain still follow their provider contracts.

Do not infer that two timer channels can share a controller merely because their
pin assignments differ. Common base frequency, update events, trigger outputs,
break inputs, reset and clock ownership can still conflict. Likewise, two typed
interfaces for one physical IP do not create two independent peripherals.

## Optional traits report selected behavior

The existing `factory.capabilities` switch controls generated read-only trait
objects. [Selected I/O traits](../../tools/configure/io_traits.py) describe a
reviewed provider and mode, including formats, limits, abort scope, continuity,
timestamp origin and DMA policy. Turning the switch off emits no new trait
objects. It does not change the two-pointer base face or add mutable device state.

A continuity value such as `double_buffer` or `rearm_gap` describes a mechanism,
not demonstrated throughput. A Cache line requirement describes the selected
provider policy, not proof that a particular clock profile enabled Cache. CAN
mailbox counts are provider-specific facts. Traits grant neither Board wiring
authorization nor physical qualification and cannot override admission checks.

Controller and endpoint records describe their own interfaces. Do not apply a
parent controller's byte-SPI or ordinary-I2C limits to an enhanced endpoint.
A per-request word-width marker does not mean unlimited length. Check the
selected face, count units and provider limit before allocating caller storage.

## Cold silicon admission is an explicit step

[Silicon preflight](../../core/include/nexus/core/silicon.h) reads one explicit,
bounded source in caller-serialized cold Thread context. It copies device-group,
revision and Flash-density observations and compares an immutable policy. It
does not discover devices, enable clocks, change option bytes or retain a loan.
The default platform does not invoke this optional helper automatically.

A device group and Flash density cannot prove an exact orderable part, package
or PCB. Revision rules bind a reviewed source digest and required workaround
mask; the helper does not authenticate that source or prove a workaround was
implemented. Unknown revisions require rejection or an explicit unreviewed
policy. That policy cannot bypass a blocked revision, missing observation,
device or density mismatch, or missing required workaround.

The caller decides whether to start hardware after admission. Reading a revision
does not establish electrical behavior, errata coverage for every mode or HIL.

## Use units that match the selected data path

The enhanced contracts are separate typed interfaces. Their existence does not
change the limits of the ordinary byte interfaces or enable a mode in every
provider. Current generated selection and execution status remain in the ledger.

| Mechanism | Units and ownership | Current boundary |
| --- | --- | --- |
| [UART frame description](../../io/include/nexus/io/uart_format.h) | Data bits exclude parity; stop and flow settings are explicit | Provider and reviewed Board routes must admit the combination; changing a format is a cold operation |
| [UART words](../../io/include/nexus/io/uart_word.h) | Each `uint16_t` carries a nine-bit value; counts and transferred prefixes are words | Independent Native, STM32F407 and GD32F470 IRQ paths; DMA and other providers do not inherit this interface |
| [SPI whole-CS transaction](../../io/include/nexus/io/spi_transaction.h) | Up to 16 segments, one common 8/16-bit word width, complete-word prefix | Synchronous polling extension with explicit provider limits; existing byte-span and finite DMA contracts remain separate |
| [Segmented SPI DMA](../../io/include/nexus/io/spi_dma_transaction.h) | Words; explicit TX/RX addresses and caller-owned fixed cells; one width across at most 16 segments | Independent asynchronous request contract; provider integration and software verification remain specific to the selected mode |
| [Long I2C messages](../../io/include/nexus/io/i2c_long.h) | Bytes; one START per message; a later message causes repeated START | Enhanced messages allow up to 65,535 bytes; ordinary messages retain their existing limit and require no additional face |
| [ADC integer format](../../io/include/nexus/io/adc_format.h) | Explicit signedness, byte order, storage width, padding and scan order | Layout validation and decoding do not add injected conversion, wider acquisition or synchronized ADC hardware |
| [PWM group](../../io/include/nexus/io/pwm_group.h) | A complete fixed-base duty/trigger batch in declared timer ticks | A coherent staged batch is not an observed update; generated selection, analog wiring and ADC synchronization are separate exits |

UART word requests retain the ordinary ACCEPTED-to-SETTLED borrowing rule. A
cancel or deadline never returns the request or payload early. The owner keeps
servicing wire completion and drain; failure to prove drain retains QUARANTINED
storage. Byte UART buffers and state do not become nine-bit buffers implicitly.

SPI validates every segment and data span before hardware use. A null TX pointer
means repeated dummy words, a null RX pointer means discard, and both may be null
for clock-only phases. All segments share one asserted CS interval. Polling
return releases the synchronous borrow only after CS is released and either wire
drain or provider reset is proven; timeout cannot preempt an in-flight word.
An error never replays earlier command phases. The asynchronous segmented
interface has a separate admission, descriptor and cancellation contract.

The STM32F407/GD32F470 segmented DMA mechanism is a service-driven finite chain.
Hardware DMA moves one segment, then the owner proves its drain before service
arms the next. CS stays asserted across segments, but SCK gaps are permitted;
this is not hardware linked-list DMA or a gapless transfer guarantee. The request,
all descriptors, all spans and explicit fixed dummy/discard cells stay borrowed
until acquire-observed SETTLED. Intermediate completion, cancel and timeout do
not release them. Caller-driven service remains necessary; wake attachment and
recovery capabilities cannot be inferred from the base SPI port.

I2C hardware reload advances a long message without inserting another address
phase. STOP and successful bus-idle observation precede successful return.
On failure, the returned byte count records provider progress, including writes
handed to TXDR before their wire ACK is known. The count does not authorize replay.
Cancellation or failure does not justify replaying an acknowledged prefix or
silently performing bus recovery. The enhanced face and base I2C interface share
one executor. Timing inputs must include their reviewed clock and electrical
bounds; a calculation is not a measurement of rise time or clock stretching.

ADC decoding reads only consumer-owned or acquired, producer-detached storage.
It handles one-to-four-byte integer samples without allocating, retaining a
pointer or performing calibrated voltage conversion. The consumer must first
join producer/DMA access. Declared regular/injected groups and trigger identities
describe an already supported mode; they do not start conversion or turn a
software observation into a sample-aperture timestamp.

PWM group commit stages one coherent shadow batch; `SUCCESS` does not prove the
timer already applied it. A partial hardware-write failure must withdraw output
and retain fault responsibility. Clearing a break does not enable output. The
limited G474 register implementation does not supply PCB pin selection, analog
thresholds, a power stage or a synchronized ADC provider. Those remain explicit
integration and physical-validation tasks, with product control policy external.

## Observe time without inventing timing guarantees

[Timestamp facts](../../io/include/nexus/io/observation.h) distinguish hardware
events, IRQ observations, service observations and acquire observations. A
caller supplies extended ticks, a domain, an epoch, an exact stated tick rate
and explicit uncertainty knowledge. The domain owner extends counter wrap and
changes epoch after discontinuity, reset or rate change.

Elapsed-time comparison requires matching domain, epoch and rate, ordered ticks
and a representable uncertainty bound. It does not synchronize clocks, convert
time domains, access a hardware clock or prove that two facts refer to causally
paired events. Unknown uncertainty is different from a known zero bound.

Checked UART/SPI/I2C service reports an invalid face or an absent service method.
`SUCCESS` establishes one dispatch only. It does not establish progress, health,
SETTLED, wire completion or permission to reclaim storage. The ordinary service
context, serialization and lifetime rules still apply, with no hidden worker.

## Cost and validation stay visible

Build-time resource plans do not impose a runtime graph walk. Optional traits
are constant data. Separate enhanced faces keep the simple path independent;
their selected provider state, buffers and code still cost space and must be
measured in an actual linked image. Segment validation depends on descriptor
count, and silicon-policy validation depends on rule count. Do not infer cycles,
worst-case latency or zero memory growth from an interface diagram.

| Evidence | What it establishes | What it does not establish |
| --- | --- | --- |
| Metadata and resolver tests | Rejection and generated-plan behavior for supplied facts | New hardware execution or a reviewed PCB route |
| GoogleTest/GoogleMock and register models | Executed production-code contracts under the stated host model | Real IRQ latency, electrical timing, uninterrupted throughput or power |
| ARM ELF, map and resource checks | Actual compilation, linking, retention and static budgets | Runtime correctness or measured stack high-water marks |
| Fresh offline builds and candidate verification | Their exact clean source, SDK and same-artifact software scope | Qualification of later commits or physical hardware |
| Physical station execution | The exact Board, firmware, wiring and measured conditions | Automatic qualification of a family, package or unrelated mode |

The current iteration's physical status remains `not_executed` until a station
runs the applicable checks. Applications, protocol policy, product partitions,
control algorithms and recovery policy remain in external consumer projects.

## Work that is still a separate implementation

The ledger retains the exits for real linked-list DMA, continuous high-throughput
H743 paths, U575 low-power autonomous execution, synchronized control groups,
CAN-FD, advanced Ethernet/USB, external storage, TrustZone services, multicore
transport, audio and frame devices. A public type, validated topology or model
does not complete these exits. Each mode needs its own provider, reviewed route,
failure and ownership tests, ARM/SDK checks, resource costs and physical evidence
where applicable.

Use [platform integration contracts](integration-contracts.md),
[the lifecycle matrix](io-lifecycle-matrix.md),
[software qualification](../reference/platforms/qualification.md) and
[the 42-item ledger](io-evolution-execution.csv) when reviewing or delivering a
change. Update that single ledger with actual evidence; do not replace pending
work by a broad claim that the platform supports an entire MCU family.
