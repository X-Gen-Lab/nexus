Shell Framework
===============

Overview
--------

The Shell Framework provides an interactive command-line interface (CLI) for
the Nexus embedded platform. It supports command registration, line editing,
command history, auto-completion, and multiple I/O backends.

Features
--------

- **Command Registration**: Dynamic command registration with help and usage
- **Line Editing**: Cursor movement, insert, delete, Home/End keys
- **Command History**: Browse history with up/down arrows
- **Auto-Completion**: Tab completion for commands and arguments
- **Built-in Commands**: help, version, clear, history, echo
- **Multiple Backends**: UART, Console, and custom backends
- **Argument Parsing**: Supports quotes and escape characters
- **Thread-Safe**: Safe for multi-task environments
- **Resource Configurable**: Static allocation for constrained systems

Quick Start
-----------

**Basic usage:**

.. code-block:: c

    #include "shell/shell.h"

    // Custom command handler
    static int cmd_led(int argc, char* argv[])
    {
        if (argc < 2) {
            shell_printf("Usage: led <on|off>\r\n");
            return -1;
        }

        if (strcmp(argv[1], "on") == 0) {
            hal_gpio_write(LED_PIN, 1);
            shell_printf("LED turned on\r\n");
        } else if (strcmp(argv[1], "off") == 0) {
            hal_gpio_write(LED_PIN, 0);
            shell_printf("LED turned off\r\n");
        }
        return 0;
    }

    // Command definition
    static const shell_command_t led_cmd = {
        .name = "led",
        .handler = cmd_led,
        .help = "Control LED state",
        .usage = "led <on|off>"
    };

    void app_init(void)
    {
        // Initialize Shell
        shell_config_t config = SHELL_CONFIG_DEFAULT;
        shell_init(&config);

        // Set backend (UART)
        shell_set_backend(&shell_uart_backend);

        // Register built-in commands
        shell_register_builtin_commands();

        // Register custom command
        shell_register_command(&led_cmd);

        // Print prompt
        shell_print_prompt();
    }

    void app_loop(void)
    {
        // Process Shell input (non-blocking)
        shell_process();
    }

Configuration
-------------

**Custom configuration:**

.. code-block:: c

    shell_config_t config = {
        .prompt = "nexus> ",        // Custom prompt
        .cmd_buffer_size = 128,     // Command buffer size
        .history_depth = 16,        // History depth
        .max_commands = 64          // Max command count
    };

    shell_init(&config);

**Default configuration:**

+----------------------+----------+---------------------------+
| Parameter            | Default  | Description               |
+======================+==========+===========================+
| ``prompt``           | ``"$ "`` | Shell prompt string       |
+----------------------+----------+---------------------------+
| ``cmd_buffer_size``  | ``128``  | Command buffer size       |
+----------------------+----------+---------------------------+
| ``history_depth``    | ``8``    | History entry count       |
+----------------------+----------+---------------------------+
| ``max_commands``     | ``32``   | Max registered commands   |
+----------------------+----------+---------------------------+

Command Definition
------------------

**Command structure:**

.. code-block:: c

    typedef struct {
        const char* name;               // Command name
        shell_cmd_handler_t handler;    // Handler function
        const char* help;               // Help text
        const char* usage;              // Usage string
        shell_completion_t completion;  // Auto-completion (optional)
    } shell_command_t;

**Handler function signature:**

.. code-block:: c

    typedef int (*shell_cmd_handler_t)(int argc, char* argv[]);

- ``argc``: Argument count (including command name)
- ``argv``: Argument array
- Return: 0 for success, non-zero for error

**Example command with auto-completion:**

.. code-block:: c

    // Auto-completion function
    static int led_completion(const char* partial, char* completions[], int max)
    {
        const char* options[] = {"on", "off", "toggle", "status"};
        int count = 0;

        for (int i = 0; i < 4 && count < max; i++) {
            if (strncmp(options[i], partial, strlen(partial)) == 0) {
                completions[count++] = (char*)options[i];
            }
        }
        return count;
    }

    static const shell_command_t led_cmd = {
        .name = "led",
        .handler = cmd_led,
        .help = "Control LED state",
        .usage = "led <on|off|toggle|status>",
        .completion = led_completion
    };

Built-in Commands
-----------------

+-------------+---------------------------+----------------------+
| Command     | Description               | Usage                |
+=============+===========================+======================+
| ``help``    | Show available commands   | ``help [command]``   |
+-------------+---------------------------+----------------------+
| ``version`` | Show Shell version        | ``version``          |
+-------------+---------------------------+----------------------+
| ``clear``   | Clear terminal screen     | ``clear``            |
+-------------+---------------------------+----------------------+
| ``history`` | Show command history      | ``history``          |
+-------------+---------------------------+----------------------+
| ``echo``    | Print arguments           | ``echo [text...]``   |
+-------------+---------------------------+----------------------+

Line Editing Keys
-----------------

+------------------------+---------------------------+
| Key                    | Function                  |
+========================+===========================+
| ``←`` / ``Ctrl+B``     | Move cursor left          |
+------------------------+---------------------------+
| ``→`` / ``Ctrl+F``     | Move cursor right         |
+------------------------+---------------------------+
| ``Home`` / ``Ctrl+A``  | Move to line start        |
+------------------------+---------------------------+
| ``End`` / ``Ctrl+E``   | Move to line end          |
+------------------------+---------------------------+
| ``Backspace``          | Delete previous character |
+------------------------+---------------------------+
| ``Delete`` / ``Ctrl+D``| Delete current character  |
+------------------------+---------------------------+
| ``Ctrl+K``             | Delete to end of line     |
+------------------------+---------------------------+
| ``Ctrl+U``             | Delete to start of line   |
+------------------------+---------------------------+
| ``↑`` / ``Ctrl+P``     | Previous history          |
+------------------------+---------------------------+
| ``↓`` / ``Ctrl+N``     | Next history              |
+------------------------+---------------------------+
| ``Tab``                | Auto-complete             |
+------------------------+---------------------------+
| ``Ctrl+C``             | Cancel current input      |
+------------------------+---------------------------+
| ``Ctrl+L``             | Clear screen              |
+------------------------+---------------------------+

Backends
--------

UART Backend
^^^^^^^^^^^^

.. code-block:: c

    #include "shell/shell_backend.h"

    // Initialize UART
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE
    };
    hal_uart_init(HAL_UART_0, &uart_cfg);

    // Use UART backend
    shell_set_backend(&shell_uart_backend);

Custom Backend
^^^^^^^^^^^^^^

.. code-block:: c

    static int my_read(char* buf, size_t len)
    {
        // Implement read logic
        return bytes_read;
    }

    static int my_write(const char* buf, size_t len)
    {
        // Implement write logic
        return bytes_written;
    }

    static const shell_backend_t my_backend = {
        .read = my_read,
        .write = my_write
    };

    shell_set_backend(&my_backend);

Output Functions
----------------

.. code-block:: c

    // Formatted output
    shell_printf("Value: %d\r\n", 42);

    // String output
    shell_puts("Hello World\r\n");

    // Character output
    shell_putc('A');

    // Print prompt
    shell_print_prompt();

    // Clear screen
    shell_clear_screen();

Error Handling
--------------

.. code-block:: c

    shell_status_t status = shell_register_command(&cmd);
    if (status != SHELL_OK) {
        const char* msg = shell_get_error_message(status);
        printf("Error: %s\n", msg);
    }

**Status codes:**

+--------------------------------+---------------------------+
| Status                         | Description               |
+================================+===========================+
| ``SHELL_OK``                   | Success                   |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_INVALID_PARAM``  | Invalid parameter         |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_NOT_INIT``       | Not initialized           |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_ALREADY_INIT``   | Already initialized       |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_NO_BACKEND``     | No backend configured     |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_CMD_NOT_FOUND``  | Command not found         |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_CMD_EXISTS``     | Command already exists    |
+--------------------------------+---------------------------+
| ``SHELL_ERROR_BUFFER_FULL``    | Buffer full               |
+--------------------------------+---------------------------+

Compile-Time Configuration
--------------------------

+------------------------------------+----------+---------------------------+
| Macro                              | Default  | Description               |
+====================================+==========+===========================+
| ``SHELL_MAX_COMMANDS``             | ``32``   | Max command count         |
+------------------------------------+----------+---------------------------+
| ``SHELL_DEFAULT_CMD_BUFFER_SIZE``  | ``128``  | Default command buffer    |
+------------------------------------+----------+---------------------------+
| ``SHELL_DEFAULT_HISTORY_DEPTH``    | ``8``    | Default history depth     |
+------------------------------------+----------+---------------------------+
| ``SHELL_MAX_ARGS``                 | ``16``   | Max argument count        |
+------------------------------------+----------+---------------------------+
| ``SHELL_DEFAULT_PROMPT``           | ``"$ "`` | Default prompt            |
+------------------------------------+----------+---------------------------+

Dependencies
------------

- **OSAL**: OS Abstraction Layer (optional, for thread safety)
- **HAL**: Hardware Abstraction Layer (UART backend)

API Reference
-------------

See :doc:`../api/shell` for complete API documentation.
