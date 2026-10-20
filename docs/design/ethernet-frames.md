# Ethernet frame loans and the MAC hardware clock

The optional `eth_frames` face adds complete scatter-gather frames, one in-band
VLAN C-tag and MAC hardware-event timestamps. It selects independent provider
state, eight-word descriptors and caller-owned storage. The existing `eth` face
keeps its contiguous untagged-frame contract, four-word descriptors and existing
operations table. Selecting one does not expand the other.

The first hardware-facing implementations are the exact STM32F407 and GD32F470
RMII providers at their maintained 168 MHz and 200 MHz HCLK profiles. Native
provides an explicit C hardware model. Executed host validation covers public
contracts, the common engine, exact family providers, Native and normal
assembly/factory composition. Both canonical ARM firmware configurations also
compile and link. These are software checks. Physical RMII, PHY behavior, external
bus timing, timestamp accuracy and station HIL remain `not_executed`.

![FIG-16: build-time assembly selects one immutable face and one private physical owner; normal frame calls dispatch directly to that owner](../assets/diagrams/io-evolution.svg)

## Assembly and hardware ownership

The maintained normal assembly contract uses an authored
`eth_frames.mac` controller to select `rmii-vlan-sg-ptp`, a reviewed
RMII route, MAC address, exact TX/RX ring sizes, RX-buffer size, IRQ policy and
finite initialization/drain budgets. The factory contract returns the existing static
`nx_eth_frames_port_t` through `nx_factory_eth_frames(NX_ETH_FRAMES_ID_MAC)`.
Lookup must perform no hardware access, allocation or initialization. The paired
MDIO face addresses the same physical owner through its separate public type.
The canonical `f407zg-eth-frames.toml` and `gd470-eth-frames.toml` fixtures in
`cmake/package/verification/io_evolution/` exercise this complete composition.

Platform owns the System clock lease, pins, RMII selection and MAC/TX/RX/PTP
clock gates. The provider manages its MAC, DMA, PTP counter and IRQ inside that
lease. Old raw Ethernet and enhanced frames cannot own the same MAC concurrently.
A product supplies PHY reset, address, power, autonegotiation/link observation,
network stack, packet classification and shutdown policy outside this repository.

Startup establishes actual HSE8/PLL168 or HXTAL25/PLL200 before MAC access. The
family provider checks fresh oscillator/PLL readiness, actual source status,
PLL factors, AHB/APB dividers, RMII selection and the separate PTP gate. Merely
requesting PLL selection or declaring a configured frequency cannot authorize
16 MHz fallback or a different actual divider. Both manuals require Ethernet AHB
at least 25 MHz. External RMII REF_CLK at 50 MHz does not establish HCLK or the
PTP input clock.

The provider rejects foreign/live descriptor tables before reset. Descriptors
and packet storage must be in the exact main-SRAM DMA domain. F407 CCM and GD470
TCM are excluded. Software metadata may use other CPU-accessible storage. The
hardware descriptor stride is 32 bytes; the ST HAL descriptor type has additional
software backup words and is not used as the hardware stride. Official headers
supply reviewed layout/mask definitions; vendor C runtimes, descriptor pools,
mutable global cursors and HAL tick initialization are not selected.

## Complete frames and VLAN handling

A TX request contains immutable `nx_eth_fragment_t` views. Preparation validates
all lengths, pointer overflow and the complete L2 header across fragments before
changing request storage. Empty fragments reject. One EtherType 0x8100 C-tag is
accepted; its TCI remains in the payload. Nested tags, 0x88A8 and length-coded
802.3 frames reject. Untagged frames contain 14..1514 caller bytes; tagged frames
contain 18..1518, excluding preamble, hardware padding and hardware FCS.

The selected mode performs no tag insertion or stripping. PCP/DEI/VID policy,
access-port/trunk policy and VLAN membership belong to the product. Hardware
padding/FCS remain enabled and checksum offload remains disabled.

| Fact | TX | RX |
|---|---|---|
| Unit | One complete L2 frame | One complete L2 frame |
| Hardware storage | One descriptor per supplied fragment | As many ring buffers as the received frame needs |
| First/last facts | Software metadata selects FS and LS before publication | Actual FS/LS writeback delimits the frame |
| FCS | Inserted by hardware | Excluded from views, including a four-byte FCS spanning two buffers |
| Buffer availability | Complete chain is admitted before any first OWN | A held descriptor is never rearmed or overwritten |
| Result | `transferred` is full caller length on proven MAC success, otherwise zero | All-or-nothing view capacity; no partial public frame |

Admission first checks every fragment, DMA range and free descriptor. Rejection
leaves request and OWN state unchanged. After acceptance the provider records a
PREPARING reservation, programs variable descriptor work without masking IRQs,
then publishes the first OWN only after all following descriptors are ready.
An IRQ cannot interpret a CPU-owned PREPARING descriptor as completed traffic.
Each IRQ advances at most one TX descriptor and one RX descriptor. Intermediate
TX writeback is not whole-frame wire completion; the actual last descriptor
provides the final status and optional capture.

RX acquisition produces one `nx_eth_frames_rx_loan_t` plus caller-provided fragment
views. Too few views return `OVERFLOW` without dequeuing the frame or changing
outputs. Views exclude FCS even when the FCS consumes the whole tail descriptor.
The token retains every hardware descriptor until exact release. Copying a token
creates no additional loan. Stale, foreign, semantically altered and repeated
releases reject. Structure padding is not token identity.

## Borrow, cancellation and recovery

![FIG-06: admission establishes a borrow; cancellation and quarantine retain storage until actual withdrawal precedes SETTLED](../assets/diagrams/request-lifecycle.svg)

Accepted TX borrows the complete request, fragment array and every payload until
acquire-observed `SETTLED`. A descriptor returning OWN does not release a fragment
independently. RX bytes remain immutable while the complete-frame loan is held.
The view array itself is an unretained output and may be reused after acquisition.
Provider output storage must remain disjoint from provider state, rings, control
objects and all active request/payload loans.

The MAC has controller-wide TX withdrawal. Cancelling one pending request closes
TX admission and drains other pending TX too; it does not invent per-frame abort
support. Actual completed transmission may win cancellation. Timeout, cancellation
and a failed drain never revoke a borrow. Failure to prove engine/MAC/FIFO quiet
leaves requests `QUARANTINED` with their payload addresses retained.

The STM32 ES0182 control-write workaround uses a finite volatile Device-read guard
based on the leased HCLK. The guard exceeds four internal TX/RX clocks at the
slowest supported 10 Mbit/s rate, including RMII's internal REF_CLK/20 divider.
Four HCLK readbacks or the fastest possible 80 ns lower bound are insufficient.
The guard applies to affected control writes, not packet poll/W1C writes. The
shared GD32 mechanism keeps the same conservative discipline without attributing
ST's silicon erratum to GD32. FIFO empty must be observed before issuing FTF;
flushing a nonempty FIFO cannot manufacture cancellation proof.

Stop closes new admission/acquisition and withdraws MAC/DMA/IRQ references before
releasing storage. Held RX and captured wake publishers return `BUSY`. An optional
wake sink remains borrowed until publishers exit; notification is a hint, not the
completion predicate. The caller separately stops and joins external producers,
consumers, waiters and observers before reclaiming their storage.

Explicit recovery belongs to the exclusive MAC owner. It rejects held RX or live
publishers before issuing reset. Only acknowledged reset may detach incomplete TX
and settle it with its locked failure cause. A reset timeout retains ownership;
there is no automatic reset, retry or replay. Successful reset records an RX gap
and advances the MAC clock epoch. Stop/start preserves RX-token incarnation and
clock-epoch counters so old identities cannot become valid again.

## Timestamp meaning

Every time fact uses the existing `nx_io_timestamp_t`. The assembly contract allocates one
nonzero 32-bit domain from its namespace of real counters; applications cannot
make unrelated counters comparable by assigning the same local device ID.

The selected clock uses digital nanosecond rollover, 20 ns subsecond increment
and fine update. Its accumulator input is HCLK, with addend 1278264076 at 168 MHz
or 1073741824 at 200 MHz. These values and stated units are not measured oscillator
accuracy. The counter is not UTC, TAI or a synchronized network clock.

| Observation | Source | Value |
|---|---|---|
| Valid TX last-descriptor or RX last-descriptor capture | `HARDWARE_EVENT` | Extended nanosecond ticks from the MAC |
| Coherent current MAC-counter read | `SERVICE_OBSERVATION` | Same domain, epoch and 1e9 ticks/second |
| Consumer obtains an RX loan | `ACQUIRE_OBSERVATION` | Separate `acquired_us` in the core monotonic domain |
| Missing/corrupt capture | `UNKNOWN` | No usable event measurement |

Capture validity comes from actual TTSS/TSV writeback. A valid zero capture is a
real zero event. Missing validity never reuses a previous descriptor capture.
All-ones or malformed raw capture records a separate corruption flag and an
UNKNOWN stamp, without replacing it with acquisition time. Unknown uncertainty
remains explicit; zero uncertainty is not invented.

Coherent counter reads retry a seconds rollover. Natural 32-bit seconds wrap is
extended without changing epoch. Reset/restart changes epoch. The owner rejects
counter regression within one epoch, ambiguous observation/capture gaps and
64-bit tick overflow rather than assuming how many wraps elapsed. Timestamp
comparison requires matching domain, epoch and rate. Products explicitly
establish event pairing and any cross-clock mapping.

## Validation and remaining physical work

The maintained external MMIO model distinguishes visible register readback from
internal latching, models W1C separately from read-only DMA process state, and
refuses engine withdrawal/reset when fault controls request it. Native ingress
and egress are explicit complete-frame events: they do not automatically service
or settle the provider. Each model instance has its own counter and DMA identity.

The Native source validates active loans under the same exclusion as provider
settlement. A deterministic final-access regression lets the real provider settle
an accepted request and lets its consumer legally protect the reclaimed page;
an old unprotected reader faults. This establishes that software boundary and
does not substitute for physical DMA or complete concurrent-system qualification.

Executed host validation contains 108 public/common/family/Native cases plus six
factory cases for each canonical assembly, with no failures or skips. Factory
cases exercise real System/GPIO/provider code against one shared clock/timer
oracle and actual DMA-visible addresses. Both canonical ARM configurations compile
and link with the selected sources. Station evidence remains separate: verify PHY
clock/reset, routes and electrical levels, both link rates, DMA traffic under
load, control-write timing, VLAN preservation, error/drain/reset, PTP capture
behavior and measured clock accuracy on actual boards.

The known GD32 ENET inter-frame-gap limitation remains a physical qualification
constraint; this software mode does not assert IEEE timing conformance from host
models. Protocol stacks, PHY/PCB policy, time synchronization and application
products remain external platform consumers.
