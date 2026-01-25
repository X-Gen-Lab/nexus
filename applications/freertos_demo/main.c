/**
 * \file            main.c
 * \brief           FreeRTOS/OSAL Demo Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-25
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the OSAL (OS Abstraction Layer)
 *                  features with multi-task operation:
 *                  - Multi-task creation and management
 *                  - Mutex for resource protection
 *                  - Semaphore for task synchronization
 *                  - Message queue for inter-task communication
 *
 *                  The demo creates a producer-consumer pattern with LED
 *                  feedback:
 *                  - Producer task: generates sensor data and sends to queue
 *                  - Consumer task: receives data and processes it
 *                  - LED task: blinks LED as heartbeat indicator
 *                  - Stats task: periodically reports system statistics
 *
 * \note            Requires OSAL backend with multi-tasking support
 *                  (FreeRTOS, RT-Thread, or Zephyr).
 */

#include "hal/nx_hal.h"
#include "osal/osal.h"

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

#define TASK_STACK_SIZE     1024 /**< Stack size for tasks (in bytes) */
#define SENSOR_QUEUE_SIZE   10   /**< Queue capacity for sensor data */
#define LED_BLINK_PERIOD_MS 500  /**< LED blink period in milliseconds */
#define SENSOR_SAMPLE_PERIOD_MS                                                \
    100                      /**< Sensor sampling period in milliseconds */
#define STATS_PERIOD_MS 2000 /**< Statistics report period in milliseconds */

/*---------------------------------------------------------------------------*/
/* Data Structures                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sensor data message structure
 */
typedef struct {
    uint32_t timestamp; /**< Timestamp in milliseconds */
    uint32_t sensor_id; /**< Sensor identifier */
    int32_t value;      /**< Sensor reading value */
    uint8_t status;     /**< Sensor status (0=OK, 1=Warning, 2=Error) */
} sensor_data_t;

/**
 * \brief           System statistics structure
 */
typedef struct {
    uint32_t samples_produced; /**< Total samples produced */
    uint32_t samples_consumed; /**< Total samples consumed */
    uint32_t queue_overflows;  /**< Queue overflow count */
    uint32_t errors;           /**< Error count */
} system_stats_t;

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/

static osal_queue_handle_t g_sensor_queue =
    NULL; /**< Message queue for sensor data */
static osal_mutex_handle_t g_stats_mutex =
    NULL; /**< Mutex for protecting shared statistics */
static osal_sem_handle_t g_data_ready_sem =
    NULL; /**< Semaphore for signaling new data available */
static system_stats_t g_stats = {
    0}; /**< System statistics (protected by mutex) */
static volatile bool g_system_running = true; /**< System running flag */

static osal_task_handle_t g_producer_task = NULL; /**< Producer task handle */
static osal_task_handle_t g_consumer_task = NULL; /**< Consumer task handle */
static osal_task_handle_t g_led_task = NULL;      /**< LED task handle */
static osal_task_handle_t g_stats_task = NULL;    /**< Statistics task handle */

static nx_gpio_write_t* g_led0 = NULL; /**< LED 0 (heartbeat) */
static nx_gpio_write_t* g_led1 = NULL; /**< LED 1 (producer activity) */
static nx_gpio_write_t* g_led2 = NULL; /**< LED 2 (consumer activity) */
static nx_gpio_write_t* g_led3 = NULL; /**< LED 3 (error indicator) */

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Simulate sensor reading
 */
static int32_t simulate_sensor_reading(uint32_t sensor_id) {
    /* Simple pseudo-random value based on tick and sensor ID */
    uint32_t tick = osal_get_tick();
    return (int32_t)((tick * (sensor_id + 1)) % 1000);
}

/**
 * \brief           Update statistics safely
 * \details         Uses mutex to protect shared statistics structure
 */
static void update_stats(int produced, int consumed, int overflow, int error) {
    if (osal_mutex_lock(g_stats_mutex, 100) == OSAL_OK) {
        g_stats.samples_produced += produced;
        g_stats.samples_consumed += consumed;
        g_stats.queue_overflows += overflow;
        g_stats.errors += error;
        osal_mutex_unlock(g_stats_mutex);
    }
}

/*---------------------------------------------------------------------------*/
/* Task Functions                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Producer task - generates sensor data
 * \details         This task simulates sensor readings and sends them
 *                  to the message queue. It demonstrates:
 *                  - Periodic task execution with osal_task_delay()
 *                  - Queue send operations
 *                  - Semaphore signaling
 */
static void producer_task(void* arg) {
    (void)arg;
    uint32_t sample_count = 0;
    uint32_t sensor_id = 0;

    while (g_system_running) {
        /* Create sensor data message */
        sensor_data_t data = {
            .timestamp = osal_get_tick(),
            .sensor_id = sensor_id,
            .value = simulate_sensor_reading(sensor_id),
            .status = 0 /* OK */
        };

        /* Send to queue */
        osal_status_t status = osal_queue_send(g_sensor_queue, &data, 10);

        if (status == OSAL_OK) {
            /* Signal consumer that data is available */
            osal_sem_give(g_data_ready_sem);
            update_stats(1, 0, 0, 0);
            sample_count++;

            /* Toggle LED 1 on successful send */
            if ((sample_count % 10) == 0 && g_led1) {
                g_led1->toggle(g_led1);
            }
        } else if (status == OSAL_ERROR_FULL) {
            /* Queue overflow */
            update_stats(0, 0, 1, 0);
            if (g_led3) {
                g_led3->write(g_led3, 1);
            }
        } else {
            /* Other error */
            update_stats(0, 0, 0, 1);
        }

        /* Cycle through sensors */
        sensor_id = (sensor_id + 1) % 4;

        /* Wait for next sample period */
        osal_task_delay(SENSOR_SAMPLE_PERIOD_MS);
    }

    /* Task cleanup */
    osal_task_delete(NULL);
}

/**
 * \brief           Consumer task - processes sensor data
 * \details         This task receives sensor data from the queue and
 *                  processes it. It demonstrates:
 *                  - Semaphore wait for synchronization
 *                  - Queue receive operations
 *                  - Data processing
 */
static void consumer_task(void* arg) {
    (void)arg;
    sensor_data_t data;
    uint32_t process_count = 0;

    while (g_system_running) {
        /* Wait for data available signal */
        if (osal_sem_take(g_data_ready_sem, 500) == OSAL_OK) {
            /* Receive data from queue */
            if (osal_queue_receive(g_sensor_queue, &data, 10) == OSAL_OK) {
                /* Process the data (simulate processing time) */
                osal_task_delay(5);

                update_stats(0, 1, 0, 0);
                process_count++;

                /* Toggle LED 2 on successful processing */
                if ((process_count % 10) == 0 && g_led2) {
                    g_led2->toggle(g_led2);
                }

                /* Check for warning/error status */
                if (data.status > 0 && g_led3) {
                    g_led3->write(g_led3, 1);
                }
            }
        }
    }

    /* Task cleanup */
    osal_task_delete(NULL);
}

/**
 * \brief           LED task - heartbeat indicator
 * \details         This task blinks the LED as a heartbeat indicator.
 *                  It demonstrates simple periodic task execution.
 */
static void led_task(void* arg) {
    (void)arg;

    while (g_system_running) {
        /* Toggle LED 0 as heartbeat */
        if (g_led0) {
            g_led0->toggle(g_led0);
        }

        /* Wait for blink period */
        osal_task_delay(LED_BLINK_PERIOD_MS);
    }

    /* Turn off LED before exit */
    if (g_led0) {
        g_led0->write(g_led0, 0);
    }

    /* Task cleanup */
    osal_task_delete(NULL);
}

/**
 * \brief           Statistics task - reports system status
 * \details         This task periodically reports system statistics.
 *                  It demonstrates mutex usage for protecting shared data.
 */
static void stats_task(void* arg) {
    (void)arg;
    system_stats_t local_stats;

    while (g_system_running) {
        /* Wait for report period */
        osal_task_delay(STATS_PERIOD_MS);

        /* Get statistics snapshot with mutex protection */
        if (osal_mutex_lock(g_stats_mutex, 100) == OSAL_OK) {
            local_stats = g_stats;
            osal_mutex_unlock(g_stats_mutex);

            /* Report statistics (would normally go to UART/debug output) */
            /* For now, just check queue status */
            size_t queue_count = osal_queue_get_count(g_sensor_queue);

            /* Clear error LED if no recent errors */
            if (local_stats.errors == 0 && local_stats.queue_overflows == 0 &&
                g_led3) {
                g_led3->write(g_led3, 0);
            }

            /* Yield to other tasks */
            osal_task_yield();

            (void)queue_count; /* Suppress unused warning */
        }
    }

    /* Task cleanup */
    osal_task_delete(NULL);
}

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 * \details         Initializes the system and creates all tasks.
 *                  Demonstrates:
 *                  - OSAL initialization
 *                  - Resource creation (mutex, semaphore, queue)
 *                  - Task creation with different priorities
 *                  - Starting the OSAL scheduler
 */
int main(void) {
    osal_status_t status;

    /* Initialize OSAL */
    status = osal_init();
    if (status != OSAL_OK) {
        while (1) {
            /* Error state */
        }
    }

    /* Initialize HAL */
    if (nx_hal_init() != NX_OK) {
        while (1) {
            /* Error state */
        }
    }

    /* Get GPIO devices */
    g_led0 = nx_factory_gpio_write('A', 0);
    g_led1 = nx_factory_gpio_write('A', 1);
    g_led2 = nx_factory_gpio_write('A', 2);
    g_led3 = nx_factory_gpio_write('B', 0);

    /*-----------------------------------------------------------------------*/
    /* Create synchronization primitives                                     */
    /*-----------------------------------------------------------------------*/

    /* Create message queue for sensor data */
    status = osal_queue_create(sizeof(sensor_data_t), SENSOR_QUEUE_SIZE,
                               &g_sensor_queue);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /* Create mutex for statistics protection */
    status = osal_mutex_create(&g_stats_mutex);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /* Create semaphore for data ready signaling */
    status = osal_sem_create_counting(SENSOR_QUEUE_SIZE, 0, &g_data_ready_sem);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Create tasks                                                          */
    /*-----------------------------------------------------------------------*/

    /* Producer task - Normal priority */
    osal_task_config_t producer_config = {.name = "Producer",
                                          .func = producer_task,
                                          .arg = NULL,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = TASK_STACK_SIZE};
    status = osal_task_create(&producer_config, &g_producer_task);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /* Consumer task - High priority (process data quickly) */
    osal_task_config_t consumer_config = {.name = "Consumer",
                                          .func = consumer_task,
                                          .arg = NULL,
                                          .priority = OSAL_TASK_PRIORITY_HIGH,
                                          .stack_size = TASK_STACK_SIZE};
    status = osal_task_create(&consumer_config, &g_consumer_task);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /* LED task - Low priority (background heartbeat) */
    osal_task_config_t led_config = {.name = "LED",
                                     .func = led_task,
                                     .arg = NULL,
                                     .priority = OSAL_TASK_PRIORITY_LOW,
                                     .stack_size = TASK_STACK_SIZE};
    status = osal_task_create(&led_config, &g_led_task);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /* Statistics task - Low priority (periodic reporting) */
    osal_task_config_t stats_config = {.name = "Stats",
                                       .func = stats_task,
                                       .arg = NULL,
                                       .priority = OSAL_TASK_PRIORITY_LOW,
                                       .stack_size = TASK_STACK_SIZE};
    status = osal_task_create(&stats_config, &g_stats_task);
    if (status != OSAL_OK) {
        while (1) { /* Error */
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Start scheduler                                                       */
    /*-----------------------------------------------------------------------*/

    /* Start OSAL scheduler - this function does not return */
    osal_start();

    /* Should never reach here */
    return 0;
}
