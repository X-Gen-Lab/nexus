---
id: design.h743_flash_ram
type: explanation
audience: maintainer
source_language: en
---

(design-h743-flash-ram)=

# H743 Flash maintenance from a private RAM execution pod

This page specifies the **target IO-24 design** for synchronous program and
erase operations on the selected STM32H743ZIT6 V/cache-off software reference.
It explains the private execution boundary, retained ownership and the proof
required before returning to ordinary Flash code. This development batch has
connected selected-only RAM code, a generated workspace, the CPU bridge and the
MPU transaction to the ordinary provider. Actual production-ELF closure and the
complete software exit remain open. The figures retain their **target** identity
until their implementation and evidence exits are satisfied. Physical HIL is
`not_executed`.

The public Flash face still exposes physical geometry and synchronous operations.
The selected provider now requires cold RAM preparation; it has no fallback that
executes a physical pulse through the former ordinary Flash-resident path.
Existing image/control-alias guards, programming timing and clock ownership
remain prerequisites. Source integration or a successful link alone cannot
establish the entire IO-24 exit.

Flash admission requires V revision `0x2003` for every selected Flash profile,
including fixed HSI32/64 and HSI64/PLL384; unknown cuts are rejected. This
restriction and the selected RAM workspace do not extend to the other peripheral
providers or to assemblies without Flash. Their existing profile contracts
remain authoritative.

## Keep one controller owner and a cold factory

![FIG-17 target: static face lookup and explicit preparation are separate from the direct runtime call; the private RAM boundary owns CPU protection and Flash settlement](../assets/diagrams/h743-flash-ram-architecture.svg)

FIG-17 is the **target** architecture and its required ownership boundary; it does
not claim that all implementation and production-ELF exits have passed.
Solid arrows describe the explicitly labelled lookup, call or copy relationship.
The dashed external precondition is a caller responsibility, not a platform task.
The factory returns an existing immutable Flash face; it performs no MMIO,
allocation, execution-pod copy or maintenance. Once acquired, ordinary I/O enters
the selected private provider through the face's shared read-only operations.

| Owner | Responsibility in the target design |
| --- | --- |
| Generated Platform lifecycle | Select only the required private workspace and code; order preparation and reverse cleanup; retain failed cleanup responsibility |
| H743 Flash provider | Validate the physical operation, region, source and destination; stage command/input for the single execution owner |
| Private RAM owner and bridge | Prove copy/layout/context, install a private stack and RAM vectors, preserve CPU state and keep the entire busy transaction in RAM |
| Private MPU guard | Temporarily own the selector, selected descriptor and control changes; prove installation and exact restoration |
| Existing System | Own the monotonic TIM2 domain and epoch; RAM maintenance must preserve that contract |
| External maintenance caller | Serialize calls as the single execution owner; quiesce and join other bus masters and users, provide freshly erased consumer regions and choose watchdog/recovery policy |

There is no second Flash owner, runtime registry, heap, mandatory OS lock, hidden
worker or automatic watchdog feed. Application tasks, update policy, partitions,
authentication and reset policy remain in external repositories.

The [public Flash contract](../../io/include/nexus/io/flash.h) requires one
execution owner and no concurrent program/erase calls. The external caller owns
that scheduling; an active-state check does not provide a hidden concurrent lock.

## Prepare bytes and context before making Flash busy

Cold preparation must validate the selected load/run ranges, copy the audited
code/literals into admitted AXI SRAM and verify the resulting layout before
publishing readiness. Private vectors, stack, command, deadline operands,
workspace and terminal diagnostics must remain in admitted RAM throughout the
busy interval. Selected RAM cannot overlap submitted buffers, executable/load
bytes, vectors, control MMIO, current System state or other private metadata.
Half-open range checks must reject overflow and partial aliases before access.

Public reads, the program path's erased-region precheck and every programmed
flashword readback inspect the current `SNECCERR`/`DBECCERR` sticky flags in both
banks. An observed flag reports `IO`; these physical-data read checks do not
acknowledge it with W1C. This includes events already latched on entry: the
status cannot identify their source or establish recovery from a DBECC exception.
Corrected data may compare equal, so an observed event must be reported before
an all-FF input no-op or a subsequent pulse can hide it. This does not remove the
separate target-bank W1C phase in erase/pulse preparation.

The final call must re-observe actual privileged Thread context, masks, Cache
state and floating-point state; successful cold preparation alone does not prove
the later call context. Entry requires PRIMASK, BASEPRI and FAULTMASK to be zero;
an IRQ call or live lazy FP context is inadmissible. The deadline must be finite
in the current System TIM2 domain; `NX_DEADLINE_NEVER` is unsupported. The bridge
must preserve distinct MSP and PSP, CONTROL including FPCA, and incoming PRIMASK.
The assembly bridge changes only CPU stack/mask state: it establishes the private
MSP and calls the RAM transaction. That transaction publishes and reads back RAM
VTOR before the MPU guard can become active. After actual drain and protection
restoration it restores and reads back VTOR; only then can the bridge restore
the caller's exact CPU state. Every transitional NMI/HardFault path and exception
frame must have a proved owner and sufficient actual stack reserve.

The source/ELF closure must include actual instructions, veneers, literals,
direct and indirect calls, handlers, terminal paths and compiler-generated
helpers. A section name, C attribute, caller-authored `ram_safe` flag or unused
matching literal is insufficient. Firmware remains C11; test machinery does not
become a firmware dependency.

The normal firmware link for a selected H743 Flash assembly now invokes the
automatic RAM audit. It checks the complete copied pod and workspace against the
actual ELF sections and LOAD segments: the RX run range has one matching Flash
load range, their bytes agree, and the vector, stack and workspace extents are
disjoint from the pod and wholly in writable RAM. The immutable Flash image guard
must contain the actual linked endpoints and cover every file-backed LOAD. These
checks are part of the normal link, rather than only synthetic checker fixtures.
Changing a guard byte in a real firmware ELF is rejected by the same audit.

The audit also checks the actual CPU-bridge instructions, RAM direct control-flow
and literal residency, and a conservative private-stack bound based on emitted
compiler `.su` frame records and exception reserve. Its layout report separates
copied bytes, workspace bytes and their combined static RAM extent; alignment
outside those extents remains part of the ordinary image resource report. These
checks retain explicit C/ABI preconditions; arbitrary C data-pointer access, MMIO
effects and MPU/drain ordering still require provider models and source review.
They establish neither a complete formal execution-closure proof nor measured
stack, timing or physical behavior.

## Protect the CPU from Code-domain speculation

Cache-off is necessary for this reference, but it is not a branch-predictor-off
proof. PM0253 Rev6 describes speculative Normal-memory and branch-target reads;
CCR.BP is read-only and always one, and BPIALL is not implemented. DSB completes
prior explicit accesses; it does not disable later prediction. AN4838 Rev11
section 3.4 recommends using MPU memory attributes to prevent speculative
prefetch. The target therefore combines the audited explicit-access closure with
a temporary Code-domain memory-type guard.

The first admitted implementation requires the actual unified 16-region MPU.
An enabled incoming MPU, unsupported TYPE, or HFNMIENA without ENABLE is rejected.
Disabled CTRL values 0 and 4 are preserved exactly. **CTRL=0 does not prove that
all descriptors are disabled:** all 16 RASR.EN values must be observed. An enabled
foreign descriptor is rejected before policy writes. Scanning RNR is itself an
owned selector effect and must be rolled back on every returning path; a refused
rollback retains responsibility in RAM.

| Selected region 15 field | Target value |
| --- | --- |
| Base and half-open extent | 0; `[0, 0x20000000)` |
| SIZE / SRD / ENABLE | 28 / 0 / 1: 512 MiB, every subregion enabled |
| XN / AP | 1 / 001: no instructions; privileged read/write |
| TEX / C / B / S | 0 / 0 / 0 / 0: Strongly-ordered, implicitly shared |
| RASR | `0x11000039` |
| CTRL | 7: ENABLE, PRIVDEFENA and HFNMIENA |

XN prohibits Code-domain instruction fetch; Strongly-ordered replaces the Normal
memory type that permits speculative reads. AP001 permits the eight explicit
CPU stores needed to program one 32-byte flashword. AP000 would prevent those
stores and is therefore unsuitable for the unified program/erase path. AP001
also permits explicit reads: the independently audited RAM graph must exclude
them. This is not a blanket no-bus-access permission check.

The owner saves the exact incoming CTRL, RNR and selected RBAR/RASR before their
first possible effect. RBAR readback includes the selected region number; the
base field must be checked according to the real register semantics. Disabled
descriptor fields need not be zero and must survive restoration. Readback and
DSB/ISB must prove region/control installation before any program store or erase
START. The policy remains active in NMI/HardFault through HFNMIENA. No ordinary
Flash-resident Arch or vendor MPU helper may be called during this window.

## A deadline expires; a physical pulse does not cancel

![FIG-18 target: deadline expiry latches a result while the RAM transaction retains protection, waits for actual both-bank drain and restores state before return](../assets/diagrams/h743-flash-ram-lifecycle.svg)

FIG-18 shows protocol stages, not a new public request enum. Every arrow is a
guarded control-flow transition. The returning path requires the following
sequence; the terminal path has no edge back to ordinary code.

1. Reject invalid input or a deadline already expired before trigger. If any
   pre-trigger temporary selector/protection effect was acquired, prove rollback
   while both banks are idle. Every returning RAM path, including rejected vector
   publication and pre-trigger guard/deadline refusal, must pass the common RAM
   return gate: both banks idle, zero retained guard effects and the exact saved
   target control plus LOCK proved. Lock the idle target if necessary before
   restoring VTOR; an unproved condition enters RAM fail-stop.
2. Stage a validated command and input, enter the RAM bridge, install RAM vectors
   and prove the MPU guard before starting the physical operation.
3. Perform only the explicit validated program stores or erase START. Observe
   real BSY/QW/WBNE for both banks and retain all command/workspace ownership.
4. If the absolute deadline expires after trigger, latch `TIMEOUT` and continue
   actual RAM polling. Do not clear busy bits, withdraw protection, release
   storage or jump to a Flash error handler to manufacture settlement.
5. After both banks are actually idle, prove target LOCK, complete the required
   barrier and restore/read back MPU state. The RAM transaction must pass the
   same common return gate used for pre-trigger refusals, then restore/read back
   VTOR before the CPU bridge restores the exact caller context and returns to
   ordinary code.
6. Report the settled result. Hardware failure reports `IO`; otherwise a latched
   expiry reports `TIMEOUT`, and successful completion reports `SUCCESS`.

Permanently busy hardware never reaches the returning path. It remains in the
RAM drain path with protection and storage retained. Unprovable lock, protection,
vector or time-domain state likewise takes a nonreturning RAM fail-stop path.
The CPU-only bridge has no invented runtime readback or failure dispatcher:
its exact restoration must be proved against the actual ARM instructions and
admitted incoming context before software exit. A host watchdog may observe and
terminate its isolated test process; that is not device drain or product
recovery. This design promises no
hidden reset, watchdog service, RTOS progress or bounded wall-time return.

The selected deadline uses the existing TIM2 domain and epoch. Any wrap extension
must have an actual supported service/pulse bound; one pending UIF cannot count
an arbitrary number of masked wraps. A lost or exhausted epoch cannot resume
deadline consumers with a plausible zero. RTOS/SysTick compensation and product
time recovery require their own reviewed contract. Long masked Flash maintenance
does not inherit the short Arch critical-section latency contract.

## CPU protection does not quiesce the system

This MPU guard constrains the CPU. It neither stops DMA1/2, MDMA, Ethernet, USB,
display engines and debug access nor joins their late IRQ or publication paths.
Before calling maintenance, the external owner must withdraw and join every
master that can access the busy Flash domain, including descriptors and input
sources, or supply an independently proved disjoint plan. Address ranges alone
do not prove that a worker or bus master has stopped.

The same caller must choose how its product tolerates masked interrupts,
watchdog expiry, power failure and a nonreturning fault. The platform does not
select these policies or infer them from a Board reference. This design does
not admit Cache-on programming, enabled foreign MPU regions, bank swap,
option/OTP writes, every H7 cut, bootloader adoption or runtime clock changes.

## Keep cost and evidence tied to the actual bytes

Selected-only source and instance generation must keep unselected images free
of the RAM pod, workspace and calls. Actual selected/unselected ELF comparison
must separately report Flash LOAD/footprint, copied executable bytes, static RAM,
alignment padding and the real call/exception stack requirement. An array size
or host `sizeof` is not linked cost, measured stack use, throughput or WCET.
This page makes no measured cost, stack high-water, throughput or WCET claim.

| Evidence required before software exit | Boundary |
| --- | --- |
| Real GoogleTest/GoogleMock RED then GREEN | Actual provider/bridge boundary, failures, late drain and instance isolation; no mock implementation |
| Normal TOML/generated lifecycle and factory tests | One authored assembly, immutable IR, real selected-only constructor and retained cleanup |
| Actual emitted ARM firmware and closure report | Copied bytes, decoded calls/literals/register flow, guard values, no premature return, vectors/stack/time and image/control aliases |
| Selected and unselected ARM resource checks | Exact emitted source graph, retention and separate RAM/Flash costs |
| Clean SDK, full gates and candidate evidence | Exact source/configuration/dependencies/tools; no reuse of older commit results |
| Physical station execution | Real silicon/PCB/electrical conditions, speculative traffic, pulse timing, power loss, watchdog and interrupt behavior |

Synthetic checker fixtures establish only their checker behavior. Host register
models do not measure the real branch predictor, bus contention or IRQ latency.
If the checker cannot prove part of the production control flow, the missing
proof remains an open exit. The [IO-01–42 ledger](io-evolution-execution.csv)
remains the implementation-status authority; actual execution counts and source
identities belong in delivery evidence rather than this design contract.

The implementation is split between [cold preparation and admission](../../soc/stm32h743/drivers/flash_ram_owner.c),
the [CPU-only bridge](../../soc/stm32h743/drivers/flash_ram_enter.S),
[RAM vector publication](../../soc/stm32h743/drivers/flash_ram_transaction.c),
the [MPU transaction](../../soc/stm32h743/drivers/flash_mpu.c) and
[physical pulse/time observations](../../soc/stm32h743/drivers/flash_ram.c).
Their private workspace and checked C/assembly prefix are declared in
[the RAM contract](../../soc/stm32h743/private/stm32h743_flash_ram.h).
These links locate current work; they do not substitute for the required
execution and production-ELF evidence.

Primary references are PM0253 Rev6 sections 2.3 and 4.6–4.8, AN4838 Rev11
section 3.4, RM0433 Rev9's Flash maps/read-while-write rules and ES0392 Rev15
section 2.1.2. The source-identity record and selected provider evidence must
accompany the implementation review. See also the [H743 provider scope](../../soc/stm32h743/README.md),
[V-only clock contract](h743-high-clock.md), [I/O evolution](io-evolution.md),
[Arch contract](arch-contracts.md) and [qualification boundary](../delivery/qualification.md).
