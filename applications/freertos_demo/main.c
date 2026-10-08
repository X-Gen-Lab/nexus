/** OSAL producer/consumer example for the selected board.
 * Queue waits are bounded; the queue owns data-ready synchronization. A
 * separate semaphore gates worker startup until all tasks/resources exist. */
#include "hal/base/nx_device.h"
#include "product/product.h"
#include "nexus_board.h"
#include "nexus_config.h"
#include "osal/osal.h"
#include <stdatomic.h>
#include <stdio.h>
#if defined(NX_CONFIG_PLATFORM_NATIVE)
#include <errno.h>
#include <stdlib.h>
#include <string.h>
static unsigned long run_ms;
#endif

#if !defined(NX_CONFIG_OSAL_FREERTOS) && !defined(NX_CONFIG_OSAL_NATIVE)
#error "The task demo requires the Native or FreeRTOS backend"
#endif


#define TASK_COUNT 4U
#define TASK_STACK_BYTES 1024U
#define QUEUE_CAPACITY 10U

typedef struct {
    uint32_t sequence;
    int32_t value;
} sample_t;

typedef struct {
    uint32_t produced;
    uint32_t consumed;
    uint32_t dropped;
} statistics_t;

static osal_queue_handle_t samples;
static osal_mutex_handle_t statistics_lock;
static osal_sem_handle_t start_gate;
static osal_task_handle_t workers[TASK_COUNT];
static nx_device_ref_t led;
static statistics_t statistics;
static atomic_bool stopping;

static void fail(osal_status_t status) {
    atomic_store(&stopping, true);
    OSAL_REPORT_ERROR(status);
}

static bool should_stop(void) {
    return atomic_load(&stopping) || osal_task_should_stop();
}

static bool await_start(void) {
    while (!should_stop()) {
        osal_status_t status = osal_sem_take(start_gate, 100U);
        if (status == OSAL_OK) {
            return !should_stop();
        }
        if (status != OSAL_ERROR_TIMEOUT) {
            fail(status);
            return false;
        }
    }
    return false;
}

static bool release_start_gate(void) {
    for (size_t i = 0; i < TASK_COUNT; ++i) {
        osal_status_t status = osal_sem_give(start_gate);
        if (status != OSAL_OK) { fail(status); return false; }
    }
    return true;
}

static bool record(uint32_t produced, uint32_t consumed, uint32_t dropped) {
    osal_status_t status = osal_mutex_lock(statistics_lock, 50U);
    if (status != OSAL_OK) {
        fail(status);
        return false;
    }
    statistics.produced += produced;
    statistics.consumed += consumed;
    statistics.dropped += dropped;
    status = osal_mutex_unlock(statistics_lock);
    if (status != OSAL_OK) {
        fail(status);
        return false;
    }
    return true;
}

static void produce(void* unused) {
    (void)unused;
#if defined(NX_CONFIG_OSAL_FREERTOS)
    /* Bootstrap created every task before starting the scheduler. Release
     * operational semaphores from this scheduled coordinator, never main. */
    if (!release_start_gate()) return;
#endif
    if (!await_start()) return;
    uint32_t sequence = 0;
    while (!should_stop()) {
        uint32_t current = sequence++;
        sample_t sample = {.sequence = current,
                           .value = (int32_t)(current % 1000U)};
        osal_status_t status = osal_queue_send(samples, &sample, OSAL_NO_WAIT);
        if (status == OSAL_OK) {
            if (!record(1, 0, 0)) break;
        } else if (status == OSAL_ERROR_FULL) {
            if (!record(0, 0, 1)) break;
        } else {
            fail(status);
            break;
        }
        status = osal_task_delay(100U);
        if (status != OSAL_OK) {
            if (status != OSAL_ERROR_CANCELLED) fail(status);
            break;
        }
    }
}

static void consume(void* unused) {
    (void)unused;
    if (!await_start()) return;
    uint32_t previous = 0;
    bool have_previous = false;
    while (!should_stop()) {
        sample_t sample;
        osal_status_t status = osal_queue_receive(samples, &sample, 100U);
        if (status == OSAL_ERROR_TIMEOUT) continue;
        if (status != OSAL_OK) {
            if (status != OSAL_ERROR_CANCELLED) fail(status);
            break;
        }
        /* Gaps are allowed when the producer drops on queue saturation.
         * Duplicates/reordering violate the queue contract. */
        if (have_previous && (int32_t)(sample.sequence - previous) <= 0) {
            fail(OSAL_ERROR_INVALID_PARAM);
            break;
        }
        previous = sample.sequence;
        have_previous = true;
        if (!record(0, 1, 0)) break;
    }
}

static void heartbeat(void* unused) {
    (void)unused;
    if (!await_start()) return;
    /* This is the sole GPIO writer after startup. */
    while (!should_stop()) {
        if (nx_device_gpio_toggle(led) != NX_OK) { fail(OSAL_ERROR); break; }
        osal_status_t status = osal_task_delay(500U);
        if (status != OSAL_OK) {
            if (status != OSAL_ERROR_CANCELLED) fail(status);
            break;
        }
    }
    (void)nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL);
}

static void report(void* unused) {
    (void)unused;
    if (!await_start()) return;
    while (!should_stop()) {
        osal_status_t status = osal_task_delay(2000U);
        if (status != OSAL_OK) {
            if (status != OSAL_ERROR_CANCELLED) fail(status);
            break;
        }
        statistics_t snapshot;
        status = osal_mutex_lock(statistics_lock, 50U);
        if (status != OSAL_OK) { fail(status); break; }
        snapshot = statistics;
        status = osal_mutex_unlock(statistics_lock);
        if (status != OSAL_OK) { fail(status); break; }
        /* Transport output happens outside the shared statistics mutex. */
        printf("[%s] produced=%lu consumed=%lu dropped=%lu pending=%lu\n",
               NX_BOARD_NAME, (unsigned long)snapshot.produced,
               (unsigned long)snapshot.consumed, (unsigned long)snapshot.dropped,
               (unsigned long)osal_queue_get_count(samples));
#if defined(NX_CONFIG_PLATFORM_NATIVE)
        fflush(stdout);
#endif
    }
}

static int stop_and_cleanup(int result) {
    atomic_store(&stopping, true);
    for (size_t i = 0; i < TASK_COUNT; ++i) {
        if (workers[i]) (void)osal_task_request_stop(workers[i]);
    }
#if defined(NX_CONFIG_OSAL_NATIVE)
    /* Native workers may already exist before osal_start. Join them before
     * deleting any shared resource; retain resources if a join fails. */
    bool joined = true;
    for (size_t i = 0; i < TASK_COUNT; ++i) {
        if (workers[i] && osal_task_join(workers[i], 3000U) != OSAL_OK)
            joined = false;
    }
    if (joined) {
        if (result == 0 &&
            (statistics.produced == 0 || statistics.consumed == 0)) {
            result = 1;
        }
        for (size_t i = 0; i < TASK_COUNT; ++i) {
            if (workers[i] && osal_task_delete(workers[i]) != OSAL_OK)
                result = 1;
        }
        if (samples && osal_queue_delete(samples) != OSAL_OK) result = 1;
        if (start_gate && osal_sem_delete(start_gate) != OSAL_OK) result = 1;
        if (statistics_lock && osal_mutex_delete(statistics_lock) != OSAL_OK)
            result = 1;
        printf("[%s] stopped: produced=%lu consumed=%lu dropped=%lu\n",
               NX_BOARD_NAME, (unsigned long)statistics.produced,
               (unsigned long)statistics.consumed,
               (unsigned long)statistics.dropped);
        if (led.descriptor && nx_device_close(led) != NX_OK) result = 1;
        else led = (nx_device_ref_t){0};
        if (nx_product_shutdown(NULL) != NX_OK) result = 1;
    } else {
        result = 1;
    }
#endif
    /* A not-yet-started FreeRTOS scheduler cannot join ready tasks. Preserve
     * their resources and never start a partially configured application. */
    if (led.descriptor) (void)nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL);
    return result;
}

#if defined(NX_CONFIG_PLATFORM_NATIVE)
int main(int argc, char** argv) {
    if (argc != 1) {
        if (argc != 3 || strcmp(argv[1], "--run-ms") != 0) return 2;
        char* end = NULL;
        errno = 0;
        run_ms = strtoul(argv[2], &end, 10);
        if (argv[2][0] < '0' || argv[2][0] > '9' || *end != '\0' ||
            errno == ERANGE || run_ms < 100U || run_ms > 10000U) return 2;
    }
#else
int main(void) {
#endif
    if (nx_product_boot(NULL) != NX_OK) return 1;
    char name[16];
    int written = snprintf(name, sizeof(name), "GPIO%c%u", NX_BOARD_LED_GPIO_PORT,
                            (unsigned)NX_BOARD_LED_GPIO_PIN);
    if (written < 0 || (size_t)written >= sizeof(name) ||
        nx_device_open(name, NX_DEVICE_CLASS_GPIO, (uintptr_t)&led, &led) != NX_OK) {
        (void)nx_product_shutdown(NULL);
        return 1;
    }
    if (nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL) != NX_OK) return stop_and_cleanup(1);
    if (osal_queue_create(sizeof(sample_t), QUEUE_CAPACITY, &samples) != OSAL_OK ||
        osal_mutex_create(&statistics_lock) != OSAL_OK ||
        osal_sem_create_counting(TASK_COUNT, 0, &start_gate) != OSAL_OK)
        return stop_and_cleanup(1);

    const osal_task_config_t tasks[TASK_COUNT] = {
        {.name="Producer", .func=produce, .arg=NULL,
         .priority=OSAL_TASK_PRIORITY_NORMAL, .stack_size=TASK_STACK_BYTES},
        {.name="Consumer", .func=consume, .arg=NULL,
         .priority=OSAL_TASK_PRIORITY_HIGH, .stack_size=TASK_STACK_BYTES},
        {.name="Heartbeat", .func=heartbeat, .arg=NULL,
         .priority=OSAL_TASK_PRIORITY_LOW, .stack_size=TASK_STACK_BYTES},
        {.name="Statistics", .func=report, .arg=NULL,
         .priority=OSAL_TASK_PRIORITY_LOW, .stack_size=TASK_STACK_BYTES},
    };
    for (size_t i = 0; i < TASK_COUNT; ++i)
        if (osal_task_create(&tasks[i], &workers[i]) != OSAL_OK)
            return stop_and_cleanup(1);
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    if (!release_start_gate()) return stop_and_cleanup(1);
    if (run_ms) {
        osal_status_t status = osal_task_delay((uint32_t)run_ms);
        int result = status == OSAL_OK && !atomic_load(&stopping) ? 0 : 1;
        return stop_and_cleanup(result);
    }
#endif
    osal_start();
    return stop_and_cleanup(1);
}
