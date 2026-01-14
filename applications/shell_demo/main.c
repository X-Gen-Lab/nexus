/**
 * \file            main.c
 * \brief           Shell Demo Example Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the Shell/CLI middleware
 *                  on STM32F4 Discovery board. It provides an interactive
 *                  command-line interface over UART2.
 *
 * \note            UART2 pins: PA2 (TX), PA3 (RX)
 *                  Connect to a serial terminal at 115200 baud.
 */

#include "hal/hal.h"
#include "shell/shell.h"
#include "shell/shell_command.h"
#include "shell/shell_backend.h"
#include <string.h>
#include <stdlib.h>

/*---------------------------------------------------------------------------*/
/* Pin Definitions                                                           */
/*---------------------------------------------------------------------------*/

/** LED pins on STM32F4 Discovery */
#define LED_GREEN_PORT      HAL_GPIO_PORT_D
#define LED_GREEN_PIN       12
#define LED_ORANGE_PORT     HAL_GPIO_PORT_D
#define LED_ORANGE_PIN      13
#define LED_RED_PORT        HAL_GPIO_PORT_D
#define LED_RED_PIN         14
#define LED_BLUE_PORT       HAL_GPIO_PORT_D
#define LED_BLUE_PIN        15

/** User button */
#define USER_BTN_PORT       HAL_GPIO_PORT_A
#define USER_BTN_PIN        0

/*---------------------------------------------------------------------------*/
/* Custom Command Handlers                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           LED control command
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success, non-zero on error
 */
static int cmd_led(int argc, char* argv[]) {
    if (argc < 3) {
        shell_printf("Usage: led <color> <on|off|toggle>\n");
        shell_printf("  color: green, orange, red, blue, all\n");
        return 1;
    }

    const char* color = argv[1];
    const char* action = argv[2];
    
    hal_gpio_port_t port = LED_GREEN_PORT;
    hal_gpio_pin_t pin = LED_GREEN_PIN;
    bool all_leds = false;
    
    /* Determine which LED */
    if (strcmp(color, "green") == 0) {
        port = LED_GREEN_PORT;
        pin = LED_GREEN_PIN;
    } else if (strcmp(color, "orange") == 0) {
        port = LED_ORANGE_PORT;
        pin = LED_ORANGE_PIN;
    } else if (strcmp(color, "red") == 0) {
        port = LED_RED_PORT;
        pin = LED_RED_PIN;
    } else if (strcmp(color, "blue") == 0) {
        port = LED_BLUE_PORT;
        pin = LED_BLUE_PIN;
    } else if (strcmp(color, "all") == 0) {
        all_leds = true;
    } else {
        shell_printf("Unknown color: %s\n", color);
        return 1;
    }
    
    /* Perform action */
    if (strcmp(action, "on") == 0) {
        if (all_leds) {
            hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_HIGH);
            hal_gpio_write(LED_ORANGE_PORT, LED_ORANGE_PIN, HAL_GPIO_LEVEL_HIGH);
            hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_HIGH);
            hal_gpio_write(LED_BLUE_PORT, LED_BLUE_PIN, HAL_GPIO_LEVEL_HIGH);
        } else {
            hal_gpio_write(port, pin, HAL_GPIO_LEVEL_HIGH);
        }
        shell_printf("LED %s ON\n", color);
    } else if (strcmp(action, "off") == 0) {
        if (all_leds) {
            hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_LOW);
            hal_gpio_write(LED_ORANGE_PORT, LED_ORANGE_PIN, HAL_GPIO_LEVEL_LOW);
            hal_gpio_write(LED_RED_PORT, LED_RED_PIN, HAL_GPIO_LEVEL_LOW);
            hal_gpio_write(LED_BLUE_PORT, LED_BLUE_PIN, HAL_GPIO_LEVEL_LOW);
        } else {
            hal_gpio_write(port, pin, HAL_GPIO_LEVEL_LOW);
        }
        shell_printf("LED %s OFF\n", color);
    } else if (strcmp(action, "toggle") == 0) {
        if (all_leds) {
            hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
            hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
            hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
            hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
        } else {
            hal_gpio_toggle(port, pin);
        }
        shell_printf("LED %s toggled\n", color);
    } else {
        shell_printf("Unknown action: %s\n", action);
        return 1;
    }
    
    return 0;
}

/**
 * \brief           Button status command
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int cmd_button(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    hal_gpio_level_t level = hal_gpio_read(USER_BTN_PORT, USER_BTN_PIN);
    shell_printf("User button: %s\n", level == HAL_GPIO_LEVEL_HIGH ? "PRESSED" : "RELEASED");
    
    return 0;
}

/**
 * \brief           System tick command
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int cmd_tick(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uint32_t tick = hal_get_tick();
    shell_printf("System tick: %lu ms\n", (unsigned long)tick);
    
    return 0;
}

/**
 * \brief           Delay command
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 */
static int cmd_delay(int argc, char* argv[]) {
    if (argc < 2) {
        shell_printf("Usage: delay <ms>\n");
        return 1;
    }
    
    uint32_t ms = (uint32_t)atoi(argv[1]);
    shell_printf("Delaying %lu ms...\n", (unsigned long)ms);
    hal_delay_ms(ms);
    shell_printf("Done\n");
    
    return 0;
}

/**
 * \brief           Reboot command
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success (never returns)
 */
static int cmd_reboot(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    shell_printf("Rebooting...\n");
    hal_delay_ms(100);  /* Allow message to be sent */
    hal_system_reset();
    
    return 0;  /* Never reached */
}

/*---------------------------------------------------------------------------*/
/* Command Completion Callbacks                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           LED color completion
 */
static void led_color_completion(const char* partial, char* completions[], int* count) {
    static char colors[][8] = {"green", "orange", "red", "blue", "all"};
    size_t partial_len = strlen(partial);
    *count = 0;
    
    for (int i = 0; i < 5 && *count < SHELL_MAX_COMPLETIONS; i++) {
        if (strncmp(colors[i], partial, partial_len) == 0) {
            completions[*count] = colors[i];
            (*count)++;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Command Definitions                                                       */
/*---------------------------------------------------------------------------*/

static const shell_command_t cmd_led_def = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LEDs on the board",
    .usage = "led <color> <on|off|toggle>",
    .completion = led_color_completion
};

static const shell_command_t cmd_button_def = {
    .name = "button",
    .handler = cmd_button,
    .help = "Read user button status",
    .usage = "button",
    .completion = NULL
};

static const shell_command_t cmd_tick_def = {
    .name = "tick",
    .handler = cmd_tick,
    .help = "Show system tick count",
    .usage = "tick",
    .completion = NULL
};

static const shell_command_t cmd_delay_def = {
    .name = "delay",
    .handler = cmd_delay,
    .help = "Delay for specified milliseconds",
    .usage = "delay <ms>",
    .completion = NULL
};

static const shell_command_t cmd_reboot_def = {
    .name = "reboot",
    .handler = cmd_reboot,
    .help = "Reboot the system",
    .usage = "reboot",
    .completion = NULL
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize LEDs
 * \return          HAL_OK on success
 */
static hal_status_t led_init(void) {
    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,
        .pull        = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .speed       = HAL_GPIO_SPEED_LOW,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };

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
 * \brief           Initialize user button
 * \return          HAL_OK on success
 */
static hal_status_t button_init(void) {
    hal_gpio_config_t config = {
        .direction   = HAL_GPIO_DIR_INPUT,
        .pull        = HAL_GPIO_PULL_DOWN,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .speed       = HAL_GPIO_SPEED_LOW,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };

    return hal_gpio_init(USER_BTN_PORT, USER_BTN_PIN, &config);
}

/**
 * \brief           Initialize shell
 * \return          SHELL_OK on success
 */
static shell_status_t shell_app_init(void) {
    shell_status_t status;
    
    /* Configure UART for shell I/O */
    hal_uart_config_t uart_config = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };
    
    if (hal_uart_init(HAL_UART_1, &uart_config) != HAL_OK) {
        return SHELL_ERROR;
    }
    
    /* Initialize UART backend */
    status = shell_uart_backend_init(HAL_UART_1);
    if (status != SHELL_OK) {
        return status;
    }
    
    /* Configure shell */
    shell_config_t config = {
        .prompt = "nexus> ",
        .cmd_buffer_size = 128,
        .history_depth = 16,
        .max_commands = 32
    };
    
    /* Initialize shell */
    status = shell_init(&config);
    if (status != SHELL_OK) {
        return status;
    }
    
    /* Set UART backend */
    status = shell_set_backend(&shell_uart_backend);
    if (status != SHELL_OK) {
        shell_deinit();
        return status;
    }
    
    /* Register built-in commands */
    shell_register_builtin_commands();
    
    /* Register custom commands */
    shell_register_command(&cmd_led_def);
    shell_register_command(&cmd_button_def);
    shell_register_command(&cmd_tick_def);
    shell_register_command(&cmd_delay_def);
    shell_register_command(&cmd_reboot_def);
    
    return SHELL_OK;
}

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 * \return          Should never return
 */
int main(void) {
    /* Initialize HAL system */
    hal_system_init();
    
    /* Initialize peripherals */
    if (led_init() != HAL_OK) {
        /* Error: blink red LED */
        while (1) {
            hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
            hal_delay_ms(100);
        }
    }
    
    if (button_init() != HAL_OK) {
        /* Error: blink orange LED */
        while (1) {
            hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
            hal_delay_ms(100);
        }
    }
    
    /* Initialize shell */
    if (shell_app_init() != SHELL_OK) {
        /* Error: blink blue LED */
        while (1) {
            hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
            hal_delay_ms(100);
        }
    }
    
    /* Print welcome message */
    shell_printf("\r\n");
    shell_printf("========================================\r\n");
    shell_printf("  Nexus Shell Demo v%s\r\n", shell_get_version());
    shell_printf("  STM32F4 Discovery Board\r\n");
    shell_printf("  Type 'help' for available commands\r\n");
    shell_printf("========================================\r\n");
    shell_print_prompt();
    
    /* Turn on green LED to indicate ready */
    hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_HIGH);
    
    /* Main loop */
    while (1) {
        /* Process shell input (non-blocking) */
        shell_process();
    }
    
    return 0;
}
