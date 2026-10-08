/** GPIO heartbeat using board wiring and explicit backend startup. */
#include "hal/nx_hal.h"
#include "nexus_board.h"
#include "nexus_config.h"
#include "osal/osal.h"

#if defined(NX_CONFIG_PLATFORM_NATIVE)
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#if defined(NX_CONFIG_PLATFORM_STM32)
#include "boot/stm32_boot.h"
#endif

#define BLINK_DELAY_MS 500U

static nx_gpio_write_t* led;

static int startup_error(const char* stage) {
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    fprintf(stderr, "[%s] startup failed: %s\n", NX_BOARD_NAME, stage);
#else
    (void)stage;
#endif
    return 1;
}

#if defined(NX_CONFIG_PLATFORM_NATIVE)
static unsigned long cycles;
static bool blink_failed;
static nx_gpio_read_t* observer;
#endif

static void blink(void* unused) {
    (void)unused;
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    unsigned long completed = 0;
#endif
    for (;;) {
        led->toggle(led);
#if defined(NX_CONFIG_PLATFORM_NATIVE)
        if (cycles) {
            unsigned int state = observer->read(observer);
            ++completed;
            printf("[%s] cycle %lu P%c%u=%u\n", NX_BOARD_NAME, completed,
                   NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN, state);
            if (state != (completed & 1U)) {
                blink_failed = true;
                break;
            }
            if (completed == cycles) break;
        }
#endif
        if (osal_task_delay(BLINK_DELAY_MS) != OSAL_OK) {
#if defined(NX_CONFIG_PLATFORM_NATIVE)
            blink_failed = true;
#endif
            led->write(led, 0);
            return;
        }
    }
    led->write(led, 0);
}

#if defined(NX_CONFIG_PLATFORM_NATIVE)
int main(int argc, char** argv) {
    if (argc != 1) {
        if (argc != 3 || strcmp(argv[1], "--cycles") != 0) return 2;
        char* end = NULL;
        errno = 0;
        cycles = strtoul(argv[2], &end, 10);
        if (argv[2][0] < '0' || argv[2][0] > '9' || *end != '\0' ||
            errno == ERANGE || cycles == 0 || cycles > 1000U) return 2;
    }
#else
int main(void) {
#endif
#if defined(NX_CONFIG_PLATFORM_STM32)
    /* Establish board clock/interrupts and baremetal monotonic source before
     * OSAL initialization; HAL's generic weak hook is insufficient. */
    if (stm32_platform_init() != 0) {
        return startup_error("platform initialization");
    }
#endif
    if (osal_init() != OSAL_OK || nx_hal_init() != NX_OK) {
        return startup_error("OSAL/HAL initialization");
    }
    led = nx_factory_gpio_write(NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN);
    if (!led) {
        return startup_error("configured GPIO write capability");
    }
    nx_lifecycle_t* lifecycle = led->get_lifecycle(led);
    if (!lifecycle || lifecycle->init(lifecycle) != NX_OK) {
        return startup_error("GPIO lifecycle initialization");
    }
    led->write(led, 0);
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    observer = nx_factory_gpio_read(NX_BOARD_LED_GPIO_PORT,
                                    NX_BOARD_LED_GPIO_PIN);
    if (!observer) return startup_error("GPIO read capability");
#endif

#if defined(NX_CONFIG_OSAL_BAREMETAL)
    /* Baremetal has no scheduler or task-creation capability. */
    blink(NULL);
#else
    osal_task_handle_t task = NULL;
    const osal_task_config_t config = {
        .name = "Blinky",
        .func = blink,
        .arg = NULL,
        .priority = OSAL_TASK_PRIORITY_LOW,
        .stack_size = 1024U,
    };
    if (osal_task_create(&config, &task) != OSAL_OK) {
        led->write(led, 0);
        (void)lifecycle->deinit(lifecycle);
        return startup_error("task creation");
    }
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    if (cycles) {
        /* Native tasks start on pthread creation. Join this bounded smoke
         * run instead of entering the long-lived osal_start main loop. */
        if (osal_task_join(task, OSAL_WAIT_FOREVER) != OSAL_OK) return 1;
        if (osal_task_delete(task) != OSAL_OK) return 1;
        if (lifecycle->deinit(lifecycle) != NX_OK) return 1;
        return blink_failed ? 1 : 0;
    }
#endif
    osal_start();
#endif
    /* A stopped/failed scheduler or failed delay cannot be reported as a
     * successful application start. */
    led->write(led, 0);
    (void)lifecycle->deinit(lifecycle);
    return 1;
}
