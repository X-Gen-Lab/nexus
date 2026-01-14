/**
 * \file            main.c
 * \brief           FreeRTOS Adapter Demo Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the FreeRTOS OSAL adapter
 * features:
 *                  - Multi-task creation and management
 *                  - Mutex for resource protection
 *                  - Semaphore for task synchronization
 *                  - Message queue for inter-task communication
 *
 *                  The demo creates a producer-consumer pattern with LED
 * feedback:
 *                  - Producer task: generates sensor data and sends to queue
 *                  - Consumer task: receives data and processes it
 *                  - LED task: blinks LED based on system status
 *                  - Stats task: periodically reports system statistics
 *
 * \note            Requirements: 2.4 - FreeRTOS adapter example
 */

#include "hal/hal.h"
#include "osal/osal.h"

/*---------------------------------------------------------------------------*/
/* Configuration                                                             */
/*---------------------------------------------------------------------------*/

/** \brief Stack size for tasks (in bytes) */
#define TASK_STACK_SIZE 1024

/** \brief Queue capacity for sensor data */
#define SENSOR_QUEUE_SIZE 10

/** \brief LED blink period in milliseconds */
#define LED_BLINK_PERIOD_MS 500

/** \brief Sensor sampling period in milliseconds */
#define SENSOR_SAMPLE_PERIOD_MS 100

/** \brief Statistics report period in milliseconds */
#define STATS_PERIOD_MS 2000

/*---------------------------------------------------------------------------*/
/* LED Pin Definitions (STM32F4 Discovery)                                   */
/*---------------------------------------------------------------------------*/

#define LED_GREEN_PORT  HAL_GPIO_PORT_D
#define LED_GREEN_PIN   12
#define LED_ORANGE_PORT HAL_GPIO_PORT_D
#define LED_ORANGE_PIN  13
#define LED_RED_PORT    HAL_GPIO_PORT_D
#define LED_RED_PIN     14
#define LED_BLUE_PORT   HAL_GPIO_PORT_D
#define LED_BLUE_PIN    15

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

/** \brief Message queue for sensor data */
static osal_queue_handle_t g_sensor_queue = NULL;

/** \brief Mutex for protecting shared statistics */
static osal_mutex_handle_t g_stats_mutex = NULL;

/** \brief Semaphore for signaling new data available */
static osal_sem_handle_t g_data_ready_sem = NULL;

/** \brief System statistics (protected by mutex) */
static system_stats_t g_stats = {0};

/** \brief System running flag */
static volatile bool g_system_running = true;

/** \brief Task handles */
static osal_task_handle_t g_producer_task = NULL;
static osal_task_handle_t g_consumer_task = NULL;
static osal_task_handle_t g_led_task = NULL;
static osal_task_handle_t g_stats_task = NULL;

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize LEDs
 * \return          HAL_OK on success
 */
static hal_status_t led_init(void) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    if (hal_gpio_init(LED_GREEN_PORT, LED_GREEN_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_ORANGE_PORT, LED_ORANGE_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_RED_PORT, LED_RED_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_BLUE_PORT, LED_BLUE_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }

    return HAL_OK;
}

/**
 * \brief           Simulate sensor reading
 * \param[in]       sensor_id: Sensor identifier
 * \return          Simulated sensor value
 */
static int32_t simulate_sensor_reading(uint32_t sensor_id) {
    /* Simple pseudo-random value based on tick and sensor ID */
    uint32_t tick = hal_get_tick();
    return (int32_t)((tick * (sensor_id + 1)) % 1000);
}

/**
 * \brief           Update statistics safely
 * \param[in]       produced: Increment produced count
 * \param[in]       consumed: Increment consumed count
 * \param[in]       overflow: Increment overflow count
 * \param[in]       error: Increment error count
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
 * \param[in]       arg: Task argument (unused)
 *
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
            .timestamp = hal_get_tick(),
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

            /* Toggle blue LED on successful send */
            if ((sample_count % 10) == 0) {
                hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
            }
        } else if (status == OSAL_ERROR_FULL) {
            /* Queue overflow */
            update_stats(0, 0, 1, 0);
            hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
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
 * \param[in]       arg: Task argument (unused)
 *
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

                /* Toggle orange LED on successful processing */
                if ((process_count % 10) == 0) {
                    hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
                }

                /* Check for warning/error status */
                if (data.status > 0) {
                    hal_gpio_write(LED_RED_PORT, LED_RED_PIN,
                                   HAL_GPIO_LEVEL_HIGH);
                }
            }
        }
    }

    /* Task cleanup */
    osal_task_delete(NULL);
}

/**
 * \brief           LED task - heartbeat indicator
 * \param[in]       arg: Task argument (unused)
 *
 * \details         This task blinks the green LED as a heartbeat indicator.
 *                  It demonstrates simple periodic task execution.
 */
static void led_task(void* arg) {
    (void)arg;

    while (g_system_running) {
        /* Toggle green LED as heartbeat */
        hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);

        /* Wait for blink period */
        osal_task_delay(LED_BLINK_PERIOD_MS);
    }

    /* Turn off LED before exit */
    hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_LOW);

    /* Task cleanup */
    osal_task_delete(NULL);
}

/**
 * \brief           Statistics task - reports system status
 * \param[in]       arg: Task argument (unused)
 *
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

            /* Clear red LED if no recent errors */
            if (local_stats.errors == 0 && local_stats.queue_overflows == 0) {
                hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_LOW);
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
/* Main Function                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 * \return          Should never return
 *
 * \details         Initializes the system and creates all tasks.
 *                  Demonstrates:
 *                  - OSAL initialization
 *                  - Resource creation (mutex, semaphore, queue)
 *                  - Task creation with different priorities
 *                  - Starting the OSAL scheduler
 */
int main(void) {
    osal_status_t status;

    /* Initialize HAL system */
    hal_system_init();

    /* Initialize LEDs */
    if (led_init() != HAL_OK) {
        /* Error: stay in infinite loop with red LED */
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
        while (1) {
            /* Error state */
        }
    }

    /* Initialize OSAL */
    status = osal_init();
    if (status != OSAL_OK) {
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
        while (1) {
            /* Error state */
        }
    }

    /*-----------------------------------------------------------------------*/
    /* Create synchronization primitives                                     */
    /*-----------------------------------------------------------------------*/

    /* Create message queue for sensor data */
    status = osal_queue_create(sizeof(sensor_data_t), SENSOR_QUEUE_SIZE,
                               &g_sensor_queue);
    if (status != OSAL_OK) {
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
        while (1) { /* Error */
        }
    }

    /* Create mutex for statistics protection */
    status = osal_mutex_create(&g_stats_mutex);
    if (status != OSAL_OK) {
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
        while (1) { /* Error */
        }
    }

    /* Create semaphore for data ready signaling */
    status = osal_sem_create_counting(SENSOR_QUEUE_SIZE, 0, &g_data_ready_sem);
    if (status != OSAL_OK) {
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
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
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
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
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
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
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
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
        hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
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
