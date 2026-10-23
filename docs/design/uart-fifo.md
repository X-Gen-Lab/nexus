---
id: design.uart-fifo
type: explanation
audience: consumer
source_language: en
---

(uart-fifo)=

# Bounded UART hardware FIFO bytes

Select `irq-fifo-bytes` when an exact H743 or U575 assembly needs hardware
FIFO buffering with an explicit bound on each interrupt's byte work. The
provider uses the existing public UART face, caller-owned TX requests and a
static RX byte ring. It creates no task, worker, allocation or second owner.
Threshold batching does not establish physical latency or loss-free throughput.
The independently selected `irq-fifo-rtscts-bytes` mode adds hardware RTS and
CTS on an explicit four-pad software-reference route, as described below.

## Select the exact controller and clock

![FIG-16: current static generation, typed lookup and private I/O ownership; FIFO selection retains one UART owner](../assets/diagrams/io-evolution.svg)

**FIG-16 shows current construction and runtime ownership, not FIFO timing or
silicon qualification.** The factory returns one `nx_uart_port_t`. Its selected
readonly table calls a private provider built from one shared compile-time
algorithm and exact-family official LL leaves. The arrows retain their
generation, lookup and call meanings; no forwarding-only runtime layer is added.

| Selection | H743 reference | U575 reference |
| --- | --- | --- |
| Exact chip | STM32H743ZIT6 | STM32U575ZIT6Q, TZEN0 |
| Controller and route | USART3, PD8/PD9 AF7 | USART1, PA9/PA10 AF7 |
| Clock profile | `hsi64-fixed-cache-off`, PCLK 64 MHz | `msis4`, PCLK 4 MHz |
| Hardware FIFO depth per direction | 16 bytes | 8 bytes |
| Maximum RX staging, including RDR | 17 bytes | 9 bytes |
| Vector | USART3 IRQ39 | USART1 IRQ61 |
| Normal fixture | Baremetal, priority 5, 115200 baud, 8N1 | Baremetal, priority 5, 115200 baud, 8N1 |
| Software RX ring in that fixture | 64 bytes | 64 bytes |

The extra RDR stage matters: FIFO depth is not the complete RX backlog. The TX
shifter is also separate. This selection does not change FIFO-disabled ordinary
UART or DMA modes. F407/F470 and other UART IPs do not inherit FIFO support from
the public API. H743 high-clock profiles, U575 Secure execution, DMA, block/word
RX, non-8N1 frames and low-power/autonomous operation remain outside both FIFO
selections. Ordinary `irq-fifo-bytes` remains flow-none.

These full inputs use the existing Board routes. The FIFO subsection belongs
to the same authored assembly and typed IR as all other resources.

```{literalinclude} ../../cmake/package/verification/io_evolution/h743-uart-fifo.toml
:language: toml
```

```{literalinclude} ../../cmake/package/verification/io_evolution/u575-uart-fifo.toml
:language: toml
```

## Select hardware RTS and CTS explicitly

`irq-fifo-rtscts-bytes` retains the same public byte face and selected FIFO
state layout. Its distinct readonly operations table selects a compile-time
specialization of the same algorithm. No extra mutable flow flag, per-instance
clock field, queue, task or worker is added to either FIFO or ordinary UART.

| Selection | H743 software reference | U575 software reference |
| --- | --- | --- |
| Exact chip, controller and clock | STM32H743ZIT6, USART3, `hsi64-fixed-cache-off` | STM32U575ZIT6Q, TZEN0, USART1, `msis4` |
| TX / RX / CTS / RTS, all AF7 | PD8 / PD9 / PD11 / PD12 | PA9 / PA10 / PA11 / PA12 |
| Software-reference package | `stm32h743zi_rtscts_reference` | `stm32u575zi_rtscts_reference` |
| First normal selection | Baremetal, priority 5, 115200 baud, 8N1, 64-byte RX ring | Baremetal, priority 5, 115200 baud, 8N1, 64-byte RX ring |

The new mode derives fixed 8N1 with `flow = "rts-cts"`. An omitted frame
subsection uses that value; an explicit frame must provide all four fields and
match it exactly. RTS-only, CTS-only, modem notifications, polarity inversion,
runtime format changes, RS485-DE sharing and FIFO/DMA combinations are not
selected by it. The existing four FIFO threshold/budget fields remain explicit.

These packages declare silicon package pads and AF numbers. They do not
establish a Nucleo connector, virtual COM wiring, product PCB or peer electrical
compatibility. All four pads are exclusive resources; a Board reservation for
USB, DE or GPIO on either flow pad conflicts with the UART selection. A two-pad
Board binding cannot silently become a four-pad route.

```{literalinclude} ../../cmake/package/verification/io_evolution/h743-uart-rtscts.toml
:language: toml
```

```{literalinclude} ../../cmake/package/verification/io_evolution/u575-uart-rtscts.toml
:language: toml
```

Hardware CTS restricts the start of the next serial frame. It does not forbid
CPU writes into available TDR/FIFO space, and a frame already shifting can
finish after CTS rises. The ISR CTS flag is the inverse of the physical pin
level; CTSIE remains disabled. The provider never bypasses CTS to complete a
cancel or stop. Persistent external blocking can outlast the free-running drain
allowance: the request becomes `QUARANTINED` and remains borrowed. Only actual
wire completion and verified withdrawal permit settlement, including after a
later physical CTS release. Peer recovery policy remains external.

Hardware RTS responds to the hardware receive staging, not a software ring
watermark. When the software ring becomes full, this specialization stops RDR
reads and withdraws RX threshold, not-empty, full, IDLE and error sources with
readback. It keeps the held head and its error flags, UE, RE and RTSE intact.
The hardware FIFO/RDR can then accumulate data and apply real RTS backpressure.
A peer that ignores RTS or sends too much after its change can still cause
overrun; no unconditional loss-free guarantee follows from this design.

A public byte read creates ring space and rearms the selected RX sources with
readback. If a subthreshold head is held, a not-empty level source permits
progress without another arrival or IDLE edge. A rearm failure returns
`NX_ERROR_IO` with the actual copied count and bytes still valid; admission
closes and the owner retains cleanup responsibility. A closing read never
reopens the sources. A previously pending vector can produce a bounded extra
entry after source withdrawal; it cannot consume a held head while paused.

Generated cold assembly snapshots all four pins and the selected clock gates
before preparing them. TX and RTS GPIO latches are set high before AF mode;
CTS and RX receive pull-ups. Output type, speed, pull, AF7 and mode fields are
read back. CTSE/RTSE are programmed only while UE is observed zero and are
proved before the first enable. Whole stop retains flow while TX is borrowed;
only after source/vector/publisher joins, UE zero and both ACKs clear may it
clear flow/FIFO fields and restore the original four-pad snapshots and gates.
Missing proof retains ownership. GPIO reset levels, inactive AF behavior and
the physical startup/stop handoff still need actual board validation.

## Separate thresholds from IRQ budgets

| Field | Meaning and admitted values |
| --- | --- |
| `fifo.rx_threshold` | Hardware receive threshold: `eighth`, `quarter`, `half`, `three-quarters`, `seven-eighths` or `full`. |
| `fifo.tx_threshold` | Hardware free-space threshold: the same fractional names, with `empty` instead of `full`. |
| `fifo.rx_budget_bytes` | Maximum RDR reads per handler entry, 1..16 for H743 or 1..8 for U575. |
| `fifo.tx_budget_bytes` | Maximum TDR writes per handler entry, with the same per-chip limits. |

All four fields are explicit. The normal fixtures use `half` thresholds, RX
budget 2 and TX budget 3. A budget is independent of the threshold and may be
smaller than the hardware batch. The typed IR maps names to the reviewed
raw 0..5 official LL encodings; authored values are not shifted register masks.
RX and TX have different final encodings. Exact-family status semantics remain
in the hardware leaves rather than a universal FIFO model.

Steady RX uses threshold, IDLE and error sources. The handler acknowledges
captured IDLE before draining. If its budget ends with data remaining, it
enables the level-sensitive RX-not-empty source with readback. Subsequent
bounded entries consume the tail without new traffic or a second IDLE. Once
empty, the handler withdraws that temporary source with readback. Unchanged
masks are not rewritten. A refused required change closes admission and retains
recovery responsibility.

PE, FE and NE describe the current RDR head. Captured flags are acknowledged
before that head is popped; its bad byte is discarded and the next status is
read afresh. With flow-none, overrun and a full software ring record aggregate
loss while preserving older queued good bytes; the RTSCTS specialization holds
a pending head when the ring is full, as described above. `read_bytes` may return
copied bytes with
`NX_ERROR_OVERFLOW`; this indication is not an exact missing-character count.
An empty read returns `NX_ERROR_EMPTY` when no loss is pending.

## Keep the task and request contract

![FIG-06: current request borrowing continues through drain and quarantine until final release-publication](../assets/diagrams/request-lifecycle.svg)

**FIG-06 describes the shared request lifecycle. FIFO space is not a
request-completion state.** Its optional QUEUED path belongs to an explicit
adapter. Cancellation and deadline expiry retain accepted storage.

1. Start the platform from its privileged, unmasked task owner. Check the primary error,
   cleanup error and remaining effects. A failed constructor can retain cold
   leases and requires the same owner to complete stop.
2. Obtain `nx_factory_uart(NX_UART_ID_CONSOLE)`. Lookup is pure and may precede
   startup; operations require successful platform startup.
3. Initialize a request once, prepare it with `nx_uart_tx_prepare` and submit
   through `nx_uart_port_submit`. Use an absolute deadline in the live
   `nx_time_now_us` domain. Keep the accepted request and immutable payload live
   until acquire-observed `SETTLED`.
4. Call `nx_uart_port_service` from the serialized task owner. IRQ dispatch
   moves bounded bytes and latches facts; it never settles requests. Optional
   wake hints do not replace the request predicate. A kernel adapter must
   separately validate its selected IRQ policy.
5. Use `nx_uart_port_cancel` to request drain. Continue service and preserve
   storage while draining or quarantined. Join consumers and observers before
   platform stop; retain the RX ring, wake target and owner until stop succeeds.

Every TX load checks the request, byte position, terminal cause and current
space. TXFT, TX-not-full and TX FIFO empty do not prove the last stop bit has
left the pin. Only fresh USART `TC` after loading proves wire completion; task
service must still withdraw TX sources with readback before clearing the active
reference and release-publishing `SETTLED`.

Cancellation and deadline observation stop further payload loads and preserve
the first terminal cause. Already loaded FIFO/shifter bytes remain borrowed.
Drain allows up to `(FIFO depth + 1) * 10` serial bits for 8N1, rounded upward in
the existing monotonic domain with the reviewed guard. UART and TIM2 use the
same admitted clock; no extra clock field is added. Later observations do not
refresh an existing drain deadline. Missing withdrawal proof at expiry retains
`QUARANTINED` storage rather than fabricating a flush or successful release.

## Prove cold programming and whole stop

Before effects, initialization validates state/ring/profile separation, exact
controller/vector, live clock/reset access and quiet foreign controls. FIFOEN
and both thresholds are programmed only while observed `UE=0`. Final frame,
enable, TEACK/REACK and NVIC enable/priority are checked. Ownership is recorded
before the first effect. Failed cleanup retains that marker and the generated
cold leases, closes admission and requires stop retry; a primary error alone
does not establish rollback.

Whole stop closes admission and drains accepted TX and wake publishers. It
proves source withdrawal, disables and joins the vector including pending and
active state, then proves `UE=0` and TEACK/REACK clear. Only then may it clear
FIFOEN/threshold fields and discard remaining RX hardware data. Missing proof
retains ownership. A successfully stopped predecessor performs no hardware
access when stopped again, including after a successor owns the controller.

Span checks reject aliases between requests, payloads, RX output/count, the
ring, selected owner/profile, active loans and controlled MMIO including the
known NVIC/system window. Wrapping spans and invalid output alignment reject
before writes. The caller still supplies live, accessible, correctly typed
storage and observes its lifetime obligations.

## Run finite concurrent acquisition

The external `nexus-examples` application `apps/uart_fifo_acquisition` uses the
public UART face to receive while a finite TX request remains borrowed. Its
caller-owned capture, service timestamps and request storage have fixed bounds.
Only acquire-observed SETTLED permits the next payload and sequence. Closing the
traffic window requests cancellation and continues drain; an expired drain or
broken clock retains storage. Platform cleanup starts only from cold or actually
closed traffic, retries BUSY/IO within an explicit attempt budget and never uses
a clock after Platform withdrawal. Cleanup exhaustion is not a successful join.

The optional platform-owned fixture at
`tests/contracts/uart_fifo_acquisition` links that external application against
the two exact FIFO references. It produces reference and instrumented images;
private provider includes remain inside the fixture. The instrumented image
wraps the selected IRQ dependency with one raw cycle probe. A station may supply
separately reviewed, bounded dispatch markers; no pin or marker is selected by
default. The ordinary reference excludes the wrapper and probe. Linked proof
must read the actual vector and follow its strong stub through the wrapper to
exactly one real provider dispatch with the selected state argument.

`tools/hil/uart_fifo.py` retains device/wire captures and declared conversion
parameters. It rejects inconsistent sessions, counts, lengths and source
identities. Four-column IRQ CSV yields only unverified candidate intervals,
without `samples.irq_latency`: an independent assertion source, dispatch
association, complete capture window and calibration still require review and
the existing SOC-55 admission. Raw DWT spans and wrapper duration cannot supply
unobserved IRQ arrival latency. No parser, model or link check establishes
physical execution.

## Assess cost and evidence separately

Only the selected owner adds a const-profile link to the ordinary UART prefix;
the profile contains four one-byte encodings/budgets. Ordinary byte state,
public operations and ordinary initialization remain unchanged. Exact object
alignment, Flash retention and instruction cost come from the selected ARM ELF,
not field-size estimates.

The IRQ bound counts RDR reads and TDR writes independently. Status reads,
source changes, barriers and wake delivery also cost CPU time. External models
check accesses and prove that a drained threshold batch does not rewrite
unchanged masks. Counts are not measured cycles, WCET or guaranteed throughput.
A separately admitted cycle probe only observes its declared platform; U575's
unreviewed DWT capability remains disabled.

From the repository root with pinned tools and dependencies prepared:

```sh
python tools/dev/dev.py configure --preset fixture-io-evolution-h743-uart-fifo
python tools/dev/dev.py build --preset fixture-io-evolution-h743-uart-fifo
python tools/dev/dev.py configure --preset fixture-io-evolution-u575-uart-fifo
python tools/dev/dev.py build --preset fixture-io-evolution-u575-uart-fifo
```

Successful normal builds must produce the ELF/BIN/map, linked-resource and
provider-retention reports, with one selected face and its actual FIFO table.
These commands do not flash hardware or start UART traffic. Product protocol
and traffic generation remain external.

Current development validation passed 90 real-provider GoogleTest/GoogleMock
cases, 45 per family, and 18 normal-TOML/generated-factory cases, 9 per family:
108 cases with no failures or skips. Models use the pinned official LL threshold
functions and separate FIFO, RDR and shifter state. Both canonical configurations
also passed normal ARM configure/build, resource budgets and retention checks
for one face, one operations table and seven method slots per image.

The separate RTSCTS development validation passed 76 real-provider cases,
38 per family, and 22 normal-TOML/generated-factory cases, 11 per family, with
no failures or skips. Both exact four-pad normal configurations also passed
ARM configure/build. These results establish software behavior and linking for
the stated reference selections, not peer response, physical handoff, timing
or a clean reproducible candidate. The existing flow-none arithmetic comparison
below is not a measured cost comparison for the new RTSCTS selection.

For these two exact `Os` FIFO fixtures, narrowing only bounded intermediate
arithmetic removed the linked software division helpers. H743 BIN/loaded Flash
changed from 9216 to 8372 bytes, saving 844 bytes; U575 changed from 8948
to 8016 bytes, saving 932 bytes. Each image's allocated/reserved RAM stayed
2296 bytes, including
its 2048-byte main-stack reservation. Absolute deadlines remain 64-bit. These
are measured artifact sizes for this selected arithmetic comparison, not a
zero-cost FIFO claim, a comparison with ordinary IRQ-byte mode, measured cycles
or WCET. They do not establish a clean reproducible software candidate.

Physical HIL is `not_executed`. Neither model nor link results establish
oscillator accuracy, peer compatibility, U575 noise immunity, physical latency
or silicon erratum qualification. RM0433 Rev9 section 48.5.4/table 402 and
RM0456 Rev7 section 66.5.5/table 674 define the reviewed FIFO shapes; other chips
and modes need their own exact review.

See the [public UART contract](../../io/include/nexus/io/uart.h),
[shared private algorithm](../../io/private/uart_fifo_impl.h),
[H743 specialization](../../soc/stm32/stm32h743/drivers/uart_fifo.c),
[U575 specialization](../../soc/stm32/stm32u575/drivers/uart_fifo.c),
[H743 RTSCTS specialization](../../soc/stm32/stm32h743/drivers/uart_rtscts.c),
[U575 RTSCTS specialization](../../soc/stm32/stm32u575/drivers/uart_rtscts.c),
[finite U575 DMA selection](u575-uart-gpdma.md) and
[current qualification boundaries](../delivery/qualification.md).
