Log Framework
=============

Overview
--------

The Log Framework provides a unified logging interface for the Nexus embedded
platform. It supports multiple log levels, multiple output backends, module-level
filtering, and both synchronous and asynchronous modes.

Features
--------

- **Multiple Log Levels**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **Multiple Backends**: Console (stdout), UART, Memory (for testing)
- **Module Filtering**: Per-module log levels with wildcard support
- **Customizable Format**: Timestamp, level, module, file, line, function
- **Sync/Async Modes**: Non-blocking async mode for real-time systems
- **Thread-Safe**: Safe for multi-task environments
- **Compile-Time Optimization**: Remove low-level logs at compile time
- **Resource Configurable**: Static allocation support for constrained systems

Quick Start
-----------

**Basic usage:**

.. code-block:: c

    #define LOG_MODULE "app"
    #include "log/log.h"

    void app_init(void)
    {
        // Initialize with default config
        log_init(NULL);

        // Use convenience macros
        LOG_TRACE("Detailed trace info");
        LOG_DEBUG("Debug value: %d", 42);
        LOG_INFO("Application started");
        LOG_WARN("Resource usage at 80%%");
        LOG_ERROR("Failed to open file: %s", "config.txt");
        LOG_FATAL("Critical system failure");

        // Cleanup
        log_deinit();
    }

Log Levels
----------

Log levels are ordered from most verbose (TRACE) to least verbose (FATAL):

+------------------+-------+----------------------------------+
| Level            | Value | Description                      |
+==================+=======+==================================+
| LOG_LEVEL_TRACE  | 0     | Most detailed tracing info       |
+------------------+-------+----------------------------------+
| LOG_LEVEL_DEBUG  | 1     | Debug information                |
+------------------+-------+----------------------------------+
| LOG_LEVEL_INFO   | 2     | General information              |
+------------------+-------+----------------------------------+
| LOG_LEVEL_WARN   | 3     | Warning messages                 |
+------------------+-------+----------------------------------+
| LOG_LEVEL_ERROR  | 4     | Error messages                   |
+------------------+-------+----------------------------------+
| LOG_LEVEL_FATAL  | 5     | Fatal error messages             |
+------------------+-------+----------------------------------+
| LOG_LEVEL_NONE   | 6     | Disable all logging              |
+------------------+-------+----------------------------------+

Configuration
-------------

**Custom configuration:**

.. code-block:: c

    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,           // Filter out TRACE
        .format = "[%T] [%L] [%M] %m",      // Custom format
        .async_mode = false,                // Synchronous mode
        .buffer_size = 0,                   // Not used in sync mode
        .max_msg_len = 256,                 // Max message length
        .color_enabled = true               // Enable ANSI colors
    };

    log_init(&config);

Format Tokens
-------------

The format pattern supports the following tokens:

+--------+---------------------------+---------------+
| Token  | Description               | Example       |
+========+===========================+===============+
| ``%T`` | Timestamp (milliseconds)  | ``12345678``  |
+--------+---------------------------+---------------+
| ``%t`` | Time (HH:MM:SS)           | ``14:30:25``  |
+--------+---------------------------+---------------+
| ``%L`` | Level name (full)         | ``INFO``      |
+--------+---------------------------+---------------+
| ``%l`` | Level name (short)        | ``I``         |
+--------+---------------------------+---------------+
| ``%M`` | Module name               | ``app``       |
+--------+---------------------------+---------------+
| ``%F`` | File name                 | ``main.c``    |
+--------+---------------------------+---------------+
| ``%f`` | Function name             | ``app_init``  |
+--------+---------------------------+---------------+
| ``%n`` | Line number               | ``42``        |
+--------+---------------------------+---------------+
| ``%m`` | Message content           | ``Hello``     |
+--------+---------------------------+---------------+
| ``%c`` | ANSI color code           | -             |
+--------+---------------------------+---------------+
| ``%C`` | ANSI color reset          | -             |
+--------+---------------------------+---------------+
| ``%%`` | Literal percent sign      | ``%``         |
+--------+---------------------------+---------------+

**Default format:** ``[%T] [%L] [%M] %m``

Backends
--------

Console Backend
^^^^^^^^^^^^^^^

Outputs to stdout, suitable for Native platform debugging.

.. code-block:: c

    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    // When done
    log_backend_unregister("console");
    log_backend_console_destroy(console);

UART Backend
^^^^^^^^^^^^

Outputs to UART serial port, suitable for embedded targets.

.. code-block:: c

    // Initialize UART first
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };
    hal_uart_init(HAL_UART_0, &uart_cfg);

    // Create and register UART backend
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    log_backend_register(uart);

    // When done
    log_backend_unregister("uart");
    log_backend_uart_destroy(uart);
    hal_uart_deinit(HAL_UART_0);

Memory Backend
^^^^^^^^^^^^^^

Outputs to memory buffer, suitable for testing and debugging.

.. code-block:: c

    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);

    // Read buffer contents
    char buf[256];
    size_t len = log_backend_memory_read(memory, buf, sizeof(buf));

    // Clear buffer
    log_backend_memory_clear(memory);

    // When done
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);

Multiple Backends
^^^^^^^^^^^^^^^^^

Messages are delivered to all registered backends:

.. code-block:: c

    log_init(NULL);

    // Register multiple backends
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);

    // Message goes to both backends
    LOG_INFO("Message to console and memory");

Module Filtering
----------------

Set different log levels for different modules:

.. code-block:: c

    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO);  // Global level: INFO

    // Enable DEBUG for HAL modules (wildcard)
    log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

    // Only show WARN and above for network module
    log_module_set_level("network", LOG_LEVEL_WARN);

    // Get effective level for a module
    log_level_t level = log_module_get_level("hal.gpio");  // Returns DEBUG
    log_level_t level2 = log_module_get_level("app");      // Returns INFO (global)

    // Clear module-specific level
    log_module_clear_level("network");

    // Clear all module-specific levels
    log_module_clear_all();

Asynchronous Mode
-----------------

Async mode enables non-blocking log writes:

.. code-block:: c

    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .format = "[%T] [%L] %m",
        .async_mode = true,                          // Enable async
        .buffer_size = 4096,                         // Async buffer size
        .max_msg_len = 128,
        .async_queue_size = 32,                      // Queue depth
        .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST // Policy when full
    };

    log_init(&config);

    // Log writes return immediately
    for (int i = 0; i < 100; i++) {
        LOG_INFO("Async message %d", i);
    }

    // Wait for all messages to be processed
    log_async_flush();

    // Check pending message count
    size_t pending = log_async_pending();

**Buffer full policies:**

+-----------------------------------+----------------------------------+
| Policy                            | Description                      |
+===================================+==================================+
| ``LOG_ASYNC_POLICY_DROP_OLDEST``  | Drop oldest message when full    |
+-----------------------------------+----------------------------------+
| ``LOG_ASYNC_POLICY_DROP_NEWEST``  | Drop newest message when full    |
+-----------------------------------+----------------------------------+
| ``LOG_ASYNC_POLICY_BLOCK``        | Block until space available      |
+-----------------------------------+----------------------------------+

Backend-Level Filtering
-----------------------

Each backend can have its own minimum level:

.. code-block:: c

    log_init(NULL);
    log_set_level(LOG_LEVEL_TRACE);  // Global: allow all

    // Console shows all messages
    log_backend_t* console = log_backend_console_create();
    console->min_level = LOG_LEVEL_TRACE;
    log_backend_register(console);

    // UART only shows WARN and above
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    uart->min_level = LOG_LEVEL_WARN;
    log_backend_register(uart);

    LOG_DEBUG("Only to console");
    LOG_WARN("To console and UART");

Runtime Reconfiguration
-----------------------

Log settings can be changed at runtime:

.. code-block:: c

    // Change log level
    log_set_level(LOG_LEVEL_DEBUG);

    // Change format
    log_set_format("[%l] %m");

    // Change max message length
    log_set_max_msg_len(64);

    // Enable/disable backends
    log_backend_enable("uart", false);
    log_backend_enable("uart", true);

Compile-Time Configuration
--------------------------

**Compile-time level filtering:**

Remove low-level logs at compile time to reduce code size:

.. code-block:: cmake

    # CMakeLists.txt
    add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO)

This completely removes ``LOG_TRACE()`` and ``LOG_DEBUG()`` calls from the binary.

**Static allocation mode:**

Disable dynamic memory allocation:

.. code-block:: cmake

    add_definitions(-DLOG_USE_STATIC_ALLOC=1)

**Configuration macros:**

+-------------------------------+--------------------+---------------------------+
| Macro                         | Default            | Description               |
+===============================+====================+===========================+
| ``LOG_DEFAULT_LEVEL``         | ``LOG_LEVEL_INFO`` | Default log level         |
+-------------------------------+--------------------+---------------------------+
| ``LOG_MAX_MSG_LEN``           | ``128``            | Max message length        |
+-------------------------------+--------------------+---------------------------+
| ``LOG_MAX_BACKENDS``          | ``4``              | Max backend count         |
+-------------------------------+--------------------+---------------------------+
| ``LOG_MAX_MODULE_FILTERS``    | ``16``             | Max module filter count   |
+-------------------------------+--------------------+---------------------------+
| ``LOG_COMPILE_LEVEL``         | ``LOG_LEVEL_TRACE``| Compile-time level        |
+-------------------------------+--------------------+---------------------------+
| ``LOG_USE_STATIC_ALLOC``      | ``0``              | Static allocation mode    |
+-------------------------------+--------------------+---------------------------+

Custom Backend
--------------

Create a custom backend by implementing the backend interface:

.. code-block:: c

    static log_status_t my_backend_write(void* ctx, const char* msg, size_t len)
    {
        // Custom output logic (file, network, etc.)
        return LOG_OK;
    }

    static log_backend_t my_backend = {
        .name = "custom",
        .init = NULL,              // Optional
        .write = my_backend_write, // Required
        .flush = NULL,             // Optional
        .deinit = NULL,            // Optional
        .ctx = NULL,               // Custom context
        .min_level = LOG_LEVEL_INFO,
        .enabled = true
    };

    log_backend_register(&my_backend);

Thread Safety
-------------

The log framework is thread-safe in multi-task environments:

- Uses OSAL Mutex to protect shared state
- Minimizes lock hold time
- Async mode uses lock-free queue

Dependencies
------------

- **OSAL**: OS Abstraction Layer (Mutex, Queue, Task)
- **HAL**: Hardware Abstraction Layer (UART)

API Reference
-------------

See :doc:`../api/log` for complete API documentation.
