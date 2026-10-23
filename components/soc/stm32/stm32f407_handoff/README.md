# STM32F407 masked image handoff

Link `Nexus::STM32F407Handoff` through `nexus_add_firmware(... LIBRARIES ...)`.
The optional target exists only for exact STM32F407VET6/ZGT6 baremetal Platform
assemblies. Product files include only `nexus/components/stm32f407_handoff.h`;
vendor registers and mutable admission storage stay private.

Call `nx_stm32f407_handoff_initialize(capacity)` once before admitting users.
The stable `nx_stm32f407_handoff_domain()` getter never initializes hardware or
allocates. Explicitly enroll every Platform/product execution, IRQ, DMA,
notification and memory owner. Close admission, perform actual cancellation and
drain/join, stop the Platform and only then release each exact lease. A foreign
empty domain, stale epoch or retained lease cannot authorize transfer. The domain
has no automatic knowledge of unenrolled users, another CPU or bus master.

From privileged MSP Thread, save/mask interrupts and call
`nx_stm32f407_handoff(closed, physical_flash_vector)`. Entry requires PRIMASK=1,
BASEPRI=FAULTMASK=0, no active FP/lazy preservation, no MPU or enabled/pending/active
IRQ/SysTick, HSI16/reset buses, stopped System, no peripheral/DMA gates or busy
Flash. Ordinary SystemInit full CP10/11 permissions with no active FP owner are
accepted. The tail disables CP10/11 with DSB/ISB before replacing MSP and branching.
Rejection preserves masks and writes no MMIO, but observing SysTick CTRL consumes
COUNTFLAG. An off gate prevents execution; it does not certify cleared latent
controller configuration. The target resets controllers before enabling gates.

The complete 98-entry vector table must fit the generated and physically observed
512 KiB/1 MiB Flash density and be 512 aligned. MSP is 8 aligned in
(0x20000000,0x20020000]. Reset PC is Thumb and its first halfword lies in physical
Flash. DEV_ID 0x413 is a shared device group; generated exact-part/PCB provenance
still establishes F407/package identity. The mechanism does not authenticate an
image, select boot/recovery policy or reserve any Flash partition.

Successful transfer never returns. VTOR names the admitted Flash table; CONTROL=0,
PSP=0, CP10/11 disabled and PRIMASK=1. Main SRAM is preserved until target startup
chooses its own copy/clear policy. The target must accept masked entry and unmask
only when its own state is ready. It cannot reuse source deadlines. NMI/HardFault,
debugger and other masters are excluded by the caller throughout.

This is a conservative SOC-33 software increment, not complete SOC-33 acceptance:
there is no live RTOS/kernel adoption, active-FPU context transfer, RAM/ROM-vector
transfer, H743/cache-enabled transition, universal Cortex-M support or measured
physical handoff. SOC-34 DVFS and IO-30 runtime clock transitions remain open;
fixed-frequency providers are not reclassified as mutable clock contracts.
