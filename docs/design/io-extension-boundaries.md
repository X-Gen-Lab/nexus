---
id: design.io-extension-boundaries
type: explanation
audience: integrator
source_language: en
---

(io-extension-boundaries)=
# Optional storage, cryptography, frames and F4 handoff

Public generic mechanisms are separate from concrete MCU adapters. Current
production integration is F4; retired H5/H7/U5 adapters are not selected merely
because their vendor libraries remain in the repository. Physical HIL is not
executed and source presence does not create a software candidate.

| Mechanism | Current boundary |
| --- | --- |
| Mapped storage owner | Caller-owned slots, exact epochs and a sole writer; explicit host model or selected F479 CPU-read adapter |
| AMP IPC core | Public lane/epoch/cache contract with external boot and coherency proof; no current production dual-core adapter |
| Frame layout | Public dimensions, stride and full-frame loans; no retained H743 DMA2D adapter |
| Cipher and hash | F415ZG/F417ZG selected CRYP/HASH owners; explicit byte spans and cleanup, without a key store or protocol policy |
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

The selected [F479 CPU-read endpoint](f407-fsmc.md) authorizes only the range of
each live CPU loan. Indirect Flash endpoints still forbid all aperture access;
neither mode grants DMA readers, XIP or access by foreign masters. Failed map
cleanup can remain without a readable loan. A failed final release preserves
cleanup responsibility, not renewed reads; an invalid System epoch may require
product recovery or reset.

Frame contracts validate complete storage extents, format and stride. A format
conversion API or generic frame description does not establish an MCU accelerator
or Board connection. Storage and execution progress stay caller-owned.

## Selected F415/F417 cryptographic engines

`cipher.h` and `hash.h` expose separate two-pointer interfaces with shared
read-only operations. The typed factory returns statically selected objects;
neither lookup nor query acquires clocks or starts hardware. The F415ZG and
F417ZG [software references](../../tests/contracts/stm32f4_crypto/f415zg.toml)
select `irq-blocks` or the separately selected DMA mode for each IP. F405/F407 do not acquire these engines
merely because the same vendor family library contains their implementation.

CRYP supplies AES ECB/CBC/CTR and DES/TDES ECB/CBC. Query reports the actual
key sizes, alignment, request granularity and in-place support. This provider
requires whole blocks, including CTR; the public interface permits future
providers to report byte-tail support without changing the ownership contract.
Keys and IVs use algorithm byte order rather than vendor word types. The
platform does not generate IVs, pad messages, authenticate ciphertext or select
product algorithms. SHA1, MD5 and their HMAC variants describe the capabilities
of this HASH IP; they do not imply SHA256, AEAD or a TLS acceleration binding.

One privileged task serializes each engine. ACCEPTED borrows the request and
every referenced input, output, key and IV until acquire-observed SETTLED.
Inputs cannot overlap mutable request, provider or System storage; output
cannot overwrite key/IV or partially overlap input. Cipher failures can leave
an explicitly counted output prefix. A digest is valid only on SUCCESS.
Cancel and deadline select a terminal cause; neither releases the loan early.

In IRQ mode, CRYP uses the official HAL interrupt transport. HASH uses official initialization
and a bounded C FIFO protocol because the selected HAL transport assumes padded
word reads and can wait while advancing HMAC phases. Each service call reads
only bytes inside the declared spans; a partial final word is assembled locally.
HMAC needs explicit task service between phases, even when no final-digest IRQ
has arrived. There is no full-message copy, queue or implicit worker.

The optional DMA paths retain the same owner and request lifetime. Each selected
IP adds only its own static DMA extension and official HAL transport; task
service observes its DMA status with the selected DMA NVIC entries disabled.
HASH input and HMAC key require word alignment and lengths divisible by four;
HMAC keys must also be nonempty. IRQ byte-span constraints remain unchanged.

Every terminal path withdraws IRQ and peripheral references, clears provider
scratch and HAL pointers, and proves the exclusive hardware reset that removes
key, context and FIFO residue. A refused reset or withdrawal retains
QUARANTINED storage for recovery. Caller-owned key bytes are not erased by the
provider. CRYP owns IRQ79; HASH owns only its source on shared HASH/RNG IRQ80.
The currently selected RNG path polls, and a future IRQ RNG transport must join
the same vector owner rather than independently disable or clear it.

Models exercise the real C transport, ownership and failures. ARM references
check exact headers, static factory, linked methods and resource budgets.
Neither result establishes silicon known-answer tests, throughput, side-channel
properties or physical qualification. Source SDK consumers use the same public
references and do not gain access to vendor handles or private configuration.

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
