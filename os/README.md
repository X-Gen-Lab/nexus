# Optional OS adapters

Thin wait/wake and per-object storage contracts. Baremetal has no scheduler;
Native uses POSIX threads; FreeRTOS uses explicit static TCB, stack and queue
storage selected by the caller. No universal maximum pool, hidden worker,
default timer daemon or device dependency on OS.

Notifications are hints: arm/recheck the authoritative request predicate.
Kernel ISR operations validate the selected syscall ceiling. A returning task
parks until another task completes deletion/join; reclaim its TCB/stack only
after the join contract says observers are gone. Applications own scheduler
start, workers, queue depth, fairness and shutdown progress.

The real FreeRTOS POSIX scheduler fixture validates host task/queue lifecycle;
ARM integration links the actual Cortex-M4F kernel port. Host execution does not
qualify MCU exception priority, FPU context, stack high-water or timing.

The host fixture keeps the pinned kernel and POSIX port unchanged, and uses
`tests/contracts/os_freertos_runtime/posix_event.c` as its owned event support.
The port cancels cond-waiting pthreads before signalling/joining them; upstream
event support has no cancellation cleanup and can leave an exited thread's
mutex locked. Owned waits unlock on both return and cancellation, preventing
repeated task-join deadlocks. This host-only correction does not change the MCU
backend or establish MCU scheduler qualification.
