/**
 * \file            shell_example.c
 * \brief           Shell/CLI Middleware Usage Example
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This example demonstrates how to use the Shell/CLI middleware
 * to create an interactive command-line interface for embedded systems.
 *
 * \section shell_example_features Features Demonstrated
 * - Shell initialization and configuration
 * - Custom command registration
 * - Command with arguments
 * - Command with argument completion
 * - Using the UART backend
 * - Main loop integration
 *
 * \section shell_example_usage Usage
 * 1. Initialize the HAL UART
 * 2. Initialize the Shell with configuration
 * 3. Set the UART backend
 * 4. Register built-in and custom commands
 * 5. Call shell_process() in the main loop
 */

#include "shell/shell.h"
#include "shell/shell_command.h"
#include "shell/shell_backend.h"
#include "hal/hal_uart.h"

/*---------------------------------------------------------------------------*/
/* Custom Command Handlers                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           LED control command handler
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success, non-zero on error
 *
 * Usage: led <on|off|toggle>
 */
static int cmd_led(int argc, char* argv[]) {
    if (argc < 2) {
        shell_printf("Usage: led <on|off|toggle>\n");
        return 1;
    }

    if (strcmp(argv[1], "on") == 0) {
        shell_printf("LED turned ON\n");
        /* hal_gpio_write(LED_PORT, LED_PIN, HAL_GPIO_LEVEL_HIGH); */
    } else if (strcmp(argv[1], "off") == 0) {
        shell_printf("LED turned OFF\n");
        /* hal_gpio_write(LED_PORT, LED_PIN, HAL_GPIO_LEVEL_LOW); */
    } else if (strcmp(argv[1], "toggle") == 0) {
        shell_printf("LED toggled\n");
        /* hal_gpio_toggle(LED_PORT, LED_PIN); */
    } else {
        shell_printf("Unknown option: %s\n", argv[1]);
        return 1;
    }
    
    return 0;
}

/**
 * \brief           System information command handler
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success
 *
 * Usage: sysinfo
 */
static int cmd_sysinfo(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    shell_printf("System Information:\n");
    shell_printf("  Platform: Nexus Embedded\n");
    shell_printf("  Shell Version: %s\n", shell_get_version());
    shell_printf("  Commands: %d registered\n", shell_get_command_count());
    
    return 0;
}

/**
 * \brief           GPIO read/write command handler
 * \param[in]       argc: Argument count
 * \param[in]       argv: Argument array
 * \return          0 on success, non-zero on error
 *
 * Usage: gpio <port> <pin> [value]
 */
static int cmd_gpio(int argc, char* argv[]) {
    if (argc < 3) {
        shell_printf("Usage: gpio <port> <pin> [value]\n");
        shell_printf("  port: A, B, C, D\n");
        shell_printf("  pin: 0-15\n");
        shell_printf("  value: 0 or 1 (optional, for write)\n");
        return 1;
    }
    
    char port = argv[1][0];
    int pin = atoi(argv[2]);
    
    if (port < 'A' || port > 'D') {
        shell_printf("Invalid port: %c\n", port);
        return 1;
    }
    
    if (pin < 0 || pin > 15) {
        shell_printf("Invalid pin: %d\n", pin);
        return 1;
    }
    
    if (argc >= 4) {
        /* Write mode */
        int value = atoi(argv[3]);
        shell_printf("GPIO %c%d = %d\n", port, pin, value);
        /* hal_gpio_write(port - 'A', pin, value ? HAL_GPIO_LEVEL_HIGH : HAL_GPIO_LEVEL_LOW); */
    } else {
        /* Read mode */
        shell_printf("GPIO %c%d = (read not implemented in example)\n", port, pin);
        /* int value = hal_gpio_read(port - 'A', pin); */
    }
    
    return 0;
}

/*---------------------------------------------------------------------------*/
/* Command Completion Callback                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           LED command argument completion
 * \param[in]       partial: Partial argument string
 * \param[out]      completions: Array to store completions
 * \param[out]      count: Number of completions found
 */
static void led_completion(const char* partial, char* completions[], int* count) {
    static char options[][8] = {"on", "off", "toggle"};
    int partial_len = strlen(partial);
    *count = 0;
    
    for (int i = 0; i < 3 && *count < SHELL_MAX_COMPLETIONS; i++) {
        if (strncmp(options[i], partial, partial_len) == 0) {
            completions[*count] = options[i];
            (*count)++;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Command Definitions                                                       */
/*---------------------------------------------------------------------------*/

/** LED control command */
static const shell_command_t cmd_led_def = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control the LED",
    .usage = "led <on|off|toggle>",
    .completion = led_completion
};

/** System information command */
static const shell_command_t cmd_sysinfo_def = {
    .name = "sysinfo",
    .handler = cmd_sysinfo,
    .help = "Display system information",
    .usage = "sysinfo",
    .completion = NULL
};

/** GPIO control command */
static const shell_command_t cmd_gpio_def = {
    .name = "gpio",
    .handler = cmd_gpio,
    .help = "Read/write GPIO pins",
    .usage = "gpio <port> <pin> [value]",
    .completion = NULL
};

/*---------------------------------------------------------------------------*/
/* Main Application                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize the shell with custom commands
 * \return          SHELL_OK on success, error code otherwise
 */
shell_status_t shell_app_init(void) {
    shell_status_t status;
    
    /* Configure UART for shell I/O */
    hal_uart_config_t uart_config = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };
    
    if (hal_uart_init(HAL_UART_0, &uart_config) != HAL_OK) {
        return SHELL_ERROR;
    }
    
    /* Initialize UART backend */
    status = shell_uart_backend_init(HAL_UART_0);
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
    
    /* Register built-in commands (help, version, clear, history, echo) */
    status = shell_register_builtin_commands();
    if (status != SHELL_OK) {
        shell_deinit();
        return status;
    }
    
    /* Register custom commands */
    shell_register_command(&cmd_led_def);
    shell_register_command(&cmd_sysinfo_def);
    shell_register_command(&cmd_gpio_def);
    
    /* Print welcome message */
    shell_printf("\n");
    shell_printf("=================================\n");
    shell_printf("  Nexus Shell v%s\n", shell_get_version());
    shell_printf("  Type 'help' for commands\n");
    shell_printf("=================================\n");
    shell_print_prompt();
    
    return SHELL_OK;
}

/**
 * \brief           Main application entry point
 * \return          0 on success
 *
 * \note            This is a simplified example. In a real application,
 *                  you would integrate shell_process() into your main loop
 *                  or RTOS task.
 */
int main(void) {
    /* Initialize HAL */
    hal_init();
    
    /* Initialize shell application */
    if (shell_app_init() != SHELL_OK) {
        /* Handle initialization error */
        return 1;
    }
    
    /* Main loop */
    while (1) {
        /* Process shell input (non-blocking) */
        shell_process();
        
        /* Other application tasks can be performed here */
        /* ... */
    }
    
    /* Cleanup (never reached in this example) */
    shell_deinit();
    hal_deinit();
    
    return 0;
}

/*---------------------------------------------------------------------------*/
/* Example Session                                                           */
/*---------------------------------------------------------------------------*/

/**
 * \section shell_example_session Example Session
 *
 * \code
 * =================================
 *   Nexus Shell v1.0.0
 *   Type 'help' for commands
 * =================================
 * nexus> help
 * Available commands:
 *   help     - Show help information
 *   version  - Show shell version
 *   clear    - Clear the screen
 *   history  - Show command history
 *   echo     - Echo arguments
 *   led      - Control the LED
 *   sysinfo  - Display system information
 *   gpio     - Read/write GPIO pins
 *
 * nexus> led on
 * LED turned ON
 *
 * nexus> sysinfo
 * System Information:
 *   Platform: Nexus Embedded
 *   Shell Version: 1.0.0
 *   Commands: 8 registered
 *
 * nexus> gpio A 5 1
 * GPIO A5 = 1
 *
 * nexus> help led
 * led - Control the LED
 * Usage: led <on|off|toggle>
 *
 * nexus> led t<TAB>
 * nexus> led toggle
 * LED toggled
 * \endcode
 */
