# SoC execution budgets

The authored assembly and immutable resolved IR select exact controller modes,
capacities and clock profiles. This document explains how to review their
execution costs; it is not another capability registry or configuration input.
The [derived capability matrix](../delivery/capabilities.md) owns the maintained
selection projection. Product scheduling and overload policy stay external.

## Work, time and completion

A finite work count is not a measured time bound. Bootstrap loops use explicit
poll limits because the monotonic clock may not yet exist. Runtime absolute
deadlines belong to the current boot clock domain. IRQ delay, DMA arbitration,
bus stalls, notification latency and service intervals require measurements for
the exact firmware and hardware under load.

Each selected mode must document its maximum IRQ work, task service work,
critical-section scope, caller storage and terminal predicate. A drain deadline
limits the period before quarantine; it never authorizes releasing borrowed
memory. The executor must keep calling service/recovery until hardware, IRQ,
queues and observers actually withdraw their references.

## Current mechanisms

| Mode family | Bounded work and progress | Final ownership condition |
|---|---|---|
| GPIO | Direct authorized register command; toggle requires serialized writers | External direct writers joined before pin release |
| UART IRQ bytes/events | At most one TX and one RX hardware byte per IRQ; reads copy one ring item per short critical section | Actual wire TC or reviewed reset abort, TX IRQ withdrawal and captured wake join |
| UART IRQ blocks | One hardware byte per IRQ; block acquisition/release is explicit | Last prefix published, hardware sources withdrawn and every consumer loan returned |
| UART finite TX DMA | One selected channel's terminal facts per IRQ; task service observes DMA and UART independently | DMA engine/source off and actual UART wire completion before SETTLED |
| UART RX DMA idle/continuous and duplex | Selected bank/IDLE/error facts; caller supplies finite buffers/blocks and replacement capacity | Memory engine stopped and stable prefix/bank proven; RX loans and independent TX drain retained |
| SPI short polling | Per-call finite length and absolute deadline; no IRQ mode fallback | Both controller and endpoint retain ownership for the complete CS interval |
| SPI finite full-duplex DMA | Two selected engines; task service checks memory and shifter facts | Both DMA sources withdrawn, wire idle, then CS release and SETTLED |
| I2C polling | Finite message/byte count; reviewed ACK windows and bounded STOP work | STOP/master/line state satisfies the exact implementation contract |
| I2C IRQ/DMA messages | Shared V1 IRQ consumes at most two DATA bytes; DMA observes its selected direction | Protocol STOP, wire/master state and DMA/source withdrawal all succeed |
| ADC polling/trigger blocks | At most the configured scan per task-side rearm; finite block capacity | Trigger and conversion stopped, DMA actually disabled, then publication/reclaim |
| ADC continuous trigger blocks | Double-bank terminal facts and explicit replacement; no overwrite of borrowed blocks | Closed producer and DMA sources; all outstanding block loans returned |
| EXTI | Visit only selected line states in deterministic generated order; one queue fact per pending line | This line masked/acknowledged, active shared vector joined, wake withdrawn |
| PWM/advanced PWM | Fixed authorized channel/period operations and latched break state | Reviewed inactive output before timer ownership release |
| Timer capture | Finite selected channels and visible capture/overflow facts | Capture IRQ/source withdrawal before event storage release |
| Classic CAN | Three TX mailboxes; at most three initially queued frames per each of two RX FIFOs | Per-mailbox abort/terminal facts, INAK/RX drain and publisher join |
| USB FS device PIO | At most eight RX headers and 128 payload words per IRQ; progress scans selected endpoint count | Endpoint request drain, FIFO/source withdrawal, publisher join and clock release |
| Raw Ethernet/MDIO | Task progress bounded by selected descriptor counts; MDIO has an absolute deadline | Descriptor/DMA ownership withdrawn, IRQ publisher joined and every RX loan returned |
| Internal Flash | Finite reviewed physical operation and absolute deadline; geometry is chip-specific | Hardware busy cleared or explicit retained recovery responsibility |
| Watchdog | Finite enable/feed operation; enabling is irreversible | Stop never fabricates release of an active hardware watchdog |

The exact implementations and configured limits remain authoritative:

- UART: `soc/*/*/drivers/uart*.c`; G474 and U575 use IRQ bytes. H743 RX also uses
  IRQ bytes; its explicitly selected finite TX DMA has a separate memory/Cache
  and drain contract. The F407, G474, H743 and U575 wire-drain allowance is
  `20000000 / baud + 100` microseconds. GD IRQ uses its reviewed reset-abort
  protocol; it is not assigned the ST wire-drain formula.
- SPI: `soc/*/*/drivers/spi*.c`; the finite DMA drain allowance is 200 microseconds.
- I2C: `soc/common/i2c_v1_async.c` and the SoC wrappers; V1 async drain allowance
  is 1,000 microseconds. Polling STOP has separate finite work.
- ADC/EXTI/timer: selected SoC driver and private state; scan, queue and channel
  limits come from resolved inputs or explicit caller storage.
- CAN/USB/Ethernet: `io/src/controllers/can_controller.c`, `usb_device_controller.c` and
  `eth_mac.c`; poll/drain limits and selected capacities are validated at cold
  admission. IRQ notification remains a hint, never a replacement for predicates.

## Cost review and evidence

Keep the exact resolved configuration, compiler identity, flags, linked ELF/map,
resource budget and retention outputs together. Compare equivalent CPU/ABI,
backend, clock, memory layout and capacities. A source instruction count,
object-file size or different-capacity image cannot establish an equivalent
whole-image saving. Report Flash, static RAM, reservations and caller storage
separately. Disassembly is real tool output, not a cycle measurement.

`tools/measurement/soc_costs.py` observes actual linked functions and ELF resource
costs with complete raw command receipts and input/tool hashes. Its comparison
rejects changed routes, bindings, claims, backend or capacity profiles rather than
silently attributing their cost to an implementation improvement. The empty versus
selected and baremetal versus FreeRTOS dimensions remain useful selection-cost
reports, but are not same-profile regression comparisons. The
[SoC execution record](../delivery/soc-refinement-verification.md) identifies the
source scope of completed observations and pending final baselines.

For changed interrupt/concurrency behavior, GoogleTest/GoogleMock calls the real
production source with external register/CPU ports modeled. Record actual RED,
then GREEN; retain admission, loss ordering, cancel, quarantine, wake and instance
isolation regressions. ARM budget/retention verifies actual linking. Software
checks do not certify physical IRQ latency or DMA throughput.

No numerical maximum service interval is asserted without the selected producer
rate, storage, hardware and interference budget. External products must qualify
that interval and the resulting overload policy. A missed service deadline cannot
revoke an accepted request or block loan. Current physical measurements remain
not executed until an actual station records them.
