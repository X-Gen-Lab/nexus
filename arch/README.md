# CPU-local primitives

`Nexus::Arch` exposes saved interrupt masks, exact exception identity, current
privilege, ordering barriers and an optional already-enabled DWT cycle snapshot.
It does not call I/O, OS or vendor SDK code. One compile-time Cortex-M primitive
implementation covers M0/M0+/M3/M4/M7/M23/M33/M55/M85, including ARMv8.1-M
Mainline compiler profiles. The maintained full SoC/platform targets remain
STM32F407 and GD32F470
Cortex-M4F. The separate CPU runtime assembly selects Core/Arch and optional OS
software for all nine cores without creating a SoC, Board, startup or factory.
See [CPU contracts](../docs/design/arch-contracts.md) and the
[runtime assembly example](../docs/delivery/cortex-m.md).

Save/restore tokens on the same CPU/thread in strict reverse order. Cortex-M
preserves incoming PRIMASK and does not overwrite BASEPRI/FAULTMASK where those
registers exist. Only privileged Task/configurable-IRQ callers may change masks.
NMI and HardFault remain unmasked and must not access this protected metadata.
Keep metadata sections short: no blocking, allocation,
large copies, kernel calls or user callbacks. A saved Arch token is not a kernel
critical-section token or an SMP lock.

Mask/context queries let blocking I/O reject contexts that prevent its progress.
The read-only mask snapshot distinguishes PRIMASK, BASEPRI and FAULTMASK; it is
not a restore token and does not freeze interrupt or security-world execution.
Baseline M0/M0+/M23 queries never read absent BASEPRI/FAULTMASK. Handler mode is
privileged even when CONTROL.nPRIV describes an interrupted unprivileged Thread.
An exception number identifies the current CPU/security state; it does not grant
permission to call a kernel ISR API or coordinate another security world.
DMB/DSB/ISB encode different ordering guarantees; they do not prove peripheral
or DMA quiescence. DWT snapshots require an explicit reviewed CPU capability and
privileged access. A disabled capability emits no DWT reads. Snapshots do not
enable or reset the counter and are not monotonic deadlines. Their wrap/frequency
limits are documented in the header. Reviewed capabilities are explicit CPU
facts, never inferred by probing arbitrary registers or enabling optional units.

`sleep.h` exposes two explicit shallow-sleep operations. The masked
`nx_arch_wait_for_interrupt()` requires privileged Thread mode, PRIMASK=1,
BASEPRI/FAULTMASK clear and SCR.SLEEPDEEP clear. It executes DSB/WFI/ISB without
changing masks, clocks or pending interrupts. The caller owns the final readiness
check and an enabled IRQ wake source; a pending enabled IRQ wakes WFI despite
PRIMASK and runs after the caller restores its saved mask.
`nx_arch_idle_if_unchanged()` instead requires all incoming masks clear, saves
PRIMASK, rechecks an aligned published sequence with acquire semantics and sleeps
only when it still matches the caller's snapshot. It restores the exact incoming
mask; the caller retries the authoritative predicate and supplies an IRQ timer
for finite deadlines. This is one CPU and one security state's publication
domain. NMI/HardFault, DMA, SMP and other security-world publishers are excluded.
Native reports unsupported. Compiler/model evidence does not qualify a concrete
timer, clock continuity, physical power behavior or wake latency.

`cache.h` exposes distinct data clean, invalidate, combined maintenance and
instruction synchronization. Every range must own complete reviewed 32-byte
lines, fit the CPU address space and be quiescent across CPU/DMA/security users.
The functions do not round ranges, enable caches or flush unrelated dirty lines.
`mpu.h` retains distinct v7-format and v8 base/limit/MAIR encoders. Single-region
programming requires an already masked privileged Thread and disabled MPU;
actual TYPE bounds and fields are checked before writes. CTRL and incoming RNR
are preserved. Memory policy and any later enable operation belong to the caller.
The optional M0+ MPU uses the v7-format registers confirmed by CMSIS.

`security.h` identifies the reviewed image state and encodes explicit SAU regions.
Only a Secure compiler/profile with reviewed SAU regions can program one owned
region, while SAU is disabled and the privileged Thread has PRIMASK set. No API
changes SAU CTRL/ALLNS, another region, Secure gateways or another world's alias.
NonSecure and single-domain images return unsupported without SAU MMIO. IDAU,
overlap rules, memory permissions and gateway policy stay outside these mechanisms.

`features.h` reports immutable reviewed cache/MPU/DWT/security/SAU capabilities,
not enabled state or hardware qualification. Native exposes the same APIs with
absent-hardware results; pure encoders remain usable. Extending CPU mechanisms
does not provide a SoC startup, vector table, clock tree, memory/DMA routes, Board
wiring or an RTOS FPU/MVE context port. Each new platform must supply and verify
those boundaries independently. Publication and RMW atomics have separate
contracts in `atomic.h`; CPU-specific fallback never silently links libatomic.
Aligned acquire/release word loads and stores do not mask IRQs. M0/M0+ RMW uses
bounded saved-PRIMASK exclusion and restores the incoming mask; all concurrent
users must be privileged local Thread/configurable-IRQ callers. NMI, HardFault,
another core, DMA and another security world are outside this exclusion domain.
Other maintained cores use compiler lock-free RMW; exclusive retries do not
establish a fixed cycle bound. Initialize words before publication and retain
their lifetime until every reader/writer has stopped.

Native models recursive exclusion and C11 fences; it does not model real NVIC,
DMA, physical timing or signal-safe interrupts. Invalid restore nesting aborts in
both Debug and Release. GoogleTest/GoogleMock compiles the real Cortex-M
algorithm against a narrow register/instruction hardware boundary for nine CPU
profiles; the normal ARM path inlines that boundary and never links the mocks.
Native tests exercise real recursive exclusion, strict token nesting and context
queries. Cache, MPU and security models mock only external CPU/MMIO operations
and run the production C algorithms, including rejection before state changes.
Pinned CMSIS `core_cm0plus/7/23/33/55/85.h`, `mpu_armv7/8.h` and
`cachel1_armv7.h` establish the maintained register layouts. Actual IRQ latency,
privilege transitions, DMA/cache coherency and CPU timing require HIL.
