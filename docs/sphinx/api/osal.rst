OSAL API Reference
==================

This section documents the OS Abstraction Layer (OSAL) API.

Overview
--------

The Nexus OSAL provides a portable RTOS interface that works across multiple
backends including FreeRTOS, RT-Thread, and bare-metal. It provides a unified
API for tasks, synchronization primitives, and memory management.

Usage Examples
--------------

Task Creation
~~~~~~~~~~~~~

.. code-block:: c

    #include "osal/osal.h"

    /* Task function */
    void my_task(void* arg) {
        while (1) {
            /* Task work */
            osal_task_delay(1000);  /* Delay 1000ms */
        }
    }

    /* Create task */
    osal_task_t task;
    osal_task_config_t config = {
        .name = "MyTask",
        .stack_size = 2048,
        .priority = 5
    };

    osal_task_create(&task, my_task, NULL, &config);

Mutex Usage
~~~~~~~~~~~

.. code-block:: c

    #include "osal/osal.h"

    /* Create mutex */
    osal_mutex_t mutex;
    osal_mutex_create(&mutex);

    /* Lock mutex */
    if (osal_mutex_lock(&mutex, 1000) == OSAL_OK) {
        /* Critical section */

        /* Unlock mutex */
        osal_mutex_unlock(&mutex);
    }

    /* Delete mutex when done */
    osal_mutex_delete(&mutex);

Semaphore Usage
~~~~~~~~~~~~~~~

.. code-block:: c

    #include "osal/osal.h"

    /* Create binary semaphore */
    osal_sem_t sem;
    osal_sem_create(&sem, 0, 1);

    /* Wait for semaphore */
    if (osal_sem_wait(&sem, OSAL_WAIT_FOREVER) == OSAL_OK) {
        /* Semaphore acquired */
    }

    /* Signal semaphore */
    osal_sem_post(&sem);

    /* Delete semaphore */
    osal_sem_delete(&sem);

Queue Usage
~~~~~~~~~~~

.. code-block:: c

    #include "osal/osal.h"

    /* Create queue */
    osal_queue_t queue;
    osal_queue_create(&queue, 10, sizeof(int));

    /* Send to queue */
    int data = 42;
    osal_queue_send(&queue, &data, 1000);

    /* Receive from queue */
    int received;
    if (osal_queue_receive(&queue, &received, 1000) == OSAL_OK) {
        /* Process received data */
    }

    /* Delete queue */
    osal_queue_delete(&queue);

Timer Usage
~~~~~~~~~~~

.. code-block:: c

    #include "osal/osal.h"

    /* Timer callback */
    void timer_callback(void* arg) {
        /* Timer expired */
    }

    /* Create timer */
    osal_timer_t timer;
    osal_timer_config_t config = {
        .name = "MyTimer",
        .period_ms = 1000,
        .auto_reload = true
    };

    osal_timer_create(&timer, timer_callback, NULL, &config);

    /* Start timer */
    osal_timer_start(&timer);

    /* Stop timer */
    osal_timer_stop(&timer);

    /* Delete timer */
    osal_timer_delete(&timer);

Thread Safety
-------------

All OSAL functions are **thread-safe** and can be called from multiple tasks
simultaneously. The OSAL handles all necessary synchronization internally.

OSAL Definitions
----------------

.. doxygengroup:: OSAL_DEF
   :project: nexus
   :content-only:

Task API
--------

.. doxygengroup:: OSAL_TASK
   :project: nexus
   :content-only:

Mutex API
---------

.. doxygengroup:: OSAL_MUTEX
   :project: nexus
   :content-only:

Semaphore API
-------------

.. doxygengroup:: OSAL_SEM
   :project: nexus
   :content-only:

Queue API
---------

.. doxygengroup:: OSAL_QUEUE
   :project: nexus
   :content-only:

Related APIs
------------

- :doc:`hal` - Hardware abstraction layer
- :doc:`init` - Automatic initialization system
- :doc:`log` - Logging framework (uses OSAL for thread safety)
- :doc:`shell` - Shell framework (uses OSAL for task management)

See Also
--------

- :doc:`../user_guide/osal` - OSAL User Guide
- :doc:`../reference/error_codes` - Error Code Reference
- :doc:`../platform_guides/index` - Platform-Specific Guides
