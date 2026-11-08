---
id: design.io-extension-boundaries
type: explanation
audience: integrator
source_language: en
---

(io-extension-boundaries)=
# Optional storage, IPC, frames and F407 handoff

Public generic mechanisms are separate from concrete MCU adapters. Current
production integration is F4; retired H5/H7/U5 adapters are not selected merely
because their vendor libraries remain in the repository. Physical HIL is not
executed and source presence does not create a software candidate.

| Mechanism | Current boundary |
| --- | --- |
| Mapped storage owner | Caller-owned slots, exact epochs and a sole writer; explicit host model where selected |
| AMP IPC core | Public lane/epoch/cache contract with external boot and coherency proof; no current production dual-core adapter |
| Frame layout | Public dimensions, stride and full-frame loans; no retained H743 DMA2D adapter |
| F407 tail handoff | Exact baremetal VE/ZG preconditions below; not live RTOS adoption |

Accepted asynchronous operations retain metadata and buffer loans until
acquire-observed SETTLED. Cancel and deadlines request withdrawal; callers
continue required service. Failed drain retains storage and recovery duties.
Observers and controls must also be joined before their own storage is reused.

## Mapped ownership and frame storage

The mapped owner serializes one writer against all aperture readers. Copying a
token creates no new loan, epochs do not wrap, and a failed final unmap retains
the exact cleanup token. No CPU, DMA or cache writer may bypass the selected
backend. A host NOR model establishes software rules, not physical QSPI, XIP,
cache or Flash qualification.

Frame contracts validate complete storage extents, format and stride. A format
conversion API or generic frame description does not establish an MCU accelerator
or Board connection. Storage and execution progress stay caller-owned.

## F407 masked tail handoff and clock barrier

The F407 handoff component reuses the ClockDomain barrier. Enroll every actual
owner, close admission, cancel/drain/join users, stop Platform and release exact
leases. An empty foreign domain, stale epoch or retained lease rejects; the
domain cannot discover unenrolled users or other masters.

The caller enters privileged MSP Thread with PRIMASK set and BASEPRI, FAULTMASK
and CONTROL clear. It establishes stopped System, HSI16/reset buses, no active
FP/lazy context or MPU, no live SysTick/NVIC/peripheral/DMA and idle Flash with
ART/prefetch disabled. Reject before writes; reading SysTick CTRL consumes
COUNTFLAG. Validate the exact Flash density, aligned vector table, SRAM MSP and
Thumb PC. Shared device ID does not prove full part or PCB identity.

Successful tail transfer never returns. It replaces VTOR/MSP and branches while
masked; target startup must accept that entry and reset controllers before
enabling gates. Authentication, image selection, partitions, NMI/debugger/master
exclusion and recovery remain external. Live RTOS/FPU adoption, DVFS and deep
STOP continuity are not supplied by this handoff.

Read the [public API](../reference/api/extension-components.md),
[ownership contract](../explanation/ownership.md) and
[current support](../delivery/support.md). Original chip-specific references are
available at [their fixed source](https://github.com/X-Gen-Lab/nexus/tree/8e147fc2c812d81ab6596e0d1a6bc8908d30ac18/docs/design/io-extension-boundaries.md).
