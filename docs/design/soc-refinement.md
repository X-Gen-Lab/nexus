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
an explicit GPIO/LPUART vertical slice. CPU ABI/runtime support for other
Cortex-M profiles does not imply a full SoC, Board or peripheral implementation.

Clock bootstrap uses finite poll work bounds before the final clock exists;
polls are not measured elapsed milliseconds. Cleanup must distinguish remaining
recovery obligations from conservative settings allowed to remain. The monotonic
timer belongs to one successfully started boot domain, with an overflow-service
limit of 2^32 microseconds. Old deadlines cannot be carried across stop/restart.
NMI/HardFault, paused timers, dynamic PLL changes and deep STOP require their
own exact contract rather than assumptions from ordinary task execution.

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

SOC-28–36 require actual target/profile selections, independently reviewed
hardware facts and dedicated acceptance evidence. Additional G474 modes,
non-M4 SoCs, complex DMA/cache/security domains, running-state handoff, dynamic
clocking, deep STOP and userspace transport are not inferred from existing
interfaces. Their ledger entries remain conditional until the selected scope is
implemented and validated. Current first-release devices remain supported under
their declared fixed-profile contracts.

Physical equipment is currently disconnected. SOC-37–55 have executable tooling
and station-binding obligations; their physical exit status remains
`not_executed`. See [HIL authoring and commands](../../tools/hil/README.md).
