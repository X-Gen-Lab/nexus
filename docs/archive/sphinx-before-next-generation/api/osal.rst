OSAL API Reference
==================

Public headers under ``osal/include/osal`` define resource, context, timeout and
lifecycle contracts. Maintained backends are Native, FreeRTOS and baremetal.
RT-Thread is not an implemented build backend and fails configuration.

Context and timeouts
--------------------

Creation/deletion, task control, mutex ownership, blocking waits and allocation
belong to task context. Only APIs explicitly declared for ISR use may run in
interrupts. Use ``*_from_isr`` where provided, respecting backend capability and
MCU interrupt priority rules. An API's presence does not make every operation or
timeout ISR-safe.

Timeouts are milliseconds. ``OSAL_NO_WAIT`` attempts once;
``OSAL_WAIT_FOREVER`` permits indefinite waiting. Positive finite waits use the
backend clock and cannot become zero through tick conversion. Workers needing
cooperative shutdown must use bounded waits. Full/empty, timeout, exhaustion and
unsupported operations remain distinct outcomes.

Tasks and shutdown
------------------

``osal_task_config_t`` contains function and argument. Create with
``osal_task_create(&config, &handle)``; stack size is in bytes. Native starts a
worker on creation; FreeRTOS execution depends on kernel lifecycle. Shared
resources must exist before workers can access them.

.. code-block:: c

   #include "osal/osal.h"

   static void worker(void* arg) {
       (void)arg;
       while (!osal_task_should_stop()) {
           if (osal_task_delay(10) != OSAL_OK) {
               return;
           }
       }
   }

   osal_status_t start_worker(osal_task_handle_t* handle) {
       const osal_task_config_t config = {
           .name = "worker",
           .func = worker,
           .arg = NULL,
           .priority = OSAL_TASK_PRIORITY_NORMAL,
           .stack_size = 2048
       };
       return osal_task_create(&config, handle);
   }

Initialize OSAL and check creation. Request cooperative stop with
``osal_task_request_stop``; the worker observes ``osal_task_should_stop`` and
returns. Join with bounded ``osal_task_join`` before deleting the handle or
releasing shared data. Delete of a running task can return BUSY, not forcefully
terminate arbitrary code. Retain dependencies when join fails.

Synchronization and lifetime
----------------------------

Create into an ``osal_mutex_handle_t``; lock/unlock/delete receive the handle,
not its address. Mutexes have task ownership and cannot serve as ISR locks.
Locked mutex deletion returns BUSY; its owner must unlock before reclamation.
Counting semaphores use ``osal_sem_create(initial_count, max_count, &handle)``
and ``osal_sem_take``/``osal_sem_give``.

Queues use ``osal_queue_create(item_size, item_count, &handle)`` and copy fixed
size items. Callers supply complete item buffers. Item size, storage and object
counts are bounded by generated resource profiles. Handles are opaque; successful
deletion invalidates copies, and stale tokens cannot acquire a reused slot.
Backend lifetime protection does not synchronize arbitrary application data.
Respect BUSY/errors and quiesce users before freeing contexts or buffers.

Events, timers and critical sections
------------------------------------

Event waits are task operations; ISR changes use explicitly declared variants
and can report unsupported capabilities. Event barriers require a scheduler.
Software timer callbacks run in service/task context, not as hardware-ISR timing.
Keep callbacks bounded and avoid blocking the FreeRTOS timer service. Timer
deletion can return BUSY while callbacks/control still own it; retain context
and retry after quiescence. Self-deletion does not imply automatic reclamation.

Balance critical-section enter/exit on the same task. Do not hold a critical
section across blocking work, Flash, logging or arbitrary callbacks. Mixed
ISR/task use needs the documented saved-mask HAL primitive and board priority
policy.

Backend boundaries
------------------

Native uses host synchronization. Its tests cannot validate Cortex-M masking,
DMA or deadlines. The pinned real FreeRTOS POSIX contract runner also does not
validate the Cortex-M interrupt port.

Baremetal is a main loop: tasks, event barriers and software timers return
``OSAL_ERROR_NOT_SUPPORTED`` rather than fictitious scheduler success. Its timed
waits need a real board monotonic clock and declared capabilities. Delay/yield
do not switch tasks. Scheduler-dependent applications must reject baremetal.

Generated API
-------------

.. doxygengroup:: OSAL_DEF
   :project: nexus
   :members:

.. doxygengroup:: OSAL_TASK
   :project: nexus
   :members:

.. doxygengroup:: OSAL_MUTEX
   :project: nexus
   :members:

.. doxygengroup:: OSAL_SEM
   :project: nexus
   :members:

.. doxygengroup:: OSAL_QUEUE
   :project: nexus
   :members:
