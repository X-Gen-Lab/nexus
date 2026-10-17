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
RX, non-8N1 frames, flow control and low-power/autonomous operation are outside
this selection.

These full inputs use the existing Board routes. The FIFO subsection belongs
to the same authored assembly and typed IR as all other resources.

```{literalinclude} ../../cmake/package/verification/io_evolution/h743-uart-fifo.toml
:language: toml
```

```{literalinclude} ../../cmake/package/verification/io_evolution/u575-uart-fifo.toml
:language: toml
```

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
read afresh. Overrun and a full software ring record aggregate loss while
preserving older queued good bytes. `read_bytes` may return copied bytes with
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
[H743 specialization](../../soc/stm32h743/drivers/uart_fifo.c),
[U575 specialization](../../soc/stm32u575/drivers/uart_fifo.c),
[finite U575 DMA selection](u575-uart-gpdma.md) and
[current qualification boundaries](../delivery/qualification.md).
