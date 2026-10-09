# Architecture primitives

`Nexus::Arch` owns short CPU-local exclusion, exception-context queries and
barriers. It depends only on the effective build configuration and host threading
library. HAL and baremetal OSAL use it; it never calls HAL, OSAL or a vendor SDK.

The maintained ports are Native (POSIX/Windows) and Cortex-M4 (GCC/Clang,
ARMv7E-M). Configuration rejects other CPU/platform combinations. Cortex-M4
contains real PRIMASK/IPSR/barrier instructions, not weak success fallbacks.
FPU policy remains a SoC/toolchain decision and is not required by these primitives.

`nx_arch_irq_save` returns an opaque token. Restore tokens on the same CPU/thread
in reverse order, including a region entered with interrupts already masked.
Do not sleep, allocate, invoke user callbacks or call kernel APIs while holding
this region. PRIMASK does not mask NMI/HardFault or protect another CPU.
FreeRTOS retains its own task/BASEPRI syscall boundary; an architecture token
cannot be substituted for a kernel token.
The mask query conservatively checks PRIMASK, BASEPRI and FAULTMASK on
Cortex-M4. Blocking drivers use it to reject contexts that can mask their
completion IRQ or tick. Save/restore changes only PRIMASK and does not replace
the kernel's BASEPRI state.

Native uses an independent recursive lock, thread-local nesting and C11 fences.
It models thread exclusion, not NVIC priority, DMA ordering, ISR timing or
POSIX signal-safe interrupts. Invalid restore nesting terminates the process in
Release as well as Debug. Windows and Cortex-M4 require their own target execution
evidence; Native contract execution alone cannot qualify them.

`tests/arch` checks recursive exclusion, contention, critical-section visibility
and unchanged HAL atomic behavior. The baremetal contract fixture links an
explicit CPU model to exercise saved-mask and ISR decisions independently of
production ports. Real interrupt latency and PRIMASK/BASEPRI preemption remain
physical target validation.
