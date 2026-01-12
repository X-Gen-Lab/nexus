OS Abstraction Layer (OSAL)
===========================

Overview
--------

OSAL provides a portable interface for RTOS primitives. Applications can switch
between different RTOS backends (FreeRTOS, RT-Thread, Zephyr) or run bare-metal
without code changes.

Supported Backends
------------------

+------------+-------------+
| Backend    | Status      |
+============+=============+
| Bare-metal | ✅ Supported |
+------------+-------------+
| FreeRTOS   | 🚧 Planned  |
+------------+-------------+
| RT-Thread  | 🚧 Planned  |
+------------+-------------+
| Zephyr     | 🚧 Planned  |
+------------+-------------+

Task Management
---------------

**Create a task:**

.. code-block:: c

    void my_task(void* arg)
    {
        while (1) {
            // Task code
            osal_task_delay(100);
        }
    }

    osal_task_config_t config = {
        .name       = "my_task",
        .func       = my_task,
        .arg        = NULL,
        .stack_size = 1024,
        .priority   = OSAL_PRIORITY_NORMAL
    };

    osal_task_handle_t handle;
    osal_task_create(&config, &handle);

Mutex
-----

**Usage:**

.. code-block:: c

    osal_mutex_handle_t mutex;
    osal_mutex_create(&mutex);

    // Lock
    osal_mutex_lock(mutex, OSAL_WAIT_FOREVER);

    // Critical section
    // ...

    // Unlock
    osal_mutex_unlock(mutex);

Semaphore
---------

**Binary semaphore:**

.. code-block:: c

    osal_sem_handle_t sem;
    osal_sem_create_binary(0, &sem);

    // Wait
    osal_sem_take(sem, OSAL_WAIT_FOREVER);

    // Signal
    osal_sem_give(sem);

**Counting semaphore:**

.. code-block:: c

    osal_sem_handle_t sem;
    osal_sem_create_counting(10, 0, &sem);  // max=10, initial=0

Message Queue
-------------

**Usage:**

.. code-block:: c

    typedef struct {
        uint32_t id;
        uint32_t data;
    } message_t;

    osal_queue_handle_t queue;
    osal_queue_create(sizeof(message_t), 10, &queue);

    // Send
    message_t msg = { .id = 1, .data = 42 };
    osal_queue_send(queue, &msg, OSAL_WAIT_FOREVER);

    // Receive
    message_t received;
    osal_queue_receive(queue, &received, OSAL_WAIT_FOREVER);

Critical Sections
-----------------

.. code-block:: c

    osal_enter_critical();
    // Interrupts disabled
    // ...
    osal_exit_critical();

API Reference
-------------

See :doc:`../api/osal` for complete API documentation.
