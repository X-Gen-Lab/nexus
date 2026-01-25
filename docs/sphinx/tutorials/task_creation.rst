Multi-Tasking with OSAL
=======================

This tutorial teaches you how to create multi-tasking applications using the Nexus OSAL (OS Abstraction Layer). You'll learn how to create tasks, use synchronization primitives, and build concurrent applications.

Learning Objectives
-------------------

By the end of this tutorial, you will:

- Understand RTOS concepts and task scheduling
- Create and manage multiple tasks
- Use mutexes for resource protection
- Use semaphores for task synchronization
- Use message queues for inter-task communication
- Implement producer-consumer patterns

Prerequisites
-------------

- Completed :doc:`first_application`, :doc:`gpio_control`, and :doc:`uart_communication` tutorials
- STM32F4 Discovery board with FreeRTOS support
- Understanding of concurrent programming concepts

OSAL Overview
-------------

The Nexus OSAL provides a portable interface to RTOS features:

- **Tasks**: Independent threads of execution
- **Mutexes**: Mutual exclusion for protecting shared resources
- **Semaphores**: Signaling between tasks
- **Queues**: Message passing between tasks
- **Timers**: Software timers for periodic operations

The OSAL abstracts the underlying RTOS (FreeRTOS, RT-Thread, etc.), allowing your code to be portable.

Part 1: Creating Your First Task
--------------------------------

Task Creation Workflow
~~~~~~~~~~~~~~~~~~~~~~

The following diagram shows the workflow for creating and managing tasks:

.. mermaid::
   :alt: Task creation workflow showing initialization, task creation, and scheduler start

   flowchart TD
       START([Start]) --> INIT_HAL[Initialize HAL]
       INIT_HAL --> INIT_GPIO[Configure GPIO]
       INIT_GPIO --> INIT_OSAL[Initialize OSAL]
       INIT_OSAL --> CHECK_OSAL{OSAL Init OK?}

       CHECK_OSAL -->|No| ERROR[Error Handler]
       ERROR --> HALT[Halt System]

       CHECK_OSAL -->|Yes| CREATE_TASK[Create Task]
       CREATE_TASK --> CHECK_TASK{Task Created?}

       CHECK_TASK -->|No| ERROR
       CHECK_TASK -->|Yes| START_SCHED[Start OSAL Scheduler]

       START_SCHED --> TASK_RUN[Task Running]
       TASK_RUN --> TASK_WORK[Execute Task Code]
       TASK_WORK --> TASK_DELAY[Task Delay]
       TASK_DELAY --> TASK_RUN

       style START fill:#e1f5ff
       style INIT_HAL fill:#fff4e1
       style INIT_OSAL fill:#ffe1f5
       style CREATE_TASK fill:#e1ffe1
       style START_SCHED fill:#f5e1ff
       style TASK_RUN fill:#ffe1e1
       style ERROR fill:#ffcccc

Basic Task Creation
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/hal.h"
    #include "osal/osal.h"

    /**
     * \brief           LED blink task
     * \param[in]       arg: Task argument (unused)
     */
    static void led_task(void* arg) {
        (void)arg;  /* Unused */

        while (1) {
            /* Toggle LED */
            hal_gpio_toggle(HAL_GPIO_PORT_D, 12);

            /* Delay 500ms */
            osal_task_delay(500);
        }
    }

    int main(void) {
        /* Initialize HAL */
        hal_init();

        /* Initialize LED */
        hal_gpio_config_t led_config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_D, 12, &led_config);

        /* Initialize OSAL */
        if (osal_init() != OSAL_OK) {
            while (1) { /* Error */ }
        }

        /* Create task */
        osal_task_config_t task_config = {
            .name = "LED",
            .func = led_task,
            .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_NORMAL,
            .stack_size = 512
        };

        osal_task_handle_t task_handle;
        if (osal_task_create(&task_config, &task_handle) != OSAL_OK) {
            while (1) { /* Error */ }
        }

        /* Start OSAL scheduler - this function does not return */
        osal_start();

        return 0;
    }

**Key Points:**

- ``osal_init()`` initializes the OSAL subsystem
- ``osal_task_create()`` creates a new task
- ``osal_start()`` starts the scheduler (never returns)
- ``osal_task_delay()`` suspends the task for a specified time

Task Priorities
~~~~~~~~~~~~~~~

OSAL supports multiple priority levels:

- ``OSAL_TASK_PRIORITY_IDLE``: Lowest priority (background tasks)
- ``OSAL_TASK_PRIORITY_LOW``: Low priority
- ``OSAL_TASK_PRIORITY_NORMAL``: Normal priority (default)
- ``OSAL_TASK_PRIORITY_HIGH``: High priority
- ``OSAL_TASK_PRIORITY_REALTIME``: Highest priority (time-critical tasks)

Higher priority tasks preempt lower priority tasks.

Part 2: Multiple Tasks
----------------------

Creating Multiple Tasks
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #define TASK_STACK_SIZE  1024

    /**
     * \brief           Green LED task
     */
    static void green_led_task(void* arg) {
        (void)arg;

        while (1) {
            hal_gpio_toggle(HAL_GPIO_PORT_D, 12);  /* Green */
            osal_task_delay(500);
        }
    }

    /**
     * \brief           Orange LED task
     */
    static void orange_led_task(void* arg) {
        (void)arg;

        while (1) {
            hal_gpio_toggle(HAL_GPIO_PORT_D, 13);  /* Orange */
            osal_task_delay(300);
        }
    }

    /**
     * \brief           Red LED task
     */
    static void red_led_task(void* arg) {
        (void)arg;

        while (1) {
            hal_gpio_toggle(HAL_GPIO_PORT_D, 14);  /* Red */
            osal_task_delay(700);
        }
    }

    int main(void) {
        hal_init();

        /* Initialize all LEDs */
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_D, 12, &config);
        hal_gpio_init(HAL_GPIO_PORT_D, 13, &config);
        hal_gpio_init(HAL_GPIO_PORT_D, 14, &config);

        /* Initialize OSAL */
        osal_init();

        /* Create tasks */
        osal_task_config_t task_configs[] = {
            {.name = "Green", .func = green_led_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = TASK_STACK_SIZE},
            {.name = "Orange", .func = orange_led_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = TASK_STACK_SIZE},
            {.name = "Red", .func = red_led_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = TASK_STACK_SIZE}
        };

        for (size_t i = 0; i < 3; i++) {
            osal_task_handle_t handle;
            osal_task_create(&task_configs[i], &handle);
        }

        /* Start scheduler */
        osal_start();

        return 0;
    }

**Result**: All three LEDs blink independently at different rates.

Part 3: Mutexes for Resource Protection
---------------------------------------

The Problem: Shared Resources
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When multiple tasks access shared resources, race conditions can occur:

.. code-block:: c

    /* WRONG: No protection */
    static uint32_t shared_counter = 0;

    static void task1(void* arg) {
        while (1) {
            shared_counter++;  /* Race condition! */
            osal_task_delay(10);
        }
    }

    static void task2(void* arg) {
        while (1) {
            shared_counter++;  /* Race condition! */
            osal_task_delay(10);
        }
    }

The Solution: Mutexes
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    static uint32_t shared_counter = 0;
    static osal_mutex_handle_t counter_mutex;

    /**
     * \brief           Increment counter safely
     */
    static void increment_counter(void) {
        /* Lock mutex */
        if (osal_mutex_lock(counter_mutex, 1000) == OSAL_OK) {
            /* Critical section - only one task can be here */
            shared_counter++;

            /* Unlock mutex */
            osal_mutex_unlock(counter_mutex);
        }
    }

    static void task1(void* arg) {
        while (1) {
            increment_counter();
            osal_task_delay(10);
        }
    }

    static void task2(void* arg) {
        while (1) {
            increment_counter();
            osal_task_delay(10);
        }
    }

    int main(void) {
        hal_init();
        osal_init();

        /* Create mutex */
        if (osal_mutex_create(&counter_mutex) != OSAL_OK) {
            while (1) { /* Error */ }
        }

        /* Create tasks */
        osal_task_config_t config1 = {
            .name = "Task1", .func = task1, .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = 1024
        };
        osal_task_config_t config2 = {
            .name = "Task2", .func = task2, .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = 1024
        };

        osal_task_handle_t h1, h2;
        osal_task_create(&config1, &h1);
        osal_task_create(&config2, &h2);

        osal_start();
        return 0;
    }

**Key Points:**

- Always lock mutex before accessing shared resource
- Always unlock mutex after accessing shared resource
- Use timeout to avoid deadlocks
- Keep critical sections short

Part 4: Semaphores for Synchronization
--------------------------------------

Binary Semaphores
~~~~~~~~~~~~~~~~~

Use binary semaphores for signaling between tasks:

.. code-block:: c

    static osal_sem_handle_t button_sem;

    /**
     * \brief           Button monitoring task
     */
    static void button_task(void* arg) {
        (void)arg;

        while (1) {
            /* Check button state */
            if (hal_gpio_read(HAL_GPIO_PORT_A, 0) == HAL_GPIO_LEVEL_HIGH) {
                /* Button pressed - signal LED task */
                osal_sem_give(button_sem);

                /* Wait for button release */
                while (hal_gpio_read(HAL_GPIO_PORT_A, 0) == HAL_GPIO_LEVEL_HIGH) {
                    osal_task_delay(10);
                }
            }

            osal_task_delay(10);
        }
    }

    /**
     * \brief           LED control task
     */
    static void led_control_task(void* arg) {
        (void)arg;

        while (1) {
            /* Wait for button press signal */
            if (osal_sem_take(button_sem, OSAL_WAIT_FOREVER) == OSAL_OK) {
                /* Toggle LED */
                hal_gpio_toggle(HAL_GPIO_PORT_D, 12);
            }
        }
    }

    int main(void) {
        hal_init();

        /* Initialize button and LED */
        hal_gpio_config_t btn_config = {
            .direction = HAL_GPIO_DIR_INPUT,
            .pull = HAL_GPIO_PULL_DOWN,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_A, 0, &btn_config);

        hal_gpio_config_t led_config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_D, 12, &led_config);

        osal_init();

        /* Create binary semaphore */
        if (osal_sem_create_binary(&button_sem) != OSAL_OK) {
            while (1) { /* Error */ }
        }

        /* Create tasks */
        osal_task_config_t btn_task_config = {
            .name = "Button", .func = button_task, .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_HIGH, .stack_size = 1024
        };
        osal_task_config_t led_task_config = {
            .name = "LED", .func = led_control_task, .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = 1024
        };

        osal_task_handle_t h1, h2;
        osal_task_create(&btn_task_config, &h1);
        osal_task_create(&led_task_config, &h2);

        osal_start();
        return 0;
    }

Counting Semaphores
~~~~~~~~~~~~~~~~~~~

Use counting semaphores to track resource availability:

.. code-block:: c

    #define MAX_RESOURCES  5

    static osal_sem_handle_t resource_sem;

    /**
     * \brief           Acquire resource
     * \return          true if acquired, false if timeout
     */
    static bool acquire_resource(uint32_t timeout_ms) {
        return (osal_sem_take(resource_sem, timeout_ms) == OSAL_OK);
    }

    /**
     * \brief           Release resource
     */
    static void release_resource(void) {
        osal_sem_give(resource_sem);
    }

    int main(void) {
        hal_init();
        osal_init();

        /* Create counting semaphore with 5 resources */
        if (osal_sem_create_counting(MAX_RESOURCES, MAX_RESOURCES, &resource_sem) != OSAL_OK) {
            while (1) { /* Error */ }
        }

        /* Tasks can now acquire/release resources */
        /* ... */

        osal_start();
        return 0;
    }

Part 5: Message Queues
----------------------

Producer-Consumer Pattern
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           Sensor data structure
     */
    typedef struct {
        uint32_t timestamp;
        int32_t temperature;
        int32_t humidity;
    } sensor_data_t;

    #define QUEUE_SIZE  10

    static osal_queue_handle_t sensor_queue;

    /**
     * \brief           Producer task - reads sensors
     */
    static void producer_task(void* arg) {
        (void)arg;
        uint32_t sample_count = 0;

        while (1) {
            /* Read sensor data (simulated) */
            sensor_data_t data = {
                .timestamp = hal_get_tick(),
                .temperature = 25 + (sample_count % 10),
                .humidity = 60 + (sample_count % 20)
            };

            /* Send to queue */
            if (osal_queue_send(sensor_queue, &data, 100) == OSAL_OK) {
                /* Success */
                sample_count++;
            } else {
                /* Queue full */
                hal_gpio_write(HAL_GPIO_PORT_D, 14, HAL_GPIO_LEVEL_HIGH);  /* Red LED */
            }

            /* Sample every 1 second */
            osal_task_delay(1000);
        }
    }

    /**
     * \brief           Consumer task - processes data
     */
    static void consumer_task(void* arg) {
        (void)arg;
        sensor_data_t data;

        while (1) {
            /* Receive from queue */
            if (osal_queue_receive(sensor_queue, &data, OSAL_WAIT_FOREVER) == OSAL_OK) {
                /* Process data */
                /* In real application, would log or transmit data */

                /* Toggle LED to show activity */
                hal_gpio_toggle(HAL_GPIO_PORT_D, 12);  /* Green LED */

                /* Clear error LED */
                hal_gpio_write(HAL_GPIO_PORT_D, 14, HAL_GPIO_LEVEL_LOW);
            }
        }
    }

    int main(void) {
        hal_init();

        /* Initialize LEDs */
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_D, 12, &config);
        hal_gpio_init(HAL_GPIO_PORT_D, 14, &config);

        osal_init();

        /* Create queue */
        if (osal_queue_create(sizeof(sensor_data_t), QUEUE_SIZE, &sensor_queue) != OSAL_OK) {
            while (1) { /* Error */ }
        }

        /* Create tasks */
        osal_task_config_t producer_config = {
            .name = "Producer", .func = producer_task, .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = 1024
        };
        osal_task_config_t consumer_config = {
            .name = "Consumer", .func = consumer_task, .arg = NULL,
            .priority = OSAL_TASK_PRIORITY_HIGH, .stack_size = 1024
        };

        osal_task_handle_t h1, h2;
        osal_task_create(&producer_config, &h1);
        osal_task_create(&consumer_config, &h2);

        osal_start();
        return 0;
    }

Part 6: Complete Example
------------------------

Here's a complete multi-tasking application:

.. code-block:: c

    /**
     * \file            multitask_demo.c
     * \brief           Complete OSAL Multi-Tasking Demo
     */

    #include "hal/hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define TASK_STACK_SIZE     1024
    #define SENSOR_QUEUE_SIZE   10

    /*-----------------------------------------------------------------------*/
    /* Data Structures                                                       */
    /*-----------------------------------------------------------------------*/

    typedef struct {
        uint32_t timestamp;
        uint32_t sensor_id;
        int32_t value;
    } sensor_msg_t;

    typedef struct {
        uint32_t samples_produced;
        uint32_t samples_consumed;
        uint32_t queue_overflows;
    } stats_t;

    /*-----------------------------------------------------------------------*/
    /* Global Variables                                                      */
    /*-----------------------------------------------------------------------*/

    static osal_queue_handle_t g_sensor_queue;
    static osal_mutex_handle_t g_stats_mutex;
    static osal_sem_handle_t g_data_ready_sem;
    static stats_t g_stats = {0};

    /*-----------------------------------------------------------------------*/
    /* Helper Functions                                                      */
    /*-----------------------------------------------------------------------*/

    static void update_stats(int produced, int consumed, int overflow) {
        if (osal_mutex_lock(g_stats_mutex, 100) == OSAL_OK) {
            g_stats.samples_produced += produced;
            g_stats.samples_consumed += consumed;
            g_stats.queue_overflows += overflow;
            osal_mutex_unlock(g_stats_mutex);
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Tasks                                                                 */
    /*-----------------------------------------------------------------------*/

    static void producer_task(void* arg) {
        (void)arg;
        uint32_t sensor_id = 0;

        while (1) {
            sensor_msg_t msg = {
                .timestamp = hal_get_tick(),
                .sensor_id = sensor_id,
                .value = (int32_t)(hal_get_tick() % 1000)
            };

            if (osal_queue_send(g_sensor_queue, &msg, 10) == OSAL_OK) {
                osal_sem_give(g_data_ready_sem);
                update_stats(1, 0, 0);
            } else {
                update_stats(0, 0, 1);
                hal_gpio_write(HAL_GPIO_PORT_D, 14, HAL_GPIO_LEVEL_HIGH);
            }

            sensor_id = (sensor_id + 1) % 4;
            osal_task_delay(100);
        }
    }

    static void consumer_task(void* arg) {
        (void)arg;
        sensor_msg_t msg;

        while (1) {
            if (osal_sem_take(g_data_ready_sem, 500) == OSAL_OK) {
                if (osal_queue_receive(g_sensor_queue, &msg, 10) == OSAL_OK) {
                    update_stats(0, 1, 0);
                    hal_gpio_toggle(HAL_GPIO_PORT_D, 13);
                }
            }
        }
    }

    static void heartbeat_task(void* arg) {
        (void)arg;

        while (1) {
            hal_gpio_toggle(HAL_GPIO_PORT_D, 12);
            osal_task_delay(500);
        }
    }

    static void stats_task(void* arg) {
        (void)arg;
        stats_t local_stats;

        while (1) {
            osal_task_delay(2000);

            if (osal_mutex_lock(g_stats_mutex, 100) == OSAL_OK) {
                local_stats = g_stats;
                osal_mutex_unlock(g_stats_mutex);

                /* In real app, would print stats via UART */
                if (local_stats.queue_overflows == 0) {
                    hal_gpio_write(HAL_GPIO_PORT_D, 14, HAL_GPIO_LEVEL_LOW);
                }
            }
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        hal_init();

        /* Initialize LEDs */
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_D, 12, &config);
        hal_gpio_init(HAL_GPIO_PORT_D, 13, &config);
        hal_gpio_init(HAL_GPIO_PORT_D, 14, &config);

        /* Initialize OSAL */
        osal_init();

        /* Create synchronization objects */
        osal_queue_create(sizeof(sensor_msg_t), SENSOR_QUEUE_SIZE, &g_sensor_queue);
        osal_mutex_create(&g_stats_mutex);
        osal_sem_create_counting(SENSOR_QUEUE_SIZE, 0, &g_data_ready_sem);

        /* Create tasks */
        osal_task_config_t tasks[] = {
            {.name = "Producer", .func = producer_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_NORMAL, .stack_size = TASK_STACK_SIZE},
            {.name = "Consumer", .func = consumer_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_HIGH, .stack_size = TASK_STACK_SIZE},
            {.name = "Heartbeat", .func = heartbeat_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_LOW, .stack_size = TASK_STACK_SIZE},
            {.name = "Stats", .func = stats_task, .arg = NULL,
             .priority = OSAL_TASK_PRIORITY_LOW, .stack_size = TASK_STACK_SIZE}
        };

        for (size_t i = 0; i < 4; i++) {
            osal_task_handle_t handle;
            osal_task_create(&tasks[i], &handle);
        }

        /* Start scheduler */
        osal_start();

        return 0;
    }

Best Practices
--------------

1. **Task Design**: Keep tasks focused on a single responsibility

2. **Stack Size**: Allocate sufficient stack for each task (monitor usage)

3. **Priority Assignment**: Use appropriate priorities (avoid priority inversion)

4. **Synchronization**: Always protect shared resources with mutexes

5. **Deadlock Prevention**:
   - Always acquire mutexes in the same order
   - Use timeouts
   - Keep critical sections short

6. **Queue Sizing**: Size queues appropriately for your data rate

7. **Error Handling**: Always check return values from OSAL functions

8. **Task Cleanup**: Tasks should run forever or call ``osal_task_delete()`` before returning

Common Issues
-------------

**Stack Overflow:**

Symptoms: System crashes, hard faults
Solution: Increase stack size, reduce local variables, avoid deep recursion

**Priority Inversion:**

Symptoms: High-priority task blocked by low-priority task
Solution: Use priority inheritance mutexes

**Deadlock:**

Symptoms: System hangs
Solution: Always acquire locks in same order, use timeouts

**Queue Overflow:**

Symptoms: Data loss
Solution: Increase queue size, process data faster, add flow control

Next Steps
----------

- Explore the :doc:`../user_guide/osal` for complete OSAL API reference
- Check out the FreeRTOS demo application in ``applications/freertos_demo/``
- Learn about :doc:`../user_guide/log` for logging from multiple tasks
- Read about :doc:`../platform_guides/stm32f4` for platform-specific details

