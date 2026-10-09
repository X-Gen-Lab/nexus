# CPU-local primitives

`Nexus::Arch` exposes saved interrupt masks, exception context, ordering barriers
and an optional already-enabled DWT cycle snapshot. It does not call I/O, OS or
vendor SDK code. Ports are Cortex-M4 and a POSIX-threaded Native behavior model.

Save/restore tokens on the same CPU/thread in strict reverse order. Cortex-M4
preserves incoming PRIMASK and does not overwrite BASEPRI/FAULTMASK. NMI and
HardFault remain unmasked. Keep metadata sections short: no blocking, allocation,
large copies, kernel calls or user callbacks. A saved Arch token is not a kernel
critical-section token or an SMP lock.

Mask/context queries let blocking I/O reject contexts that prevent its progress.
DMB/DSB/ISB encode different ordering guarantees; they do not prove peripheral
or DMA quiescence. DWT snapshots are not monotonic deadlines and do not enable
or reset the counter. Their wrap/frequency limits are documented in the header.

Native models recursive exclusion and C11 fences; it does not model real NVIC,
DMA, physical timing or signal-safe interrupts. Invalid restore nesting aborts in
both Debug and Release. Tests in `tests/contracts` cover saved-mask decisions and
request publication. Actual IRQ latency and CPU timing require HIL.
