/** Typed GPIO heartbeat using resolved board wiring and product startup. */
#include "hal/base/nx_device.h"
#include "product/product.h"
#include "nexus_board.h"
#include "nexus_config.h"
#include "osal/osal.h"
#include <stdio.h>

#if defined(NX_CONFIG_PLATFORM_NATIVE)
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#endif


#define BLINK_DELAY_MS 500U

static nx_device_ref_t led;

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
#endif

static void blink(void* unused) {
    (void)unused;
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    unsigned long completed = 0;
#endif
    for (;;) {
        if (nx_device_gpio_toggle(led) != NX_OK) {
#if defined(NX_CONFIG_PLATFORM_NATIVE)
            blink_failed = true;
#endif
            break;
        }
#if defined(NX_CONFIG_PLATFORM_NATIVE)
        if (cycles) {
            uint8_t state = 0;
            if (nx_device_gpio_read(led, &state) != NX_OK) { blink_failed = true; break; }
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
            (void)nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL);
            return;
        }
    }
    (void)nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL);
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
    if (nx_product_boot(NULL) != NX_OK) return startup_error("product boot");
    char name[16];
    int written = snprintf(name, sizeof(name), "GPIO%c%u", NX_BOARD_LED_GPIO_PORT,
                            (unsigned)NX_BOARD_LED_GPIO_PIN);
    if (written < 0 || (size_t)written >= sizeof(name) ||
        nx_device_open(name, NX_DEVICE_CLASS_GPIO, (uintptr_t)&led, &led) != NX_OK) {
        (void)nx_product_shutdown(NULL);
        return startup_error("configured GPIO open");
    }
    if (nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL) != NX_OK) {
        (void)nx_device_close(led);
        (void)nx_product_shutdown(NULL);
        return startup_error("GPIO safe level");
    }

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
        (void)nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL);
        (void)nx_device_close(led);
        (void)nx_product_shutdown(NULL);
        return startup_error("task creation");
    }
#if defined(NX_CONFIG_PLATFORM_NATIVE)
    if (cycles) {
        /* Native tasks start on pthread creation. Join this bounded smoke
         * run instead of entering the long-lived osal_start main loop. */
        if (osal_task_join(task, OSAL_WAIT_FOREVER) != OSAL_OK) return 1;
        if (osal_task_delete(task) != OSAL_OK) return 1;
        if (nx_device_close(led) != NX_OK) return 1;
        if (nx_product_shutdown(NULL) != NX_OK) return 1;
        return blink_failed ? 1 : 0;
    }
#endif
    osal_start();
#endif
    /* A stopped/failed scheduler or failed delay cannot be reported as a
     * successful application start. */
    (void)nx_device_gpio_write(led, NX_BOARD_LED_INACTIVE_LEVEL);
    (void)nx_device_close(led);
    (void)nx_product_shutdown(NULL);
    return 1;
}
