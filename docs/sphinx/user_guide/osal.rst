OS Abstraction Layer (OSAL)
===========================

Overview
--------

OSAL provides a portable interface for RTOS primitives. Applications can switch
between different RTOS backends (FreeRTOS, RT-Thread, Zephyr) or run bare-metal
without code changes.

Key features:

* Unified API across multiple RTOS backends
* Bare-metal support for resource-constrained systems
* Task, mutex, semaphore, queue, and timer primitives
* Critical section and interrupt management
* Memory allocation abstraction

.. seealso::

   * :doc:`hal` - Hardware Abstraction Layer
   * :doc:`kconfig_osal` - OSAL backend configuration
   * :doc:`../tutorials/task_creation` - Task creation tutorial
   * :doc:`../api/osal` - Complete OSAL API reference
   * :doc:`../platform_guides/index` - Platform-specific RTOS support

OSAL Architecture
~~~~~~~~~~~~~~~~~

.. mermaid::
   :alt: OSAL architecture showing abstraction layer and backend adapters

   graph TB
       APP[Application Code] --> OSAL[OSAL API]
       OSAL --> TASK[Task Management]
       OSAL --> SYNC[Synchronization]
       OSAL --> QUEUE[Message Queues]
       OSAL --> TIMER[Software Timers]

       TASK --> ADAPTER[Backend Adapter]
       SYNC --> ADAPTER
       QUEUE --> ADAPTER
       TIMER --> ADAPTER

       ADAPTER --> FREERTOS[FreeRTOS]
       ADAPTER --> NATIVE[Native/POSIX]
       ADAPTER --> BAREMETAL[Bare Metal]
       ADAPTER --> RTTHREAD[RT-Thread]

       style APP fill:#e1f5ff
       style OSAL fill:#fff4e1
       style TASK fill:#ffe1f5
       style SYNC fill:#ffe1f5
       style QUEUE fill:#ffe1f5
       style TIMER fill:#ffe1f5
       style ADAPTER fill:#e1ffe1
       style FREERTOS fill:#f5e1ff
       style NATIVE fill:#f5e1ff
       style BAREMETAL fill:#f5e1ff
       style RTTHREAD fill:#f5e1ff

Supported Backends
------------------

+--------------+------------+------------------+
| Backend      | Status     | Notes            |
+==============+============+==================+
| Bare-metal   | ✅ Ready   | Cooperative      |
+--------------+------------+------------------+
| FreeRTOS     | ✅ Ready   | Full support     |
+--------------+------------+------------------+
| RT-Thread    | 🚧 Planned |                  |
+--------------+------------+------------------+
| Zephyr       | 🚧 Planned |                  |
+--------------+------------+------------------+

Getting Started
---------------

**Include the header:**

.. code-block:: c

    #include "osal/osal.h"

**Initialize OSAL:**

.. code-block:: c

    osal_init();

Task Management
---------------

Task Lifecycle
~~~~~~~~~~~~~~

The following diagram shows the complete task lifecycle:

.. mermaid::
   :alt: Task lifecycle state machine showing task states and transitions

   stateDiagram-v2
       [*] --> Ready: osal_task_create()
       Ready --> Running: Scheduler
       Running --> Ready: Preempted
       Running --> Blocked: Wait/Delay
       Blocked --> Ready: Event/Timeout
       Running --> Suspended: osal_task_suspend()
       Suspended --> Ready: osal_task_resume()
       Running --> [*]: osal_task_delete()
       Suspended --> [*]: osal_task_delete()

**Create a task:**

.. code-block:: c

    void my_task(void* arg)
    {
        while (1) {
            /* Task code */
            osal_task_delay(100);
        }
    }

    osal_task_config_t cfg = {
        .name       = "my_task",
        .func       = my_task,
        .arg        = NULL,
        .stack_size = 1024,
        .priority   = OSAL_PRIORITY_NORMAL,
    };

    osal_task_handle_t handle;
    osal_status_t status = osal_task_create(&cfg, &handle);

**Task control:**

.. code-block:: c

    /* Delay current task */
    osal_task_delay(100);  /* 100 ms */

    /* Suspend task */
    osal_task_suspend(handle);

    /* Resume task */
    osal_task_resume(handle);

    /* Delete task */
    osal_task_delete(handle);

    /* Get current task handle */
    osal_task_handle_t current = osal_task_get_current();

Mutex
-----

Mutex Usage Pattern
~~~~~~~~~~~~~~~~~~~

The following diagram shows a typical mutex usage pattern for protecting shared resources:

.. mermaid::
   :alt: Mutex usage workflow showing lock, critical section, and unlock

   sequenceDiagram
       participant T1 as Task 1
       participant M as Mutex
       participant R as Shared Resource
       participant T2 as Task 2

       T1->>M: osal_mutex_lock(timeout)
       M-->>T1: Lock acquired
       T1->>R: Access resource

       T2->>M: osal_mutex_lock(timeout)
       Note over T2,M: Task 2 blocks waiting

       T1->>R: Modify resource
       T1->>M: osal_mutex_unlock()
       M-->>T1: Lock released

       M-->>T2: Lock acquired
       T2->>R: Access resource
       T2->>M: osal_mutex_unlock()
       M-->>T2: Lock released

**Create and use mutex:**

.. code-block:: c

    osal_mutex_handle_t mutex;
    osal_mutex_create(&mutex);

    /* Lock with timeout */
    if (osal_mutex_lock(mutex, 1000) == OSAL_OK) {
        /* Critical section */
        /* ... */

        /* Unlock */
        osal_mutex_unlock(mutex);
    }

    /* Lock forever */
    osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);

    /* Try lock (non-blocking) */
    if (osal_mutex_lock(mutex, 0) == OSAL_OK) {
        /* Got the lock */
    }

    /* Delete when done */
    osal_mutex_delete(mutex);

Semaphore
---------

**Binary semaphore:**

.. code-block:: c

    osal_sem_handle_t sem;
    osal_sem_create_binary(0, &sem);  /* Initial count = 0 */

    /* Wait (take) */
    osal_sem_take(sem, OSAL_WAIT_FOREVER);

    /* Signal (give) */
    osal_sem_give(sem);

    /* Delete */
    osal_sem_delete(sem);

**Counting semaphore:**

.. code-block:: c

    osal_sem_handle_t sem;
    osal_sem_create_counting(10, 0, &sem);  /* max=10, initial=0 */

    /* Take multiple */
    osal_sem_take(sem, 1000);

    /* Give multiple */
    osal_sem_give(sem);

    /* Get current count */
    uint32_t count = osal_sem_get_count(sem);

Message Queue
-------------

**Create and use queue:**

.. code-block:: c

    typedef struct {
        uint32_t id;
        uint32_t data;
    } message_t;

    osal_queue_handle_t queue;
    osal_queue_create(sizeof(message_t), 10, &queue);

    /* Send message */
    message_t msg = { .id = 1, .data = 42 };
    osal_queue_send(queue, &msg, OSAL_WAIT_FOREVER);

    /* Send to front (high priority) */
    osal_queue_send_front(queue, &msg, 1000);

    /* Receive message */
    message_t received;
    osal_queue_receive(queue, &received, OSAL_WAIT_FOREVER);

    /* Check available messages */
    uint32_t count = osal_queue_get_count(queue);

    /* Delete */
    osal_queue_delete(queue);

Software Timer
--------------

**Create and use timer:**

.. code-block:: c

    void timer_callback(void* arg)
    {
        /* Timer expired */
    }

    osal_timer_handle_t timer;
    osal_timer_create("my_timer", timer_callback, NULL,
                      1000,  /* Period: 1000 ms */
                      true,  /* Auto-reload */
                      &timer);

    /* Start timer */
    osal_timer_start(timer);

    /* Stop timer */
    osal_timer_stop(timer);

    /* Change period */
    osal_timer_set_period(timer, 500);

    /* Delete */
    osal_timer_delete(timer);

Critical Sections
-----------------

**Disable/enable interrupts:**

.. code-block:: c

    osal_enter_critical();
    /* Interrupts disabled - keep this section short! */
    /* ... */
    osal_exit_critical();

**Nested critical sections:**

.. code-block:: c

    uint32_t state = osal_interrupt_disable();
    /* Interrupts disabled */
    /* ... */
    osal_interrupt_restore(state);

Memory Management
-----------------

**Dynamic allocation:**

.. code-block:: c

    void* ptr = osal_malloc(1024);
    if (ptr) {
        /* Use memory */
        osal_free(ptr);
    }

    /* Aligned allocation */
    void* aligned = osal_malloc_aligned(1024, 32);
    osal_free_aligned(aligned);

Time Functions
--------------

**Get system time:**

.. code-block:: c

    /* Get tick count */
    uint32_t ticks = osal_get_tick_count();

    /* Get milliseconds */
    uint32_t ms = osal_get_time_ms();

    /* Delay */
    osal_delay_ms(100);

Error Handling
--------------

All OSAL functions return ``osal_status_t``:

.. code-block:: c

    osal_status_t status = osal_mutex_create(&mutex);
    if (status != OSAL_OK) {
        /* Handle error */
    }

Common status codes:

- ``OSAL_OK`` - Success
- ``OSAL_ERR_PARAM`` - Invalid parameter
- ``OSAL_ERR_TIMEOUT`` - Operation timeout
- ``OSAL_ERR_NO_MEM`` - Out of memory
- ``OSAL_ERR_ISR`` - Called from ISR context

Bare-metal Considerations
-------------------------

When using the bare-metal backend:

* Tasks run cooperatively (no preemption)
* ``osal_task_delay()`` uses busy-wait
* Mutexes use simple flags (no priority inheritance)
* Queues use ring buffers
* Call ``osal_scheduler_run()`` in main loop

.. code-block:: c

    int main(void)
    {
        osal_init();

        /* Create tasks */
        osal_task_create(&task1_cfg, &task1);
        osal_task_create(&task2_cfg, &task2);

        /* Run scheduler (bare-metal) */
        osal_scheduler_run();

        return 0;
    }

API Reference
-------------

See :doc:`../api/osal` for complete API documentation.
