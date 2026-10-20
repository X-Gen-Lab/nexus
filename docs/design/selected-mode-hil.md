---
id: design.selected-mode-hil
type: reference
audience: maintainer
source_language: en
---

(selected-mode-hil)=

# Exact selected-mode HIL preparation

The five contracts below prepare independent instrument acquisition for software
that already has exact selected providers. Every preparation result remains
`prepared_unbound` / `not_executed`. No board, probe, serial peer or calibrated
instrument is connected. Parser unit tests use synthetic bytes and establish
software contract behavior only.

| Case | Exact selected configuration | Separate observations |
| --- | --- | --- |
| `H743-SPI-CACHE` | STM32H743ZIT6 V `0x2003`, DEV `0x450`, HSI64/default trim, baremetal, SPI4 PE12/13/14 AF5 and PE11 CS; cache on; DMA1 streams 2/3, requests 83/84 | Whole exclusive 32-byte AXI lines, TX clean/RX prepare/invalidate, both DMA completions, wire idle, CS release, withdrawal and settlement |
| `H743-RX-CONTINUOUS` | Same exact H743 silicon/clock/backend; USART3 PD8/PD9, DMA1 stream 1/request 45, continuous RX; cache off or on | Actual CT/M0AR/M1AR/NDTR rotations, equal caller blocks, held contents, service deadlines, loss, stop and loan return; TX remains IRQ |
| `H743-DUPLEX-CONTINUOUS` | Same exact H743 silicon/clock/backend and USART3 pins; separate DMA1 stream 0/request 46 TX and stream 1/request 45 RX; cache off or on | Independent finite TX request and continuous RX stream loans, real DMA/wire TC, CPU acquire-visible bytes, lane-local EN refusal and joint-domain quarantine |
| `U575-TX-CHAIN` | STM32U575ZIT6Q, TZEN0/SWAP_BANK0, MSIS 4 MHz, baremetal; USART1 PA9/PA10; GPDMA1 channel 0/request 25; IRQ RX | Finite contiguous SRAM1 payload, two packed 12-byte nodes, one 64 KiB CLBAR window, LAST termination, wire TC and complete withdrawal |
| `U575-LPTIM-RUN` | Same U575 preboot/clock/backend; internal LPTIM2, PCLK1/4, 1 MHz RUN ticks, IRQ68 priority5/calls_os=false | 2..65536 ticks, actual enable/disable synchronization, full-period UE, deadline, quarantine and withdrawal |

## Preparation and admission

The ordinary `workloads.py` and `soc_acceptance.py` catalogs contain these cases.
Their selectors use exact canonical resolved controller IDs, mode, clock,
backend, physical DMA lanes and bounded descriptor contracts. A chip family name
or another UART instance does not select a case. Native, FreeRTOS and ThreadX do
not inherit these baremetal mode contracts. The continuous RX contract does not
select finite IDLE RX or DMA duplex mode.

The maintained software fixtures are:

- `tools/hil/fixtures/stm32h743_hsi_selected_reference.json`;
- `tools/hil/fixtures/stm32u575_nucleo_zi_q.json` for the USART1 Board identity;
- `tools/hil/fixtures/stm32u575_lptim_selected_reference.json`.

The two additional reference Board/part pairs are explicitly recorded in the
admission guard. Removing `scope`, changing the Board name or changing a part
cannot promote them. `validate_station` continues to reject software-reference
fixtures. The earlier H743/U575 GPIO and IRQ-byte smoke fixture limits remain in
force. New mode preparation does not widen a smoke qualification.

SPI-only and pinless LPTIM assemblies have no selected smoke console. Their plans
state `independent_control_channel_unbound`; the descriptive future UART route
does not create a UART controller. Instrument identities and numeric limits
remain unset until an actual station is reviewed. A future physical station
needs a separately reviewed fixture and corresponding tooling change before the
existing acquisition runner can operate it.

```sh
python tools/hil/soc_acceptance.py plan \
  --fixture tools/hil/fixtures/stm32h743_hsi_selected_reference.json \
  --resolved build/h743-spi-cache/generated/resolved.json \
  --output station/selected-mode-plan.json

python tools/hil/soc_acceptance.py template --case H743-SPI-CACHE \
  --fixture tools/hil/fixtures/stm32h743_hsi_selected_reference.json \
  --resolved build/h743-spi-cache/generated/resolved.json \
  --output station/selected-mode-template.json
```

The first command produces an unbound plan. The second produces an incomplete
authoring template. Neither command flashes firmware, starts an adapter, supplies
station facts or accepts the template as a reviewed protocol.

## Independent raw observations

Each case has its own reviewed protocol and stimulus inventory. Four separately
retained JSON exports form its observation contract:

| Role | Required content |
| --- | --- |
| `mode_registers` | Controller/mode and source/divider/trim/cache/security readback, chip identity, `nominal_core_hz`, IRQ policy and exact DMA lanes |
| `wire_capture` | Per-trial retained transmitted/received bytes or timer-tick observations |
| `memory_capture` | Per-trial caller addresses/capacities/exclusive ownership, retained payload snapshots, descriptor links, and held-block snapshots for RX |
| `lifecycle_events` | Per-trial acceptance, absolute monotonic deadline/domain/epoch, strictly ordered timestamps and lifecycle events; RX includes actual target-register snapshots |

Every export uses schema 1 `selected_mode_observation` and binds its case, role,
fresh capture session, reviewed protocol hash, context hash, exact controller ID
and a reviewed instrument. A missing role, duplicate trial, wrong session,
changed file, different selected instance or altered clock/cache fact fails.
The per-case contract rejects unknown effects and access after settlement.
There is no declared `pass` field that substitutes for observations.

Register, memory and lifecycle exports bind the reviewed debug probe. Wire
exports bind the logic analyzer, or the calibrated timer for LPTIM. Merely naming
a different instrument from the same protocol does not satisfy a role. U575
revision IDs must be observed as actual 16-bit integers; the current provider has
no selected revision allow-list, so the parser records the value without
inventing a revision gate. H743 retains its exact V `0x2003` gate.

`nominal_core_hz` describes the configured clock tree. It does not measure HSI or
MSIS accuracy in SI hertz. Event `at_ns`, deadlines and service samples use a
nanosecond conversion of the same live target Core monotonic domain, named
`monotonic1` by this export contract. They are not automatically calibrated SI
nanoseconds. An external instrument must retain its actual frequency,
calibration, uncertainty and mapping to that domain. Frequency error and absolute
physical latency remain separate reviewed measurements; an oscillator need not
run at mathematically exact 64 MHz or 4 MHz.

The existing `soc_acceptance.capture` path reuses the exclusive station lock,
bounded adapter subprocesses, firmware/readback identity, timeout and cleanup
recording. Validation rebuilds admission, verifies the executed argv and input
closure, matches the adapter's actual manifest and verifies raw identities before
the selected-mode parser runs. Standalone parser success carries no physical
status. Final case metrics must equal the values derived from parsed trials.

Instruments and station adapters remain external station-owned integrations.
The platform does not ship a universal SPI peer, waveform decoder, debug-probe
capture driver or automatically connected serial device. Production stimulus
firmware must use the exact admitted assembly and its public typed interfaces;
changing it requires a new ELF/configuration/resource identity and capture.

## Stimulus and boundary obligations

SPI cache DMA exercises 32-byte and 65504-byte transfers, late cancellation near
TC, late withdrawal timeout, misaligned storage and invalid length. Both borrowed
buffers are disjoint exclusive complete cache lines inside AXI SRAM. TX DMA TC
and RX DMA TC precede observed wire idle; RX invalidation and CS release precede
successful settlement. A withdrawal-timeout trial deliberately reaches wire
completion but fails later reference withdrawal: it must remain quarantined past
its deadline and cannot publish settlement or release ownership. It does not
qualify arbitrary DMA hangs.

Cancellation cases retain the actual cancel call before wire/stream withdrawal;
a scenario label alone cannot establish that stimulus. Rejected cases retain
verified byte-identical before/after memory, zero observed wire activity, no
transmitted/received payload and no borrow. Referencing a missing snapshot or
changing rejected storage fails validation.
The duplex contract applies rejection to the rejected direction: an already
active RX stream continues during TX-overlap rejection. A rejected RX start does
not submit a TX request.

Continuous RX exercises wrap, held-block backpressure, IDLE stop, service-budget
expiry, cancellation with a held block, overlapping blocks and undersized blocks.
At least three equal disjoint caller AXI blocks are required. Each accepted trial
retains an initial target snapshot and at least three CT transitions during its
observation warm-up. Snapshots include CR, M0AR, M1AR, NDTR, held addresses and
explicitly returned addresses. Held blocks cannot appear in either armed target.
Every transition is timestamped in the same domain and meets the conservative
minimum receive interval computed from observed NDTR progress and the real
ten-bit 8N1 frame, with two 1 us quanta for sample uncertainty. This calculation is
separate from the intentionally conservative eight-bit service guard below.
When CT flips, the newly active target must remain unchanged; only the just
completed inactive target may be replaced, and that completed address must be
the newly held loan. Independent held snapshots must remain byte-identical.

The replacement budget uses the actual captured block capacity, sampled NDTR and
selected BRR, rather than a requested byte count supplied independently:

```text
budget_us = max(0, floor((remaining + capacity - 20) * 8 * BRR / 64) - 2)
replace_by_ns = sampled_ns + 1000 * budget_us
```

TIM2 and USART must share the selected nominal 64 MHz HSI domain. The 20-byte guard
and two 1 us quanta are conservative bounds. Late service and backpressure report
loss and stop; they cannot silently continue with a held buffer. Cache-on blocks
use complete exclusive 32-byte lines. Cache-off observations cannot inherit
cache-on maintenance events. A new stream after stop needs a fresh explicit
submission; the protocol does not assert an automatic restart policy.

### Independent H743 duplex case

`H743-DUPLEX-CONTINUOUS` has its own case ID, protocol, stimulus, session and raw
inventory. It cannot reuse a passed `H743-RX-CONTINUOUS` capture. Both cache
profiles require the same nine production-reachable scenarios:

- `tx_min`: 1 byte with caches off, or one 32-byte line with caches on;
- `tx_max`: 65535 bytes off, or 65504 bytes on;
- `cancel_tx_tc`: actual cancel after observed DMA TC and before wire TC;
- `stop_rx_held`: producer detaches while a consumer still holds a block;
- `tx_drain_timeout`: TX EN withdrawal refuses after DMA and wire completion;
- `rx_drain_timeout`: RX EN withdrawal refuses and `stop_rx` returns `BUSY`;
- `domain_taint`: observed HSIDIV change, retained loans and three-IRQ isolation;
- `reject_tx_overlap`: TX storage aliases the already bound RX pool;
- `reject_rx_overlap`: overlapping RX blocks are rejected without TX submission.

The RX acquisition window contains an initial DMA target sample and at least
three genuine CT transitions. It demonstrates overlap with the beginning of TX;
it does not claim continuous RX for the entire approximately 5.69-second maximum
TX. An actual station must retain the exact observed interval. Normal TX can
settle while RX remains active. The finite request's `SETTLED` state never names
the RX stream or joins an independent adapter observer.

`memory_capture` separates three equal AXI RX blocks from one independently sized
AXI TX payload. Cache-on payloads occupy exclusive complete 32-byte lines;
nonempty request lengths also cover complete lines. The caller's static owner,
stream, three-slot reservation and request controls use the selected AAPCS32
minimum sizes 272, 40, 120 and 32 bytes and 8-byte alignment. Larger reservations
are allowed. Controls are disjoint from payloads and from each other; nested
members are not separate reservations. Control sizes need not be cache-line
multiples. This reference workload observes `wake_address=0` and attaches no
external wake object.

TX before/after bytes must remain unchanged. RX `rx_before` is an initial poison
pattern; `rx_after` is the debug-probe view of physical DMA memory, and
`rx_cpu_visible` is a separate capture of the bytes read by the CPU after stream
acquire. Physical RAM alone does not prove cache coherence. The RX observation
binds address, slot epoch, length, completion, invalidation, publication, acquire
and sample times to actual target and slot-state snapshots. Held before/after
bytes bind the same consumer address and epoch throughout the retained interval.
Fault scenarios sample the consumer still held after failed stop or isolation,
rather than an older block already returned before the fault.

TX `snapshots` retain actual Core request state, the independent `preparing`
flag, active request address/ref, publisher state, DMA CR/NDTR/PAR/M0AR/FCR,
DMAMUX CCR and USART CR1/CR3/ISR plus DMA LISR. Core states remain `READY`,
`ACTIVE`, `DRAINING`, `QUARANTINED` and `SETTLED`; `PREPARING` and `REJECTED`
are not invented Core states. A cold pre-clean baseline may have zero PAR,
M0AR and NDTR. It must remain unchanged during preparation, with EN and DMAT off.
Launch uses TDR `0x40004828`, request 46, finite byte MINC TX and the exact V
TRBUFF policy. RX uses RDR `0x40004824`, request 45 and the selected byte DBM
configuration. Unknown modes, widths, directions and lane substitutions fail.

DMA TC requires actual NDTR zero and TCIF0. A fresh wire TC and successful
withdrawal both precede settlement; DMA TC alone releases nothing. A delayed
software DMA sample may follow the physical wire edge. The cancellation trial
deliberately samples DMA TC before cancellation and wire TC. Non-success
`transferred=0` does not imply zero transmitted bytes: these post-DMA-TC cancel
and EN-refusal trials require the full independent wire payload. Only the taint
trial permits a captured source prefix within its pre-fault observation window.
TX settlement requires EN, DMA interrupt sources including HTIE, DMAT and UART TX
sources withdrawn, flags cleared, the publisher joined and active pointer/ref
removed. It does not disable RX or the shared USART NVIC line.

The finite TX drain deadline starts at the actual drain entry, with the provider
budget `floor(20000000 / baud) + 100` microseconds. A failed withdrawal retains
the request through actual expiry. RX has no equivalent provider TX deadline:
its `BUSY` return, still-enabled EN readback, withdrawn RX sources, retained
producer loans and expiration of a separately declared station stop budget are
recorded independently. Slot snapshots retain producer loans/pending, each slot's
state and epoch, stream outstanding count and stopping state. A consumer can keep
`BORROWED` outstanding after producer loans reach zero. `BUSY` never revokes it.

Joint taint records actual RCC CR/HSICFGR, CCR and clock-epoch observations.
The last valid Core-domain boundary, injected fault, first software observation
of domain loss and completed isolation are separate facts. The access trace is
scoped to USART3, DMA1 and DMAMUX: after software observes domain loss it permits
no further access to that invalid I/O domain, cache maintenance or publication.
RCC/SCB detection and NVIC isolation remain permitted. This is not a guarantee of
zero instructions between an external RCC write and its later software
detection. Three IRQs, DMA1 stream 0, stream 1 and USART3, must be disabled and
their publishers inactive; both directions' loans remain quarantined. Domain
loss does not itself set the stream's normal `stopping` flag.

After clock invalidation, the held-byte and pure-memory request tail snapshots
use independently retained capture sequence numbers around fault, observation
and isolation. They do not invent an elapsed Core deadline in the dead domain.
All ordinary RX rotations and service samples precede the boundary and still
satisfy the existing eight-bit replacement guard and ten-bit wire lower bound.

Cancellation during the synchronous cache-clean call remains a host/model
white-box regression using GoogleMock substitution. Production cache clean has
no callback, and cancel belongs to the same single executor. Such reentry is not
a tenth physical HIL scenario; no worker, ISR cancellation or new hook is added
to make it appear reachable.

The finite U575 chain exercises 1, 65535, 65536, 131070 and 131071 bytes, cancellation
near TC, late withdrawal timeout, an invalid descriptor window and invalid
length. The theoretical public count limit is 196605 bytes, but the actual disjoint SRAM1
payload must also leave space for generated nodes, state, ring and control
storage. The multi-block trial therefore requires more than 131070 bytes and
validates actual placement rather than demanding an impossible simultaneous
full 192 KiB payload and private nodes. `three_block_threshold` names the 131071-byte
stimulus; it does not claim a physical maximum. Nodes are packed at offsets 0 and 12
from a 32-byte-aligned base and remain in one 64 KiB window. Capture the actual packed
`block`, `source` and `link` words, initial CLBAR/CLLR/CSAR/CBR1 and each hardware
fetch snapshot. A one-block transfer leaves both pool entries zero and has
initial `CLLR=0`. Two-block transfers use one fetched node; three-block transfers
use two. The actual UB1/USA/ULL update bits and LA offsets must match the source
progression and final zero link. Successful withdrawal clears channel references
and all node words. A failed withdrawal retains legal 32-bit references and the
published nodes. Chain completion alone does not replace DMA/IRQ withdrawal and
wire TC.

LPTIM exercises 2-tick and 65536-tick boundaries, early ARRM, synchronization delay,
late withdrawal timeout and one-tick rejection with no effects. Enable/disable
write/readback/DSB anchors must demonstrate the required synchronization interval.
The delayed case must retain a readback delay beyond the ordinary synchronization
interval. UE cannot precede the full requested nominal-domain period. This is a
fixed-clock RUN contract. It establishes no RTC, backup, VBAT, autonomous, Stop or TrustZone behavior.

## Delivery status and further execution

The software delivery includes selectors, unbound plans, strict reviewed
protocols, existing-runner integration, raw parsing and negative parser tests.
No physical station has been bound and no case has executed on hardware. Keep
the original raw capture, adapter stdout/stderr, receipt and firmware/resource
identities when hardware becomes available. Record the exact successful case,
mode, silicon and conditions; a selected-mode result cannot qualify another
controller, cache profile, backend, Board or product.
