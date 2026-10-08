/** Native standard-stream shell with one board LED and bounded smoke mode. */
#include "hal/nx_hal.h"
#include "hal/base/nx_device.h"
#include "product/product.h"
#include "nexus_board.h"
#include "nexus_config.h"
#include "osal/osal.h"
#include "shell/shell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(NX_CONFIG_PLATFORM_NATIVE)
#error "Shell demo needs a product-owned MCU console binding; use Native only"
#endif

#define INPUT_CAPACITY 128U
static nx_device_ref_t led;
static const char* input;
static size_t input_size;
static size_t input_offset;
static bool output_failed;
static bool quit_requested;
static unsigned int completed_commands;
static int last_command_status;

/* fgets blocks in the application, outside shell_process(). The backend only
 * consumes an already buffered line, so its read operation is nonblocking. */
static int console_read(uint8_t* data, int max_len) {
    if (!data || max_len <= 0) return -1;
    size_t available = input_size - input_offset;
    size_t count = available < (size_t)max_len ? available : (size_t)max_len;
    if (count) {
        memcpy(data, input + input_offset, count);
        input_offset += count;
    }
    return (int)count;
}

static int console_write(const uint8_t* data, int len) {
    if (!data || len <= 0) return -1;
    size_t written = fwrite(data, 1, (size_t)len, stdout);
    if (written != (size_t)len || fflush(stdout) != 0) {
        output_failed = true;
        return -1;
    }
    return len;
}

static const shell_backend_t console = {
    .read = console_read,
    .write = console_write,
};

static int command_result(int status) {
    last_command_status = status;
    ++completed_commands;
    return status;
}

static int cmd_led(int argc, char* argv[]) {
    if (argc != 2) {
        if (shell_puts("Usage: led <on|off|toggle|status>\r\n") <= 0)
            output_failed = true;
        return command_result(1);
    }
    nx_status_t action = NX_OK;
    if (strcmp(argv[1], "on") == 0) action = nx_device_gpio_write(led, 1);
    else if (strcmp(argv[1], "off") == 0) action = nx_device_gpio_write(led, 0);
    else if (strcmp(argv[1], "toggle") == 0) action = nx_device_gpio_toggle(led);
    else if (strcmp(argv[1], "status") != 0) {
        if (shell_puts("Invalid LED action\r\n") <= 0) output_failed = true;
        return command_result(1);
    }
    uint8_t state = 0;
    if (action != NX_OK || nx_device_gpio_read(led, &state) != NX_OK)
        return command_result(1);
    int written = shell_printf("[%s] P%c%u=%u\r\n", NX_BOARD_NAME,
                               NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN,
                               (unsigned int)state);
    return command_result(written > 0 ? 0 : 1);
}

static int cmd_delay(int argc, char* argv[]) {
    char* end = NULL;
    unsigned long delay = 0;
    if (argc == 2 && argv[1][0] >= '0' && argv[1][0] <= '9') {
        errno = 0;
        delay = strtoul(argv[1], &end, 10);
        if (errno == ERANGE || *end != '\0') delay = 0;
    }
    if (delay == 0 || delay > 1000) {
        if (shell_puts("Usage: delay <1..1000 ms>\r\n") <= 0)
            output_failed = true;
        return command_result(1);
    }
    if (osal_task_delay((uint32_t)delay) != OSAL_OK) return command_result(1);
    return command_result(shell_printf("Delayed %lu ms\r\n", delay) > 0 ? 0 : 1);
}

static int cmd_info(int argc, char* argv[]) {
    (void)argv;
    if (argc != 1) return command_result(1);
    return command_result(shell_printf("Board: %s; HAL: %s; OSAL: %s\r\n",
                                        NX_BOARD_NAME, nx_hal_get_version(),
                                        NX_CONFIG_OSAL_BACKEND_NAME) > 0 ? 0 : 1);
}

static int cmd_quit(int argc, char* argv[]) {
    (void)argv;
    if (argc != 1) return command_result(1);
    if (shell_puts("Closing shell\r\n") <= 0) return command_result(1);
    quit_requested = true;
    return command_result(0);
}

static const shell_command_t commands[] = {
    {.name = "led", .handler = cmd_led, .help = "Control the board LED",
     .usage = "led <on|off|toggle|status>", .completion = NULL},
    {.name = "delay", .handler = cmd_delay, .help = "Bounded OSAL delay",
     .usage = "delay <1..1000 ms>", .completion = NULL},
    {.name = "info", .handler = cmd_info, .help = "Show board and backend",
     .usage = "info", .completion = NULL},
    {.name = "quit", .handler = cmd_quit, .help = "Exit and release resources",
     .usage = "quit", .completion = NULL},
};

static bool shell_ok(shell_status_t status, const char* operation) {
    if (status == SHELL_OK) return true;
    (void)fprintf(stderr, "%s: %s\n", operation, shell_get_error_message(status));
    return false;
}

static bool process_line(const char* line) {
    char buffer[INPUT_CAPACITY];
    size_t length = strcspn(line, "\r\n");
    if (length > sizeof(buffer) - 2) return false;
    memcpy(buffer, line, length);
    buffer[length++] = '\r';
    input = buffer;
    input_size = length;
    input_offset = 0;
    bool passed = true;
    while (input_offset < input_size) {
        size_t previous = input_offset;
        if (!shell_ok(shell_process(), "shell_process") || output_failed ||
            input_offset == previous) {
            passed = false;
            break;
        }
    }
    input = NULL;
    input_size = input_offset = 0;
    return passed;
}

static bool smoke(void) {
    static const struct {
        const char* line;
        int status;
        unsigned int pin;
    } cases[] = {
        {"led on", 0, 1}, {"led toggle", 0, 0}, {"led status", 0, 0},
        {"led off", 0, 0}, {"delay 1", 0, 0}, {"delay -1", 1, 0},
        {"led invalid", 1, 0}, {"led on extra", 1, 0},
        {"info", 0, 0}, {"quit", 0, 0},
    };
    if (shell_register_command(&commands[0]) != SHELL_ERROR_ALREADY_EXISTS ||
        shell_register_command(NULL) != SHELL_ERROR_INVALID_PARAM) return false;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        unsigned int before = completed_commands;
        uint8_t state = 0;
        if (!process_line(cases[i].line) || completed_commands != before + 1 ||
            last_command_status != cases[i].status ||
            nx_device_gpio_read(led, &state) != NX_OK || state != cases[i].pin) return false;
    }
    if (!quit_requested) return false;
    return shell_puts("Shell smoke passed: 10 commands, LED readback and "
                      "expected invalid requests verified.\r\n") > 0;
}

static bool run_stdio(void) {
    char line[INPUT_CAPACITY];
    if (shell_puts("Native line console; use help, led, info, delay, quit.\r\n") <= 0)
        return false;
    shell_print_prompt();
    while (!quit_requested && !output_failed) {
        if (!fgets(line, sizeof(line), stdin)) return !ferror(stdin);
        size_t length = strlen(line);
        if (!strchr(line, '\n') && !feof(stdin)) {
            /* Reject an oversized line instead of executing a truncated command. */
            (void)fprintf(stderr, "Input line exceeds %u bytes\n",
                          (unsigned int)sizeof(line) - 2);
            return false;
        }
        if (length && !process_line(line)) return false;
    }
    return !output_failed;
}

int main(int argc, char** argv) {
    bool run_smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    if (argc != 1 && !run_smoke) return 2;
    if (nx_product_boot(NULL) != NX_OK) return 1;
    bool passed = false;
    bool initialized = false;
    char name[16];
    int written = snprintf(name, sizeof(name), "GPIO%c%u", NX_BOARD_LED_GPIO_PORT,
                           (unsigned)NX_BOARD_LED_GPIO_PIN);
    if (written < 0 || (size_t)written >= sizeof(name) ||
        nx_device_open(name, NX_DEVICE_CLASS_GPIO, (uintptr_t)&led, &led) != NX_OK)
        goto cleanup;
    initialized = true;
    if (nx_device_gpio_write(led, 0) != NX_OK) goto cleanup;
    const shell_config_t config = {
        .prompt = "nexus> ", .cmd_buffer_size = INPUT_CAPACITY,
        .history_depth = 8, .max_commands = 16,
    };
    if (!shell_ok(shell_init(&config), "shell_init")) goto cleanup;
    if (!shell_ok(shell_set_backend(&console), "shell_set_backend") ||
        !shell_ok(shell_register_builtin_commands(), "register builtins"))
        goto cleanup;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (!shell_ok(shell_register_command(&commands[i]), "register command"))
            goto cleanup;
    }
    passed = run_smoke ? smoke() : run_stdio();
cleanup:
    if (shell_is_initialized() && !shell_ok(shell_deinit(), "shell_deinit"))
        passed = false;
    if (!shell_ok(shell_set_backend(NULL), "detach console")) passed = false;
    if (initialized) {
        if (nx_device_gpio_write(led, 0) != NX_OK) passed = false;
        if (nx_device_close(led) != NX_OK) passed = false;
    }
    if (nx_product_shutdown(NULL) != NX_OK) passed = false;
    if (output_failed || fflush(stdout) != 0 || ferror(stdout)) passed = false;
    return passed ? 0 : 1;
}
