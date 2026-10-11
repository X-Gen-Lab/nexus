Shell Framework API Reference
=============================

This section documents the Shell Framework API.

Overview
--------

The Nexus Shell Framework provides an interactive command-line interface with
features like command registration, autocomplete, history, and line editing.
It's designed for embedded systems with minimal memory footprint.

Usage Examples
--------------

Basic Shell Setup
~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "shell/shell.h"

    /* Initialize shell with UART backend */
    shell_config_t config = {
        .prompt = "nexus> ",
        .history_size = 10,
        .max_line_length = 128
    };

    shell_init(&config);

    /* Start shell processing loop */
    while (1) {
        shell_process();
    }

Command Registration
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "shell/shell.h"
    #include "shell/shell_command.h"

    /* Define command handler */
    static int cmd_hello(int argc, char** argv) {
        shell_printf("Hello, World!\n");
        return 0;
    }

    /* Register command */
    SHELL_CMD_REGISTER(hello, cmd_hello, "Print hello message");

    /* Command with arguments */
    static int cmd_echo(int argc, char** argv) {
        for (int i = 1; i < argc; i++) {
            shell_printf("%s ", argv[i]);
        }
        shell_printf("\n");
        return 0;
    }

    SHELL_CMD_REGISTER(echo, cmd_echo, "Echo arguments");

Command with Subcommands
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "shell/shell.h"
    #include "shell/shell_command.h"

    static int cmd_gpio_set(int argc, char** argv) {
        /* Set GPIO pin */
        return 0;
    }

    static int cmd_gpio_get(int argc, char** argv) {
        /* Get GPIO pin state */
        return 0;
    }

    /* Register subcommands */
    SHELL_CMD_REGISTER(gpio_set, cmd_gpio_set, "Set GPIO pin");
    SHELL_CMD_REGISTER(gpio_get, cmd_gpio_get, "Get GPIO pin state");

Custom Backend
~~~~~~~~~~~~~~

.. code-block:: c

    #include "shell/shell.h"
    #include "shell/shell_backend.h"

    /* Implement custom backend */
    static int my_backend_read(char* buf, size_t len) {
        /* Read from custom input */
        return bytes_read;
    }

    static int my_backend_write(const char* buf, size_t len) {
        /* Write to custom output */
        return bytes_written;
    }

    static shell_backend_t custom_backend = {
        .read = my_backend_read,
        .write = my_backend_write
    };

    /* Set custom backend */
    shell_set_backend(&custom_backend);

Thread Safety
-------------

The Shell Framework is **not thread-safe**. Shell commands should only be
processed from a single thread. If multiple threads need to interact with
the shell, external synchronization is required.

Shell Core
----------

.. doxygengroup:: SHELL
   :project: nexus
   :content-only:

Definitions & Constants
-----------------------

.. doxygengroup:: SHELL_STATUS
   :project: nexus
   :content-only:

.. doxygengroup:: SHELL_CONSTANTS
   :project: nexus
   :content-only:

Command Management
------------------

.. doxygengroup:: SHELL_COMMAND
   :project: nexus
   :content-only:

Line Editor
-----------

.. doxygengroup:: SHELL_LINE_EDITOR
   :project: nexus
   :content-only:

History
-------

.. doxygengroup:: SHELL_HISTORY
   :project: nexus
   :content-only:

Autocomplete
------------

.. doxygengroup:: SHELL_AUTOCOMPLETE
   :project: nexus
   :content-only:

Parser
------

.. doxygengroup:: SHELL_PARSER
   :project: nexus
   :content-only:

Backend Interface
-----------------

.. doxygengroup:: SHELL_BACKEND
   :project: nexus
   :content-only:


Related APIs
------------

- :doc:`osal` - OS abstraction (used for task management)
- :doc:`init` - Automatic initialization system
- :doc:`log` - Logging framework
- :doc:`hal` - HAL (for UART backend)

See Also
--------

- :doc:`../user_guide/shell` - Shell Framework User Guide
- :doc:`../reference/error_codes` - Error Code Reference
