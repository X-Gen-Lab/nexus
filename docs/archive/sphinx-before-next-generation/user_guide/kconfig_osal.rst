OSAL Backend Configuration
==========================

Maintained backends are Native, baremetal and pinned FreeRTOS. Their capabilities
are intentionally different; query ``osal_get_backend_info`` and
``osal_get_execution_info`` instead of assuming task/timer/ISR support. Unsupported
backends/configuration stop the build.

Native
------

Native uses real host threads/synchronization and dynamic resources. It validates
host behavior, not MCU ISR priority or real-time timing. Full profiles select
optional services explicitly. Linux execution does not qualify Windows/macOS.

Baremetal
---------

Baremetal owns one control thread with explicit loop/pump and hardware ISRs.
It provides bounded configured mutex/semaphore/queue/event primitives. There is
no stackful/cooperative task scheduler, software timer or dynamic OSAL heap.
The MCU platform installs a real monotonic time source; clock queries do not
increment a fake counter.

FreeRTOS
--------

The pinned kernel uses static control blocks plus configured task stacks,
queue payloads and six class pools. Idle/daemon stacks are separate. Slot reuse
does not reuse lifetime identity; token exhaustion returns explicit failure.
OSAL heap sealing controls its allocator only, not vendor/libc malloc.

External main bootstraps Runtime, creates a worker with checked return status,
then explicitly starts the scheduler. Blocking and FromISR calls reject use
before scheduler start. Incoming port masks are preserved during object creation;
Arch PRIMASK and FreeRTOS BASEPRI/syscall critical sections are distinct.
Logical priority 0–4 interrupts cannot use the selected FromISR range (current
syscall threshold 5). Running/suspended MCU kernel shutdown returns BUSY.

Budgets and Lifecycle
---------------------

Each Board/backend uses a separate build root and explicit fragment. Configure
pool counts, queue byte/item limits, task stacks and optional heap from real
application demand. Firmware main stack/libc heap are separate budgets. Close
requires stopped producers/waiters/callbacks; BUSY preserves objects. Runtime
owns infrastructure only, with OSAL then HAL release and PARTIAL quarantine on
actual hardware cleanup error.

Real FreeRTOS POSIX contract execution is distinct from ARM port timing and
physical qualification. No board has been connected for this delivery. Exact
source/pool/config evidence is linked by ``docs/strategy/support-matrix.yaml``.
