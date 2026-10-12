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
routes, selected production sources and software fixtures. STM32G474RET6 remains
an explicit GPIO/LPUART vertical slice. The selected non-M4 references are
STM32H743ZIT6 / NUCLEO-H743ZI2 and STM32U575ZIT6Q / NUCLEO-U575ZI-Q. They add
startup, fixed-clock admission, monotonic time, GPIO and IRQ-byte UART only.
CPU ABI/runtime support for other Cortex-M profiles does not imply a full SoC,
Board or peripheral implementation.

| Reference | Maintained execution contract | Explicitly excluded |
|---|---|---|
| G474 / MB1367 | HSE24 / 170 MHz, TIM2, PA5 GPIO, LPUART1 PA2/PA3; baremetal or FreeRTOS | Other controllers, DMA and ThreadX |
| H743 / NUCLEO-H743ZI2 | Bounded pre-RAM LDO entry, HSI64, Cache off, TIM2, PB0 GPIO, USART3 PD8/PD9; baremetal or FreeRTOS | Other supply/clock profiles, DMA/cache ownership, optional RAM placement, live-state handoff |
| U575 / NUCLEO-U575ZI-Q | Externally established TZEN=0 and SWAP_BANK=0, MSIS4 MHz, Cache off, TIM2, PC7 GPIO, USART1 PA9/PA10; baremetal or FreeRTOS | TZEN=1, option-byte changes, security attribution, DMA, optional RAM placement, live-state handoff |

These are source-reviewed Board references. Their physical PCB revisions,
silicon revisions, solder bridges, electrical connections and option bytes have
not been observed. Reference-only HIL admission fails closed; selecting a
matching chip name cannot promote a reference into a qualified physical Board.
The [execution record](../delivery/soc-refinement-verification.md) distinguishes
normal commit gates, development ARM observations and final qualification.

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
DMA1/DMA2, BDMA and MDMA separately; MDMA reachability to TCM does not authorize a
driver, cache protocol or linker placement. U575 GPDMA/LPDMA facts likewise do not
implement linked-list engines or security-domain admission. Legacy `dma` facts
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
| 28 | Exact H743 HSI64 and U575 MSIS4 MHz references; no alternate oscillator/PLL profile or electrical clock qualification. |
| 29 | G474 remains GPIO/LPUART. Additional controller modes are not implemented. |
| 30 | Exact H743ZI and U575ZIQ inventories and the routes above. Other packages/controllers/routes require their own facts, ownership, models and budgets. |
| 31 | M7/H743 and M33/U575 startup/time/GPIO/UART references. A complete peripheral set and actual MCU execution are not established. |
| 32 | Per-engine reachability facts only. H7/U5 DMA providers, cache ownership and security-domain transfers are not implemented. |
| 33 | Unknown running state is rejected. Bootloader-to-platform clock, IRQ, DMA, cache and RAM handoff is not implemented. |
| 34 | Fixed boot profiles only. No running frequency transition, consumer reclocking or deadline-domain conversion. |
| 35 | Existing F407/F470 shallow Sleep only. No deep-STOP continuous RTC/LPTIM domain or wake restoration. |
| 36 | U575 single-state reference and existing privileged scalar broker only. No U575 userspace SVC/MPU/TrustZone transport or isolation qualification. |

Implementing rejection of an unsupported configuration is a valid defensive
contract; it is not implementation of the rejected feature. Additional features
need exact controller/mode ownership and acceptance evidence before these ledger
entries can close. Current first-release devices retain their declared fixed
profiles.

Physical equipment is currently disconnected. SOC-37–55 have executable tooling
and station-binding obligations; their physical exit status remains
`not_executed`. See [HIL authoring and commands](../../tools/hil/README.md).
