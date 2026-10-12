# SoC refinement and physical acceptance

Nexus maintains exact silicon mechanisms. SoC facts and routes, private provider
implementations, generated static instances, Board connections and observed
physical behavior have separate responsibilities. The existing typed factory and
shared readonly operations tables remain the runtime interface; no new registry,
worker, allocator, lock or product configuration authority is introduced.

The [55-item execution ledger](soc-refinement-execution.csv) separates existing
correction and optimization (SOC-01–27), conditional target extensions
(SOC-28–36), and physical acceptance (SOC-37–55). Codex owns all maintenance.
Implementation status, executed software evidence and physical status are recorded
independently. An authored plan, a synthetic parser test or an ARM link cannot
complete a physical exit condition.

## Current software boundaries

STM32F407VET6/ZGT6 and GD32F470ZGT6 maintain reviewed fixed clock, startup,
monotonic timer and selected controller modes. The
[derived capability matrix](../delivery/capabilities.md) enumerates exact facts,
routes, selected production sources and software fixtures. STM32G474RET6 maintains
GPIO/LPUART plus selected EXTI edge events. The non-M4 references are
STM32H743ZIT6 / NUCLEO-H743ZI2 and STM32U575ZIT6Q / NUCLEO-U575ZI-Q. H743 adds
fixed clock/cache profiles and finite UART DMA transmission. U575 keeps its
single-state platform and an independent synchronous dual-world software
reference separate. CPU ABI/runtime support for other Cortex-M profiles does
not imply a full SoC, Board or peripheral implementation.

| Reference | Maintained execution contract | Explicitly excluded |
|---|---|---|
| G474 / MB1367 | HSE 24 / 170 MHz, TIM2, PA5 GPIO, LPUART1 PA2/PA3, selected PA0/PA1 EXTI edge events; baremetal or FreeRTOS | Other controllers, DMA and ThreadX; EXTI fixture wiring is not an observed Board route |
| H743 / NUCLEO-H743ZI2 | Bounded pre-RAM LDO entry; HSI 32/64 MHz with Cache off/on; TIM2, PB0 GPIO, USART3 PD8/PD9 IRQ bytes or finite DMA TX; baremetal or FreeRTOS | Other supply/PLL profiles, DMA RX/other engines, optional RAM placement, running-state handoff and runtime reclocking |
| U575 / NUCLEO-U575ZI-Q, platform | Externally established TZEN=0 and SWAP_BANK=0, MSIS 4 MHz, Cache off, TIM2, PC7 GPIO, USART1 PA9/PA10; baremetal or FreeRTOS | Option-byte changes, DMA, optional RAM placement and running-state handoff; this assembly has no TrustZone transport |
| U575, separate dual-world reference | Explicit TZEN=1 cold-owner contract, Secure/NS Runtime images, SAU/MPU/RAM attribution, actual CMSE entry and copied synchronous GPIO access from one unprivileged NS Thread | General dual-world factory, NS RTOS/task identities, asynchronous loans, cross-domain DMA, product loader authentication and physical isolation qualification |

These are source-reviewed Board references. Their physical PCB revisions,
silicon revisions, solder bridges, electrical connections and option bytes have
not been observed. Reference-only HIL admission fails closed; selecting a
matching chip name cannot promote a reference into a qualified physical Board.
The [extension execution record](../delivery/soc-extensions-verification.md)
distinguishes normal commit gates, development ARM observations and final
qualification. The [earlier baseline](../delivery/soc-refinement-verification.md)
retains its original source identity and evidence.
Reference software status in the ledger records bounded host/ARM evidence; it
does not establish a clean candidate, physical acceptance or every original
exit condition.

The [G474 contract](stm32g474.md) describes independent EXTI0/PA0 and EXTI1/PA1
rings, pending-bit acknowledgement, mux/gate ownership and source withdrawal.
Its test Board is an explicit software fixture, with no inferred connector,
button or electrical bias. The [H743 contract](../../soc/stm32h743/README.md)
defines all four HSI 32/64 MHz and Cache off/on combinations. Fixed cold profile
selection does not implement a running frequency transition.

H743 USART3 DMA TX owns DMA1 Stream0, DMAMUX1 Channel0 request 46 and its separate
vector; RX remains IRQ bytes. Cache-on buffers occupy exclusively owned complete
32-byte lines in selected AXI SRAM, with no outward rounding or hidden bounce
buffer. Cache cleaning follows borrow admission, runs outside the long interrupt
mask, and cannot settle storage while preparation is active. DMA completion,
wire TC and source withdrawal remain distinct. Failed withdrawal quarantines the
loan; the provider must drain before cache and timer shutdown.

The [U575 dual-world contract](u575-trustzone-contract.md) is an independent
no-OS integration fixture. Its cold owner observes provisioned option bytes,
checks actual physical permissions, clears future shared RAM, then configures
and seals attribution. A real SG gateway authenticates the NS world rather than
an untrusted task identity, checks CMSE permissions and copies a bounded request
through the existing GPIO broker. The layout and GPIO grant belong to the
reference fixture, not to reusable platform policy. No option-byte programming,
RTOS isolation, general factory integration or asynchronous transport is claimed.

Clock bootstrap uses finite poll work bounds before the final clock exists;
polls are not measured elapsed milliseconds. Cleanup must distinguish remaining
recovery obligations from conservative settings allowed to remain. The monotonic
timer belongs to one successfully started boot domain, with an overflow-service
limit of 2^32 microseconds. Old deadlines cannot be carried across stop/restart.
NMI/HardFault, paused timers, dynamic PLL changes and deep STOP require their
own exact contract rather than assumptions from ordinary task execution.

Cold admission rejects observable foreign selected owners before global clock
mutation. Generated pin admission observes all required GPIO gates and validates
all selected pin modes and locks before capturing or changing any pin. This
includes controller routes and hidden endpoint CS pins; an unrelated shared-bank
pin does not become owned. An initialization failure includes its partially
initialized object in reverse cleanup. Failed readback preserves exactly the
remaining cleanup responsibility instead of returning a successful release.

The timer start transaction clears stale UIF, observes a fresh UG update, clears
that flag and verifies CNT before publishing the epoch. Runtime overflow advances
the epoch only after acknowledged UIF. A refused ACK invalidates public time,
records the private timer fault and attempts finite source/clocked-counter/NVIC
withdrawal. This loss of the deadline domain is always terminal, including
successful source isolation: the CPU preserves a terminal interrupt mask,
invokes the existing bounded fault observer and parks if that observer returns.
No invalid zero timestamp resumes a deadline consumer, and no request or
low-power loan is released. Products may provide separately reviewed
non-returning recovery. Ordinary stop/restart and cleanup retry belong to normal
lifecycle control; they do not resume this terminal path. A snapshot rechecks
epoch validity under the same saved interrupt mask used to read both counter
words.

`.noinit` means startup neither copies nor clears those bytes. It does not prove
retention through a particular reset, power event or sleep mode. CPU reachability,
DMA reachability, linker placement and physical retention are separate facts.
Factory access does not initialize or reacquire any of these resources.

IRQ and DMA routines have finite work contracts. The selected execution owner
must continue required service/cancel/drain progress. DMA completion, wire
completion and withdrawal of every request reference precede SETTLED. Failed
withdrawal retains QUARANTINED storage; cancellation is not permission to reclaim
it. Continuous block providers stop on exhausted replacement storage and never
write consumer-owned blocks or invent exact missed-event counts.

Public typed I/O contracts belong under `io/include`; cross-chip controller
algorithms have private headers and implementation targets. SoC wrappers own
silicon layout/bit assertions and physical IRQ/DMA/clock bindings. Shared code
placement follows those dependencies. CMake owns source selection, and resolver
facts own configuration identity; moving a file does not create a second graph.

## Implemented correction and optimization

SOC-01–27 retain their original exit conditions. The following groups identify
the implementation, without extending a software result to physical behavior.
Per-item status and evidence entry points remain in the CSV.

| Items | Mechanism and review boundary |
|---|---|
| 01–05 | Current scope documentation, derived capability consistency, exact-package GPIO inventory, configuration input closure and versioned primary source/errata index. The source index is not a complete silicon-revision workaround qualification. |
| 06–07 | Immutable controller and family descriptors unify configuration identities, provider dispatch and required SDK payload. CMake still exclusively owns production source dependencies. |
| 08–16 | Explicit pre-RAM/post-RAM ownership, foreign-owner rejection, bounded clock transactions and rollback, observed timer start/ACK, context restrictions and boot-domain validity. |
| 17–19 | Fixed-profile shallow Sleep/tickless contracts, shared USB48 ownership and `.noinit` semantics; no implicit deep-STOP or retention guarantee. |
| 20–22 | G474 UART comparison wrap and one-byte masked reads, selected-line GD EXTI dispatch, observed EXTI withdrawal and documented IRQ/service/drain work. |
| 23, 27 | Real ELF/disassembly observation, tool/input receipts and strict comparable-profile cost review. Flash, static RAM, reserved stack and caller storage are distinct quantities; instruction counts are not measured cycles. |
| 24–25 | Explicit F407 CCM placement and GD RAM initializer validation. All copy/clear spans are validated before the first write; unselected domains cost no default storage and `.noinit` remains untouched. |
| 26 | Public I/O, private cross-chip controller algorithms and SoC SDK assertions retain their dependency boundaries. No forwarding-only runtime layer or generic mutable registry is added. |

F407 CCM is CPU-only and rejected as a DMA buffer. H743 memory facts describe
DMA1/DMA2, BDMA and MDMA separately. The implemented USART3 DMA1 TX lane admits
selected AXI SRAM only; MDMA reachability to TCM does not authorize a driver,
cache protocol or linker placement. U575 GPDMA/LPDMA facts do not implement
linked-list engines or security-crossing DMA. Its synchronous CPU gateway does
not authorize DMA buffers or asynchronous storage loans. Legacy `dma` facts
and explicit `dma_engines` are alternative authored forms; contradictory inputs
are rejected rather than repaired.

SDK identity includes reviewed original CMSIS Device files, their licenses,
upstream revisions and the selected family payload. Verification checks bytes,
file sets and observed filesystem identity across its read window. A successful
check is not an atomic filesystem snapshot; actual consumers must use the same
immutable SDK. Independent installed consumers must exercise the public platform
interface without inheriting private vendor include paths.

Hot-path observations retain actual ELF bytes, resolved selection, compile flags,
tool identities, resources and complete command receipts. A changed Board route,
CPU/ABI, backend, clock, memory domain or capacity makes a regression comparison
incomparable. Empty-to-selected image growth is reported as a selection cost,
not as an equivalent-profile performance improvement. Physical cycles, maximum
interrupt blackout and worst-case service intervals remain unmeasured.

## Physical preparation and execution

`tools/hil/soc_acceptance.py plan` prepares all nineteen physical obligations
without touching equipment. Each case contains required hardware, stimulation,
raw observations, assertions, safety actions, metric units, unreviewed null
limits and an exit condition. Selection gaps and unsupported OS cases remain
explicit. The result is always `prepared_unbound` / `not_executed`.

A real station supplies observed PCB revision, part/UID, probe, supply, wiring,
exact ELF, resolved configuration, recomputed resources and reviewed workload.
Existing `admit.py` establishes that admission. A station-authored SoC protocol
binds one case, selected controller IDs, required instruments/calibration bytes,
stimulation and numeric limits. Invariant failure counts must remain exactly zero;
actual work counts/durations require a positive reviewed lower bound. Nothing in
the repository supplies product acceptance numbers or fabricates pin wiring.

`capture` delegates to the maintained acquisition runner. The existing six
operations retain exact tool/input identities, exclusive station ownership,
whole-argument argv without a shell, timeout/output bounds, cleanup and quarantine.
The serial/instrument adapter must receive and content-pin the SoC protocol and
all instrument records. This runner does not implement automatic waveform,
CAN/USB/Ethernet peers or power interruption; station-owned adapters provide those
operations. A successful capture is only `executed_unqualified`.

`validate` rebuilds unchanged admission and recipe, verifies actual operation
records, protocol, instrument bytes, session, firmware/configuration, acquisition
context, raw observations and sample files. It derives every result from finite,
contiguous nonempty samples against the reviewed limits. Rehashed opaque
protocols, null limits, missing/failed/skipped operations, stale records, changed
bytes and wrong scopes fail closed. A result qualifies only the named case,
selected modes and recorded conditions; it does not establish WCET, unobserved
loads, certification or product release.

Flash power-loss cases additionally require a content-pinned external product
layout. The test region must use the exact part's erase boundaries, remain within
physical Flash, avoid every protected region and avoid all actual firmware LOAD
bytes. The external layout must protect every admitted firmware LOAD. Products
own the region, protected data, power-cut safety and recovery policy. The platform
reserves no default test partition and performs no destructive action during
preparation, validation or commit checks.

## Extension decisions

SOC-28–36 keep their original exit conditions. Choosing H743 and U575 resolves
the first non-M4 target decision; it does not complete every conditional feature.

| Item | Delivered subset or remaining implementation |
|---|---|
| 28 | H743 HSI 32/64 MHz with Cache off/on, plus the U575 MSIS 4 MHz reference. Other oscillator/PLL/supply profiles and electrical qualification remain outside the implemented subset. |
| 29 | G474 GPIO/LPUART and selected PA0/PA1 EXTI edge events, including generated ownership and software fixtures. Other peripheral modes and physical edge acceptance remain open. |
| 30 | Exact H743ZI/U575ZIQ inventories and finite routes, plus the G474 EXTI fixture. Other packages/controllers/routes require their own facts, ownership, models and budgets; actual Board wiring remains unobserved. |
| 31 | Selected M7/H743 and M33/U575 startup/time/GPIO/UART and backend references. A complete peripheral set, every OS mode and actual MCU execution are not established. |
| 32 | H743 fixed cache ownership and finite USART3 DMA1 TX, plus U575 CPU-domain permission/attribution mechanisms. DMA RX/block streams, other H7 routes/engines, U5 DMA and security-crossing DMA remain unimplemented; physical coherency is unverified. |
| 33 | Not implemented. Unknown running state is rejected; bootloader-to-platform clock, IRQ, DMA, cache, RAM and live-owner handoff is absent. The U575 cold world transition does not implement this feature. |
| 34 | Not implemented. H743's four profiles are cold configurations, with no running frequency transition, consumer reclocking or deadline-domain conversion. |
| 35 | Not implemented. Existing F407/F470 shallow Sleep does not supply a deep-STOP continuous RTC/LPTIM domain, retention proof or wake restoration. |
| 36 | Partial synchronous U575 reference only: Secure/NS images, cold attribution owner, single unprivileged NS Thread and copied GPIO gateway. No dual-world platform factory, NS RTOS, multitask identity, mutable address space, asynchronous borrow/drain or HIL isolation qualification. |

Implementing rejection of an unsupported configuration is a valid defensive
contract; it is not implementation of the rejected feature. Additional features
need exact controller/mode ownership and acceptance evidence before these ledger
entries can close. Current first-release devices retain their declared fixed
profiles.

The remaining software work for SOC-33–35 requires coordinated ownership rather
than another profile label. Running-state handoff needs an exact outgoing and
incoming image/resource contract, validated stack/vector/FP/IRQ/cache/RAM state,
and a reviewed terminal transfer. External products retain loader authentication,
partition and recovery policy. Dynamic frequency changes need actual consumer
quiescence, ordered electrical changes, bounded rollback, OS timing adaptation
and an explicit continuous time contract. Deep STOP additionally needs an exact
RTC/LPTIM reference, wake/retention admission, elapsed-time reconstruction and
consumer restoration; ordinary shallow-sleep preparation does not establish it.

SOC-36 still needs a separately reviewed dual-world configuration/factory path,
trusted OS task identity and mutable-map/copy-fault contracts before general
userspace support. Async and DMA extensions must preserve borrow, cancel, drain
and final-publication rules across the security boundary. These are additional
mechanisms, not consequences of the current synchronous GPIO reference.

Physical equipment is currently disconnected. SOC-37–55 have executable tooling
and station-binding obligations; their physical exit status remains
`not_executed`. See [HIL authoring and commands](../../tools/hil/README.md).
