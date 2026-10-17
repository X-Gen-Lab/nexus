---
id: design.u575-uart-gpdma
type: explanation
audience: consumer
source_language: en
---

(u575-uart-gpdma)=

# Finite USART1 GPDMA TX on STM32U575

Select this mode when an exact STM32U575ZIT6Q TZEN0 assembly needs finite byte
transmission through USART1 while retaining independent IRQ-byte reception.
The implementation uses one static UART owner, caller-owned requests and TX
buffers, and the pinned official LL register operations. Product tasks, wait
policy, protocol framing and recovery policy remain external. Physical HIL is
`not_executed`.

## Select the exact assembly

![FIG-16: current generated faces, private providers and caller-owned requests; this selection has one UART owner](../assets/diagrams/io-evolution.svg)

**FIG-16 shows current shared construction and runtime ownership. It is not a
U575 DMA timing diagram or hardware qualification.** For this selection, the
generated face is `nx_uart_port_t`, its shared table is
`nx_stm32u575_uart_gpdma_ops`, and its private owner embeds one UART prefix plus
selected GPDMA metadata. The arrows retain their generation, lookup and call
meanings. No second UART face or hidden DMA worker owns the same controller.
Ordinary IRQ-byte UART state and operations gain no fields.

| Fact | Maintained selection |
| --- | --- |
| Chip and execution | STM32U575ZIT6Q, TZEN0 baremetal reference, existing MSIS/PCLK4MHz profile |
| UART | USART1, PA9/PA10 AF7,8N1, FIFO disabled, baud62..250000 |
| TX engine | GPDMA1 channel0, request25, IRQ29 |
| RX and UART IRQ | Independent caller-selected byte ring, USART1 IRQ61 |
| IRQ policy | Both vectors priority5; validate each selected kernel policy |
| TX count |1..65535bytes, matching the16bit BNDT field |
| TX source | Entire nonwrapping span in nonsecure SRAM1 `[0x20000000,0x20030000)` |
| Current Board input | Source-reviewed NUCLEO-U575ZI-Q reference; physical wiring remains unqualified |

The normal fixture supplies the existing Board package and UART mode. Physical
engine, channel, request and vector facts come from the exact SoC descriptor;
there is no competing authored DMA register configuration.

```{literalinclude} ../../cmake/package/verification/io_evolution/u575-uart-gpdma.toml
:language: toml
```

The last SRAM1 byte is valid for a one-byte transfer. SRAM2/3/4, Secure aliases
and LPDMA-only memory are outside this selected source domain. The provider
rejects wrapping, oversized or unreachable payloads and overlap with its known
state, immutable route facts, RX ring and controlled MMIO before admission.
RX read outputs and count storage cannot overwrite those controls or an active
request/payload. These checks do not replace the caller's obligation to supply
live, correctly typed storage.

## Execute through the existing UART face

1. Call platform start from the privileged task owner. Check its primary error,
   cleanup error and remaining effects. A failed partial constructor can retain
   ownership; keep its storage and cold leases live while retrying stop.
2. Obtain the static face through
   `nx_factory_uart(NX_UART_ID_CONSOLE)`. Lookup is pure identity and may happen
   before startup; hardware operations require successful platform startup.
3. Initialize a fresh request once with `nx_request_initialize`, then prepare
   it with `nx_uart_tx_prepare`. The deadline is absolute in the live
   `nx_time_now_us` domain. Keep the entire payload immutable while borrowed.
4. Submit through `nx_uart_port_submit`. Rejection retains no request, payload
   or new hardware effects. `NX_SUCCESS` means ACCEPTED, including a subsequent
   hardware start failure that the same owner must drain. An explicit adapter
   may instead pass its existing QUEUED loan through
   `nx_uart_port_start_admitted`.
5. The same task calls `nx_uart_port_service` to advance deadline observation
   and drain. IRQs only latch bounded physical facts and issue an optional wake
   hint. A wait adapter is optional; it must validate both IRQ policies before
   using kernel notifications. A wake hint never replaces the request level.
6. Acquire-observe `SETTLED` before reading the result or reusing the request
   and payload. Adapter observers must also join before their storage is
   reused. Ordinary RX reads remain available during DMA TX.

No scheduler, worker, allocation or runtime name registry is created by lookup,
startup, submission or IRQ dispatch. One privileged task serializes controls;
cross-task cancellation uses the established external slot/epoch adapter rather
than sharing a mutable request pointer.

## Separate DMA, wire and reference completion

![FIG-06: current request states retain the borrow through drain and quarantine until final release-publication](../assets/diagrams/request-lifecycle.svg)

**FIG-06 describes the current shared request contract, not a promise that a
timeout can stop hardware.** Its optional QUEUED path belongs to an explicit
adapter. In this provider, cancellation and deadline observation retain the
same request and source until hardware and reference withdrawal is proved.

| Observation | Provider action and release condition |
| --- | --- |
| DMA `TCF` | Latch block completion; still retain the request and source. Prove disabled/idle engine with no FIFO data before relying on USART wire state. |
| Cancellation, deadline or DMA error | Retain the first terminal cause, begin bounded drain and preserve the earliest existing drain bound. No buffer revocation follows from a timeout. |
| Active channel withdrawal | Request `SUSP`; observe fresh hardware `SUSPF` and `IDLEF`; issue legal `RESET` and read back disabled/idle state. Writing zero to `EN` does not disable GPDMA. |
| Suspension IRQ | Withdraw `SUSPIE` with readback and preserve the actual `SUSPF` needed by the later reset proof. Cached flags cannot authorize reset. |
| Independent USART `TC` | The final stop bit is complete; source/IRQ/publisher withdrawal is still required before settlement. |
| All required proof joined | Clear the active reference, then release-publish `SETTLED` as the final provider request access. |
| Proof still missing when the drain bound expires | Keep `QUARANTINED` storage borrowed and expose continued owner recovery responsibility. |

Normal block completion does not require a reset on every request when the
engine is already proved disabled, idle and empty. Cancellation of an active
channel requires the suspension/reset sequence above. Old completion and
suspension flags are drained before a new `EN`; errors remain latched and an
old `TCF` cannot start the next request's wire-drain budget. DMA errors arriving
while flags are acknowledged cannot turn a failed request into success.

Task service withdraws DMA/USART TX interrupt sources and checks their
readback, disabled/not-pending/inactive DMA vector state, active UART IRQ and
captured wake publishers. Only after DMA cannot load another TDR byte does it
independently observe USART `TC`. IRQ-byte RX remains independent of TX DMA.
An error or aborted request reports zero guaranteed transferred bytes; a
successful request reports its full submitted byte count.

## Preserve the USART request gate through recovery

ES0499 Rev12 section2.21.2 describes lost subsequent DMA requests when `DMAT`
is cleared and set again. This mode uses the documented persistent-gate
workaround: install `DMAT=1` before the first `UE` enable and retain it through
success, cancellation, errors, channel reset and rearm. Per-request recovery
does not cycle the receiver's `UE`.

Whole stop closes admission and retains `DMAT` until the outstanding request,
interrupt sources, both vectors and wake publishers have joined and
`UE=0`, `TEACK=0` and `REACK=0` are observed. Only then may it clear `DMAT` and
withdraw its added GPDMA clock. Partial initialization records effects before
their first write and retains unproved cleanup. Repeated stop on a cold or
successfully stopped owner performs no hardware access, including after a
successor starts on the same controller.

If service or stop cannot prove withdrawal, keep the request, payload, ring,
wake target and selected cold owner live. Continue bounded service/stop through
that same owner. A product decides how to handle a permanently unresponsive
peripheral; this platform does not invent a successful release or automatically
restart protocol traffic.

## Build and assess the evidence

From the repository root with the pinned tools and dependencies prepared, run:

```sh
python tools/dev/dev.py configure --preset fixture-io-evolution-u575-uart-gpdma
python tools/dev/dev.py build --preset fixture-io-evolution-u575-uart-gpdma
```

The maintained input is the TOML above. Successful commands produce
`io_evolution_fixture.elf`, `.bin`, `.map`, `.resources.json` and
`provider-retention.json` under the preset's `bin` directory. Check successful
resource validation and retention of one selected UART face with the real
GPDMA operations table. These commands build software and do not flash a board
or execute physical HIL. Application TX execution requires the explicit task
flow above; the linked smoke fixture only establishes its declared link scope.

Current development evidence includes 40 real-provider GoogleTest/GoogleMock
cases, 8 normal-TOML/generated-factory cases, and the normal ARM configure/build
with linked resource and provider-retention checks. All recorded model cases
passed with no skips. Model execution establishes the tested software behavior;
ARM artifacts establish linking/resources. Neither establishes clean-source
full-gate candidate status, measured cycles, oscillator accuracy, electrical
behavior or physical erratum qualification. Physical HIL is `not_executed`.

The source uses the already pinned official LL DMA and USART headers for the
selected register operations. It adds no vendor HAL handle, HAL tick, vendor
IRQ callback, linked-list descriptor or second transfer owner. USART2 DMA, RX
DMA, continuous streams, autonomous/low-power DMA and a TrustZone broker remain
outside this implementation. The general Cortex-M33 architecture and other
chip DMA capabilities do not extend this exact provider scope.

See the [U575 source map](../../soc/stm32u575/README.md),
[private provider contract](../../soc/stm32u575/private/stm32u575_uart_gpdma.h),
[real provider](../../soc/stm32u575/drivers/uart_dma.c),
[public UART ownership contract](../../io/include/nexus/io/uart.h) and
[current qualification boundaries](../delivery/qualification.md).
