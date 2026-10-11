Operating System Abstraction Layer
==================================

OSAL maintains Native, baremetal and pinned FreeRTOS backends. API names are
shared; capabilities and execution contexts differ. Query
``osal_get_backend_info`` and ``osal_get_execution_info`` before assuming tasks,
timers, ISR operations, static allocation or scheduler behavior.

Backend Boundaries
------------------

Native uses real host threads/synchronization and dynamic resources. Baremetal
supports one control thread and hardware ISRs with configured bounded sync
objects, but no task scheduler, software timer or dynamic OSAL heap. FreeRTOS
uses the pinned kernel, static control blocks and explicit task/queue/pool
budgets. RT-Thread, Zephyr and a separate Linux backend are not maintained.

Application main owns startup. ``nx_runtime_bootstrap`` initializes HAL then
OSAL without creating workers/scheduler. FreeRTOS applications create a checked
startup worker, then call ``osal_start``; blocking component initialization belongs
in that worker. Baremetal runs its own loop/pump. Native starts threads according
to host backend contract. See the actual external ``rtos_pipeline`` example.

Ownership, Resources and Time
-----------------------------

Mutexes are owned; queues/semaphores/events and tasks have explicit lifetimes.
Deletion requires stopped producers/waiters/callbacks; BUSY retains an object
for retry. FreeRTOS slots are reusable but cumulative lifetime tokens are not;
exhaustion fails explicitly. Finished task identity cannot be reused while it
retains a mutex. Six object pools, queue bytes/items, task stacks and idle/daemon
stacks are distinct budgets. OSAL heap sealing does not seal vendor/libc malloc.

Finite timeouts use real backend clocks and include the documented wait budget;
``OSAL_NO_WAIT`` attempts once and ``OSAL_WAIT_FOREVER`` permits indefinite wait.
Use bounded waits for cooperative worker shutdown. Context/unsupported,
full/empty, timeout and exhaustion remain distinct errors.

Interrupt and Lifecycle Rules
-----------------------------

Only explicit FromISR APIs may be called from permitted interrupts. Blocking
operations reject ISR/active masks. FreeRTOS pre-scheduler blocking/FromISR is
rejected and object creation preserves incoming port masks. Arch PRIMASK and
FreeRTOS BASEPRI/syscall critical sections are different mechanisms. Current
logical syscall threshold is 5; more urgent IRQs may not call kernel FromISR.

Runtime releases OSAL then HAL after application settlement. Idle MCU cleanup
is limited to baremetal or pre-scheduler state. Running/suspended FreeRTOS kernel
returns BUSY; global kernel restart is unsupported. HAL mutating cleanup error
keeps PARTIAL ownership and admission fencing; Runtime does not restore OSAL
over a stopped/partially released platform.

Public contracts are in ``osal/include/osal`` and :doc:`../api/osal`. Real
FreeRTOS POSIX execution is not ARM interrupt/timing qualification. Hardware is
currently deferred; source-bound evidence is linked by the support matrix.
