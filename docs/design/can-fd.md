---
id: design.can-fd
type: explanation
audience: maintainer
source_language: en
---

(can-fd)=

# CAN FD with fixed physical RAM and explicit request ownership

Use this contract to assemble and execute the exact STM32G474RET6 FDCAN1
software reference. It offers `iso-fd-normal` for external communication and
`iso-fd-loopback` for internal silent loopback, both at nominal 500 kbit/s and
data 2 Mbit/s from HSE 24 MHz. These are separately selected software modes.
Neither mode establishes a populated transceiver, measured clock accuracy,
external ACK, termination or physical timing qualification. HIL remains
`not_executed`.

## Select a separate static interface

![FIG-16: current build-time assembly and runtime calls; CAN FD selects one controller face and one private owner](../assets/diagrams/io-evolution.svg)

**FIG-16 describes shared current mechanisms, not a CAN bus timing diagram.**
Here the selected face is `nx_can_fd_port_t`, the compact lookup is
`nx_factory_can_fd`, and the runtime provider owns FDCAN1 and its fixed message
RAM. Generation arrows, lookup arrows and runtime calls keep their original
meanings. Lookup neither reads hardware nor starts a transmission.

The [normal fixture](../../cmake/package/verification/io_evolution/g474-can-fd-normal.toml)
and [loopback fixture](../../cmake/package/verification/io_evolution/g474-can-fd-loopback.toml)
use the same software Board and physical claims. The strict resolver selects
one mode, validates exact rates, masks, ring capacity and IRQ policy, then
publishes immutable typed IR and one generated bundle. Conflicting controller,
shared FDCAN configuration, message RAM, vector and pad claims are rejected.
The Board records silicon PA11/PA12 AF9 routing; its physical PCB revision,
oscillator population and external PHY are unknown.

| Owner | Responsibility |
| --- | --- |
| Platform | Start HSE24/PLL170 and TIM2, acquire the shared FDCAN gate and pads, construct domain then controller, and retain unfinished outer leases |
| Private domain | Validate fixed physical RAM and CKDIV0, reject a live peer, and retain the domain until every acquired controller stops |
| FDCAN provider | Own the three physical transmit slots, fixed filters, two receive FIFOs, value ring and line0 publishers |
| Application executor | Serialize privileged unmasked task calls, supply service progress and deadlines, and join external observers |
| Caller/generated storage | Keep accepted requests, immutable filter plan, ring and wake sink live through their stated release conditions |

The classic [CAN interface](classic-can.md) remains separate. A classic face
never becomes a CAN FD face, and its frame, operations table and default state
do not gain 64-byte payload or FDCAN fields. The optional CAN FD face has two
pointers and eight shared methods: 8 B and 32 B respectively on a 32-bit ABI.
Selected private state, requests and the caller-sized event ring are separate
costs. These structural sizes are not measured cycles, latency or stack bounds.
There is no allocator, software transmit FIFO, global object pool or hidden
worker.

## Exact hardware selection and units

| Fact | Fixed selection and boundary |
| --- | --- |
| Target | STM32G474RET6, Cortex-M4F, baremetal, FDCAN1 only |
| Clock | System/APB 170 MHz; FDCAN uses HSE 24 MHz, CCIPR.FDCANSEL0 and CKDIV=0 / divide by 1 |
| Nominal phase | BRP=3, SEG1=12, SEG2=3, SJW=2; 500 kbit/s, 81.25% sample point; NBTP `0x02020B02` |
| Data phase | BRP=1, SEG1=9, SEG2=2, SJW=2; 2 Mbit/s, 83.33% sample point; DBTP `0x00000811` |
| Normal mode | ISO FD enabled, BRS permitted per frame, DAR1; TEST/MON/ASM/LBCK0 |
| Loopback mode | Same format/timing with TEST1, MON1 and LBCK1; no external ACK qualification |
| Transmit values | DATA only; classic 0..8 B or FD 0..8/12/16/20/24/32/48/64 B; lengths never round up |
| Physical queue | Three 18-word elements; hardware identifier priority, not software admission order |
| Filters | Immutable mask lists: at most 28 standard or 8 extended entries, each targeting FIFO0 or FIFO1; unmatched and remote frames rejected |
| Receive | Two blocking FIFOs, three 18-word elements each; copied events preserve raw DLC, ESI, filter and FIFO facts |
| Time | TSS0, raw hardware timestamp0 and `NX_CAN_FD_TIMESTAMP_UNAVAILABLE`; `acquired_us` is monotonic acquisition time |
| Interrupts | Reserve IRQ21/22 cold; only IT0/IRQ21 is enabled, at logical priority5; ILS0 and ILE.EINT0 |
| Pads | PA11 RX/PA12 TX AF9, with TX latch prepared recessive before alternate-function connection |

The FDCAN kernel is not the 170 MHz APB clock and does not use the timer clock
doubling rule. Arbitrary timing tuples, automatic clock changes, additional
controllers, remote transmit and other operating modes are not selected.
Clock tolerance, propagation delay, oscillator accuracy and sampling margins
require separate physical validation.

The G4 reduced M_CAN integration has a fixed 212-word / 848 B instance region at
`SRAMCAN_BASE` (`0x4000A400`). A generated domain points to this physical lease;
arbitrary caller SRAM cannot substitute for message RAM.

| Section | Byte offset | Capacity |
| --- | ---: | --- |
| Standard filters | 0x000 | 28 words |
| Extended filters | 0x070 | 8 two-word filters |
| RX FIFO0 | 0x0B0 | 3 elements of 18 words |
| RX FIFO1 | 0x188 | 3 elements of 18 words |
| TX event FIFO | 0x260 | 3 two-word elements; frame requests do not select event storage |
| TX queue | 0x278 | 3 elements of 18 words, ending before `0x350` |

Initialization clears only this instance region. It does not reset a live
FDCAN2/3 peer or disable the shared gate. H7's configurable message RAM uses a
different integration; these register and RAM offsets do not establish H7
support.

## Prepare, admit and prove physical completion

Initialize request metadata once, then use `nx_can_fd_tx_prepare` to copy a whole
frame and an absolute deadline in the selected monotonic domain. Preparation
retains no frame pointer. Submission from the sole task executor validates the
whole value, physical queue index, deadline and storage disjointness before
admission. A rejected call leaves the request unborrowed and performs no
transmit RAM writes or add request.

ACCEPTED borrows the complete request until acquire-observed SETTLED. The
provider packs the exact DLC and payload into the selected physical element,
then publishes TXBAR after a barrier. No software queue changes the hardware
identifier arbitration order. Existing requests cannot become another call's
output or wake storage. Public face/table identities, private controls, peer
registers, shared RAM and SCS/NVIC controls are also protected from output aliases.

![FIG-06: current request lifecycle; cancellation and quarantine retain the request until physical drain precedes SETTLED](../assets/diagrams/request-lifecycle.svg)

**FIG-06 supplies the shared ownership states.** ACCEPTED and REJECTED are
admission results. In this provider, source withdrawal means both TXBAR and
TXBRP are clear for the slot, followed by a fresh TXBTO or TXBCF observation.
An empty slot alone, a latched interrupt alone or a stale prior completion
cannot release the request. A hardware add scan may keep TXBAR set while TXBRP
still reads zero.

TXBTO proves successful transmission and reports the actual payload byte count.
It can win a cancellation race, even when TXBCF is also set. TXBCF without TXBTO
proves completed cancellation or an unsuccessful DAR attempt; an explicit
cancel reports CANCELLED, and an unsuccessful ordinary attempt reports IO with
zero transferred bytes. A missing external ACK does not trigger software replay.
Bus retry and protocol recovery policy belong to the external product.

Cancel, timeout, bus-off and stop request physical cancellation. They never
revoke the existing loan. A failed drain deadline leaves the request
QUARANTINED, the slot retained and stop/recover/domain release blocked. Later
physical proof may still permit settlement. The executor must continue bounded
service; a wake hint does not supply execution progress.

## Receive, recover and close the owner

An IRQ or explicit service call processes at most three transmit slots and
the initial three elements in each receive FIFO. Copied receive values retain
ESI as observed transmitter state. FD DLC15 means 64 B; classic DLC9..15
normalizes to 8 B with an explicit flag. The event's acquisition time is not SOF,
and a disabled hardware timestamp is never promoted to physical bus time.

The caller-sized event ring does not overwrite older records. Software loss
counts and hardware-loss lower bounds remain distinct. Pending loss follows
older records and precedes newly retained frames. Bus-off, error counters and
last-error state are latched separately from ring fullness. Only actual enabled
line0 imposes a syscall ceiling on wake binding; disabled reserved line1 does
not become an additional kernel publisher. A rejected binding preserves the
earlier sink.

Explicit recovery requires every transmit loan and notification publisher to
drain. It reenters initialization without replay, shared reset or reassignment
of the message RAM lease. The immutable mode stays normal or loopback across
recovery. Bus-off recovery timing still depends on actual controller and bus
progress; returning from configuration does not measure the required bus idle.

Stop closes admission, drains slots, disables peripheral sources, joins line0
and only then enters acknowledged INIT. A final PSR/ECR snapshot precedes CCE,
which can destroy hardware outcome facts. Successful controller stop ends its
request/ring/wake loans; any failure preserves incomplete leases for retry.
Platform then releases the domain and restores outer pad/gate/system ownership.
External adapter observers still need their own join before storage reuse.

## Sources and software qualification

The authoritative API is [can_fd.h](../../io/include/nexus/io/can_fd.h), with
the shared [request contract](../../core/include/nexus/core/request.h).
[The private provider](../../soc/stm32/stm32g474/drivers/fdcan.c) uses pinned official
FDCAN definitions and direct volatile MMIO. It does not link HAL driver C,
`HAL_GetTick`, HAL handles or MSP callbacks into this implementation. The
[G4 source lock](../../dependencies/stm32g4-hal.lock.json) preserves the exact
selected headers, paired CMSIS identities and original licenses.

Reviewed hardware references are RM0440 Rev9, DS12288 Rev6 and ES0430 Rev9.
RM0440 defines the fixed message RAM, queue and cancellation proof, including
unsuccessful DAR attempts. Its TEST.RX bit is read-only pin state: configuration
readback checks owned LBCK/TX controls and must allow a recessive RX bit.
EFBI0 and the absence of mixed dedicated/FIFO transmission avoid the selected
FDCAN errata conditions; they do not qualify every silicon limitation.

[Common contracts](../../tests/google/can_fd_contract_test.cpp),
[real provider register/event tests](../../tests/google/g474_fdcan_test.cpp)
and [both generated-factory selections](../../tests/google/g474_fdcan_factory_test.cpp)
exercise the actual C implementation. GoogleMock observes external MMIO/IRQ
effects. Models cover external ACK/no-ACK, cancellation races, fixed RAM,
read-only status, input/output isolation and retained shutdown. Generated tests
execute actual system startup with one shared RCC/TIM2 model, then the normal
TOML/factory path; they do not hand-set Platform readiness.

Actual execution and source identities belong to the
[delivery evidence](../delivery/README.md). Host models and any separately
recorded ARM links do not establish physical ACK, error injection, bus-off
recovery time, oscillator envelope, throughput, IRQ latency or WCET. Normal
mode still needs an inspected transceiver circuit, safe electrical connection
and independent HIL evidence before physical deployment qualification.
