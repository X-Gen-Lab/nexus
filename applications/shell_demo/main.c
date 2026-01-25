/**
 * \file            main.c
 * \brief           Shell Demo Example Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-25
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the Shell/CLI framework with
 *                  interactive command-line interface over UART. It shows:
 *                  - HAL and OSAL initialization
 *                  - UART device usage for console I/O
 *                  - GPIO control via shell commands
 *                  - Custom command registration
 *
 * \note            UART0 is used for shell I/O (115200 baud).
 *                  GPIO pins are configured via Kconfig.
 */

#include "framework/shell/shell.h"
#include "hal/nx_hal.h"
#include "osal/osal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/

static nx_uart_t* g_uart = NULL; /**< UART device for shell I/O */

static nx_gpio_write_t* g_led0 = NULL; /**< LED 0 (GPIOA0) */
static nx_gpio_write_t* g_led1 = NULL; /**< LED 1 (GPIOA1) */
static nx_gpio_write_t* g_led2 = NULL; /**< LED 2 (GPIOA2) */
static nx_gpio_write_t* g_led3 = NULL; /**< LED 3 (GPIOB0) */

/*---------------------------------------------------------------------------*/
/* UART Output Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Print string to UART
 */
static void uart_print(const char* str) {
    if (g_uart) {
        nx_tx_sync_t* tx = g_uart->get_tx_sync(g_uart);
        if (tx) {
            tx->send(tx, (const uint8_t*)str, strlen(str), 1000);
        }
    }
}

/**
 * \brief           Print formatted string to UART
 */
static void uart_printf(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    uart_print(buf);
}

/*---------------------------------------------------------------------------*/
/* Custom Command Handlers                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           LED control command
 * \details         Controls LEDs (on/off/toggle)
 */
static int cmd_led(int argc, char* argv[]) {
    if (argc < 3) {
        uart_print("Usage: led <0|1|2|3|all> <on|off|toggle>\r\n");
        return 1;
    }

    const char* led_id = argv[1];
    const char* action = argv[2];

    nx_gpio_write_t* led = NULL;
    bool all_leds = false;

    /* Determine which LED */
    if (strcmp(led_id, "0") == 0) {
        led = g_led0;
    } else if (strcmp(led_id, "1") == 0) {
        led = g_led1;
    } else if (strcmp(led_id, "2") == 0) {
        led = g_led2;
    } else if (strcmp(led_id, "3") == 0) {
        led = g_led3;
    } else if (strcmp(led_id, "all") == 0) {
        all_leds = true;
    } else {
        uart_printf("Unknown LED: %s\r\n", led_id);
        return 1;
    }

    /* Perform action */
    if (strcmp(action, "on") == 0) {
        if (all_leds) {
            if (g_led0)
                g_led0->write(g_led0, 1);
            if (g_led1)
                g_led1->write(g_led1, 1);
            if (g_led2)
                g_led2->write(g_led2, 1);
            if (g_led3)
                g_led3->write(g_led3, 1);
        } else if (led) {
            led->write(led, 1);
        }
        uart_printf("LED %s ON\r\n", led_id);
    } else if (strcmp(action, "off") == 0) {
        if (all_leds) {
            if (g_led0)
                g_led0->write(g_led0, 0);
            if (g_led1)
                g_led1->write(g_led1, 0);
            if (g_led2)
                g_led2->write(g_led2, 0);
            if (g_led3)
                g_led3->write(g_led3, 0);
        } else if (led) {
            led->write(led, 0);
        }
        uart_printf("LED %s OFF\r\n", led_id);
    } else if (strcmp(action, "toggle") == 0) {
        if (all_leds) {
            if (g_led0)
                g_led0->toggle(g_led0);
            if (g_led1)
                g_led1->toggle(g_led1);
            if (g_led2)
                g_led2->toggle(g_led2);
            if (g_led3)
                g_led3->toggle(g_led3);
        } else if (led) {
            led->toggle(led);
        }
        uart_printf("LED %s toggled\r\n", led_id);
    } else {
        uart_printf("Unknown action: %s\r\n", action);
        return 1;
    }

    return 0;
}

/**
 * \brief           System tick command
 */
static int cmd_tick(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    uint32_t tick = osal_get_tick();
    uart_printf("System tick: %lu ms\r\n", (unsigned long)tick);

    return 0;
}

/**
 * \brief           Delay command
 */
static int cmd_delay(int argc, char* argv[]) {
    if (argc < 2) {
        uart_print("Usage: delay <ms>\r\n");
        return 1;
    }

    uint32_t ms = (uint32_t)atoi(argv[1]);
    uart_printf("Delaying %lu ms...\r\n", (unsigned long)ms);
    osal_task_delay(ms);
    uart_print("Done\r\n");

    return 0;
}

/**
 * \brief           HAL version command
 */
static int cmd_version(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    uart_printf("Nexus HAL Version: %s\r\n", nx_hal_get_version());
    uart_printf("OSAL Backend: %s\r\n", NX_CONFIG_OSAL_BACKEND_NAME);

    return 0;
}

/*---------------------------------------------------------------------------*/
/* Command Definitions                                                       */
/*---------------------------------------------------------------------------*/

static const shell_command_t cmd_led_def = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LEDs",
    .usage = "led <0|1|2|3|all> <on|off|toggle>",
    .completion = NULL};

static const shell_command_t cmd_tick_def = {.name = "tick",
                                             .handler = cmd_tick,
                                             .help = "Show system tick count",
                                             .usage = "tick",
                                             .completion = NULL};

static const shell_command_t cmd_delay_def = {
    .name = "delay",
    .handler = cmd_delay,
    .help = "Delay for specified milliseconds",
    .usage = "delay <ms>",
    .completion = NULL};

static const shell_command_t cmd_version_def = {.name = "version",
                                                .handler = cmd_version,
                                                .help =
                                                    "Show HAL and OSAL version",
                                                .usage = "version",
                                                .completion = NULL};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize shell
 * \details         Configures shell with UART backend and registers commands
 */
static int shell_app_init(void) {
    /* Configure shell */
    shell_config_t config = {.prompt = "nexus> ",
                             .cmd_buffer_size = 128,
                             .history_depth = 16,
                             .max_commands = 32};

    /* Initialize shell */
    if (shell_init(&config) != SHELL_OK) {
        return -1;
    }

    /* Register built-in commands */
    shell_register_builtin_commands();

    /* Register custom commands */
    shell_register_command(&cmd_led_def);
    shell_register_command(&cmd_tick_def);
    shell_register_command(&cmd_delay_def);
    shell_register_command(&cmd_version_def);

    return 0;
}

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 */
int main(void) {
    /* Initialize OSAL */
    if (osal_init() != OSAL_OK) {
        while (1) {
            /* OSAL initialization failed */
        }
    }

    /* Initialize HAL */
    if (nx_hal_init() != NX_OK) {
        while (1) {
            /* HAL initialization failed */
        }
    }

    /* Get UART device for shell I/O */
    g_uart = nx_factory_uart(0);
    if (!g_uart) {
        while (1) {
            /* UART device not available */
        }
    }

    /* Get GPIO devices */
    g_led0 = nx_factory_gpio_write('A', 0);
    g_led1 = nx_factory_gpio_write('A', 1);
    g_led2 = nx_factory_gpio_write('A', 2);
    g_led3 = nx_factory_gpio_write('B', 0);

    /* Initialize shell */
    if (shell_app_init() != 0) {
        while (1) {
            /* Shell initialization failed */
        }
    }

    /* Print welcome message */
    uart_print("\r\n");
    uart_print("========================================\r\n");
    uart_print("  Nexus Shell Demo\r\n");
    uart_printf("  HAL Version: %s\r\n", nx_hal_get_version());
    uart_print("  Type 'help' for available commands\r\n");
    uart_print("========================================\r\n");
    uart_print("nexus> ");

    /* Turn on LED 0 to indicate ready */
    if (g_led0) {
        g_led0->write(g_led0, 1);
    }

    /* Main loop */
    while (1) {
        /* Process shell input (non-blocking) */
        shell_process();

        /* Small delay to prevent busy-waiting */
        osal_task_delay(10);
    }

    return 0;
}
