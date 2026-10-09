# Backend capabilities and deterministic object storage

This is the implemented OS-001/002 and MEM-001 contract. Query
`osal_get_backend_info()` instead of inferring support from a header or a backend
name. No public structure contains a FreeRTOS or vendor SDK type.

| Behavior | Native | Baremetal | FreeRTOS |
|---|---|---|---|
| Tasks / software timers | Host threads | Unsupported | Kernel tasks / daemon |
| Priority scheduler | No; priority is metadata | None | Preemptive single-core |
| OSAL object storage | Static slots; dynamic payload / host runtime | Static arrays | Static kernel control blocks, stacks and queue storage |
| Dynamic OSAL memory | Host heap | Unsupported | Optional kernel management heap |
| Delete with waiting callers | Cancel old waits | Cancel old waits | BUSY; settle waits then retry |
| Task delete while running | Cooperative stop, BUSY | Unsupported | Cooperative stop, BUSY |
| Hardware ISR | None | Supported Cortex-M port | Supported Cortex-M4F port |
| Monotonic clock | Host clock | Requires installed board clock | Kernel tick |
| OSAL memory seal | Unsupported | Unsupported | Irreversible, task-only |

`OSAL_CAP_STATIC_OBJECTS` means the backend's OSAL kernel object and application
storage do not use its management heap. The FreeRTOS POSIX simulation port can
still allocate pthread/event implementation storage on the host heap. Neither
this bit nor a successful host test establishes physical interrupt behavior or
whole-program absence of allocation.

## Effective budgets

The selected product config determines object counts. FreeRTOS additionally
uses two independent byte capacities:

- `OSAL_FREERTOS_TASK_STACK_BYTES`: each task's static stack. The requested byte
  count rounds up to kernel stack words and must fit the fixed slot. Requests
  above it fail with INVALID_PARAM rather than fall back to allocation.
- `OSAL_FREERTOS_QUEUE_STORAGE_BYTES`: payload storage per queue, capped further
  by `OSAL_MAX_QUEUE_BYTES`. Each queue also reserves one overwrite scratch item
  bounded by the smaller of the item and payload limits.

Task stack requests of zero retain the kernel minimum. Kernel word size is an
implementation detail; the public query reports the usable byte capacity.
Queues reject zero, multiplication overflow, item-size limits and total payload
limits before object reservation. Pool exhaustion returns NO_MEMORY with a NULL
output. Mutexes, semaphores, events and timers have dedicated static control-block
arrays. Reservation and release use the same generation-protected registry; no
old token can address a new static-storage lifetime.

`reserved_object_bytes` reports backend-owned arrays, including its registry,
control blocks, task stacks, queue payloads and scratch. It excludes compiler
padding between separate symbols, the kernel's own idle/timer objects, driver
storage and management heap. The final linker map is the complete SRAM budget.
The maintained kernel's `configKERNEL_PROVIDED_STATIC_MEMORY=1` supplies static
idle and timer daemon TCB/stacks; its timer command queue is also static. Product
profiles must budget these kernel allocations, the exception/main stack and all
other linked components separately.

`osal_get_execution_info()` exposes the current initialization/ISR context and
kernel scheduler state without starting or allocating resources. Native and
baremetal report `NONE`; FreeRTOS reports prestart/running/suspended. Task
operational calls during FreeRTOS scheduler suspension return BUSY, so a finite
wait cannot enter an illegal kernel wait. Prestart operations remain NOT_INIT.

`osal_get_resource_usage()` counts actual per-class reserved slots, including
constructing/closing/quarantined lifetimes, independently of optional statistics.
It also reports cumulative token capacity/issued/remaining. The default
`OSAL_LIFETIME_TOKEN_LIMIT` is `UINTPTR_MAX >> 4` (268435455 on 32-bit MCUs).
Deletion reuses storage, never identities; failed construction may consume one.
Exhaustion returns NO_MEMORY with NULL output. Neither statistics reset nor
rollback/reinit resets the token budget. See the executable small-budget
production variants and [refactor evidence](../../docs/implementation/osal-refactor.md).

Every maintained backend advertises `OSAL_EVENT_BITS_MASK` (bits 0..23) and
rejects bits 24..31 consistently in update/wait/barrier and FromISR entry.

## Completion and reclamation

Before the scheduler starts, OSAL metadata locks and its public balanced boot
critical regions use saved Arch IRQ state. In particular an initialization
query preserves the incoming PRIMASK rather than entering the kernel's initial
sentinel BASEPRI nesting and freezing HAL startup ticks. Boot constructors and
idle-object deletion also save/restore the kernel priority mask around kernel
calls. Boot task context may create resources, query their actual initial state,
delete unused mutex/semaphore/queue/event objects, or allocate/free/seal the OSAL
management heap. Task/timer reclamation needs a started scheduler/daemon; pending
boot task/timer lifetimes remain owned, and rollback is BUSY rather than success.
Mutex locking, queue send/receive, semaphore/event updates and task operations
require a started scheduler and return NOT_INIT before it starts. FromISR actions
are likewise rejected before start, including clock reads: an ISR clock query
returns NOT_INIT with zero output before reaching kernel priority validation.
Task-context boot clock reads remain valid and preserve the incoming masks.
A fake pre-start current task cannot acquire
ownership. Strict startup order: Product boot initializes hardware and OSAL,
then create the bootstrap task and start scheduling, then the scheduled worker
initializes application services. Once scheduling starts, task/ISR metadata
locks use the kernel's port masks. No scheduler may start from inside an OSAL
boot critical region.

The boot regression runs the pinned kernel with link wrappers modeling the
Cortex-M port's initial sentinel nesting. It verifies that all constructors and
supported boot deletion/memory operations preserve the incoming priority mask
and Arch state. A context probe and a kernel-entry counter verify that rejected
pre-start ISR clock reads do not call the kernel ISR tick getter, including
when Arch and the priority mask were already held. This is a software mask
model, not ARM instruction/HIL evidence.

FreeRTOS self-deletion defers TCB cleanup to the idle task. A function-return
flag alone cannot permit reuse of a static TCB. The OSAL trampoline therefore
marks function completion and parks the kernel task. Join observes completion;
manager delete calls `vTaskDelete(other)` on the single-core port. That call
finishes port cleanup synchronously before releasing the storage slot. A task
cannot resume once its function has completed. SMP ports require their own
reclamation design and are not supported by this implementation.

A worker returning while it still owns an OSAL mutex is an application fault.
Native and FreeRTOS keep its task lifetime quarantined and delete returns BUSY.
A new task cannot inherit the old mutex through a reused TCB address or host
thread identifier. The platform does not force-unlock the faulted owner's lock;
product fault recovery/reset is required. An isolated executable for each
backend deliberately leaves this quarantined pair and verifies the failure
outcome; it is not a successful resource-drain scenario.

Timer control and delete retain the existing daemon acknowledgement barriers.
Static timer storage is released only after the daemon processes delete and its
barrier; concurrent delete has one reclaim owner. Timer names are copied into
the registry rather than borrowing caller-local memory.

`osal_deinit()` is an externally quiesced boot rollback operation: stop producers
and reclaim all objects first. It returns BUSY with live objects or OSAL heap
allocations, does not invalidate live objects, and never resets token generations.
The FreeRTOS backend additionally rejects deinit after the scheduler has started;
it does not claim to stop a Cortex-M scheduler. Native process-lifetime conditions,
monitors and TLS are retained for safe reinitialization. Baremetal retains its
installed board time source.

## Management heap seal

After boot allocation and before admitting concurrent allocators, FreeRTOS
products may call `osal_mem_seal()`. It permanently rejects new OSAL allocations
and realloc growth; shrinking an existing allocation and freeing remain valid.
Static object creation continues to work. Sealing is idempotent and survives
OSAL rollback/reinitialization. It does not intercept libc, SDK, direct kernel
or other module allocators. A control product must still audit those routes.

## Executed software regressions

The real pinned kernel POSIX runner exercises 12 contract groups. Added cases
fill task/queue pools, reject excessive stack/payload budgets, create every
static object class with the OSAL heap sealed, and check unchanged kernel heap.
It also creates/joins/deletes 100 task lifetimes without depending on idle cleanup,
rejects stale tokens, and preserves the 30 concurrent timer-delete/retry lifetimes.
Native and the explicit baremetal board model check capability and rollback
outcomes. These are software checks; Cortex-M stack high-water, priority/IRQ,
electrical timing and worst-case load still require target evidence.
