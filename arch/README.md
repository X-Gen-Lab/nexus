# CPU-local primitives

`Nexus::Arch` exposes saved interrupt masks, exact exception identity, current
privilege, ordering barriers and an optional already-enabled DWT cycle snapshot.
It does not call I/O, OS or vendor SDK code. One compile-time Cortex-M primitive
implementation covers ARMv6-M, ARMv7-M, ARMv7E-M, ARMv8-M Baseline and ARMv8-M
Mainline. The maintained full SoC/platform targets remain STM32F407 and GD32F470
Cortex-M4F; compiling CPU primitives does not add a SoC, Board or RTOS port.

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
or DMA quiescence. DWT snapshots require an explicit reviewed SoC capability and
privileged access. A disabled capability emits no DWT reads. Snapshots do not
enable or reset the counter and are not monotonic deadlines. Their wrap/frequency
limits are documented in the header. These primitives do not supply cache/MPU,
TrustZone policy or a fallback for the Core's required lock-free atomics.

Native models recursive exclusion and C11 fences; it does not model real NVIC,
DMA, physical timing or signal-safe interrupts. Invalid restore nesting aborts in
both Debug and Release. GoogleTest/GoogleMock compiles the real Cortex-M
algorithm against a narrow register/instruction hardware boundary for seven CPU
profiles; the normal ARM path inlines that boundary and never links the mocks.
Native tests exercise real recursive exclusion, strict token nesting and context
queries. Actual IRQ latency, privilege transitions and CPU timing require HIL.
