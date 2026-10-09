# OSAL backend and resource contracts

Implements RF-FIX-03, RF-OS-01 and RF-OS-02. This record covers software
behavior, not real Cortex-M interrupt priority, stack watermark or board timing.
The final platform matrix must bind its own clean source/configuration/artifacts.

## Defects reproduced and corrected

Native and baremetal advertised `UINT32_MAX` event bits, while their production
set/wait paths rejected bits 24..31. A production Native adapter reproducer
aborted at the first advertised unavailable bit. All three adapters now use
`OSAL_EVENT_BITS_MASK` for discovery and validation. Shared executable regression
code checks each advertised bit through set/wait/get/clear and FromISR entry,
and rejects every reserved bit through updates, waits and barriers. FreeRTOS
POSIX exercises its actual deferred daemon entry from host task context; the
baremetal fixture separately models an ISR. These do not establish physical ISR
priority or electrical behavior.

The pinned FreeRTOS kernel also reproduced a suspended-scheduler defect: a finite
OSAL semaphore wait entered `xQueueSemaphoreTake` and tripped the kernel assertion
at `queue.c:1675`. Task operational calls now return `BUSY` during scheduler
suspension; callers resume scheduling before retry. Construction, nonblocking
metadata queries and unused sync-object deletion keep their existing boot
behavior. Before startup operational calls and FromISR entry still return
`NOT_INIT`, without entering uninitialized kernel priority handling.

## Public diagnostics

`osal_get_backend_info()` retains its existing static capabilities, deletion
policy, per-class capacities and usable stack/queue byte budgets. Event capacity
is consistently 24 bits. `reserved_object_bytes` remains the exact backend-owned
arrays; final linker maps must additionally account for kernel idle/daemon
objects, exception stacks, management heap and all other platform components.
It is not a whole-firmware SRAM total.

`osal_get_execution_info()` reports backend, initialization, calling ISR context
and `NONE`, `NOT_STARTED`, `RUNNING` or `SUSPENDED` scheduler state. Native and
baremetal use `NONE`: they do not require starting a kernel scheduler. Queries
do not allocate, initialize OSAL or start scheduling. FreeRTOS boot queries use
saved Arch state and preserve the incoming priority mask.

`osal_get_resource_usage()` reads actual per-class `capacity` and `reserved`
slots under the adapter's metadata lock. Constructing, closing, finished and
quarantined lifetimes remain reservations until actual reclamation; optional
statistics collection is not required. The query also reports the shared,
cumulative lifetime token capacity, issued count and remaining identities.
Deletion frees slots and never returns identities to this budget. A failed host
or kernel construction after reservation may consume an identity.

`OSAL_LIFETIME_TOKEN_LIMIT` defaults to `UINTPTR_MAX >> 4`, approximately
268 million identities on a 32-bit MCU. A consistent smaller compile-time budget
may be selected for a deployment; invalid zero/oversized values are rejected.
Budget exhaustion returns `NO_MEMORY` and a NULL handle, even with free slots.
Queries distinguish this from pool exhaustion. `osal_reset_stats()` and
`osal_deinit()/osal_init()` never reset identities. This is an explicit finite
boot/process lifetime budget, not leaked heap storage or unbounded reuse.

## Executed validation

On 2026-10-09 the focused production OSAL subtrees configured and built using
CMake 4.4.4 and GNU 14.2.0, then CTest executed **278 tests**, with zero failure,
error or skip. The harness uses the repository's actual `arch/`, `osal/` and
`tests/osal/` CMake targets and pinned Google Test/FreeRTOS dependencies; Native
core tests exclude the separate HAL integration groups. It does not substitute
the production adapters or kernel with a mock implementation.

The regressions include all advertised event bits, actual slot pool exhaustion
and reuse, stale handles, statistics-independent diagnostics, suspended-kernel
rejection, boot mask/ISR gates, clock conversion around 32-bit tick wrap, real
kernel waiter deletion pins and task/timer reclamation. Three compiled production
variants use a six-identity budget to exhaust it across mutex/semaphore/queue/event
classes, then verify freed slots, statistics reset and rollback cannot reuse old
identities. Native runs this variant with statistics disabled. Existing faulted
mutex-owner tests additionally verify the actual reserved quarantined slots.

STM32 baremetal, STM32 FreeRTOS and GD32F470 FreeRTOS production adapter translation
units compiled with ARM GNU 14.3.rel1 and existing validated configuration commands.
This is adapter compile evidence; no new complete firmware or physical execution
is claimed by this scoped record. The root integration must regenerate the final
effective configurations and link/verify the complete firmware matrix.

Source/report/binary hashes and actual commands are in
[the machine record](osal-refactor-validation.json). Logs/JUnit are retained in
`build/osal-refactor/` and `build/osal-refactor-cmake/` locally. The initial
unmodified Native mask and FreeRTOS suspended-wait reproducer binaries both
returned `SIGABRT`; final regression targets passed.
