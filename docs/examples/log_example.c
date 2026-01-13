/**
 * \file            log_example.c
 * \brief           Log Framework Usage Examples
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file demonstrates various usage patterns for the
 *                  Nexus Log Framework. It covers basic logging, configuration,
 *                  multiple backends, module filtering, and async mode.
 *
 * \note            This is example code for documentation purposes.
 *                  It may not compile standalone without the full Nexus SDK.
 */

/* Define module name before including log.h */
#define LOG_MODULE "example"

#include "hal/hal_uart.h"
#include "log/log.h"


/*---------------------------------------------------------------------------*/
/* Example 1: Basic Logging                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Basic logging example
 * \details         Shows how to initialize the log system and use log macros.
 *
 * \code{.c}
 * void basic_logging_example(void) {
 *     // Initialize with default configuration
 *     log_init(NULL);
 *
 *     // Use convenience macros for different log levels
 *     LOG_TRACE("Detailed trace information");
 *     LOG_DEBUG("Debug value: %d", 42);
 *     LOG_INFO("Application started successfully");
 *     LOG_WARN("Resource usage at 80%%");
 *     LOG_ERROR("Failed to open file: %s", "config.txt");
 *     LOG_FATAL("Critical system failure");
 *
 *     // Clean up
 *     log_deinit();
 * }
 * \endcode
 */
void basic_logging_example(void) {
    /* Initialize with default configuration */
    log_init(NULL);

    /* Use convenience macros for different log levels */
    LOG_TRACE("Detailed trace information");
    LOG_DEBUG("Debug value: %d", 42);
    LOG_INFO("Application started successfully");
    LOG_WARN("Resource usage at 80%%");
    LOG_ERROR("Failed to open file: %s", "config.txt");
    LOG_FATAL("Critical system failure");

    /* Clean up */
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 2: Custom Configuration                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Custom configuration example
 * \details         Shows how to configure the log system with custom settings.
 *
 * \code{.c}
 * void custom_config_example(void) {
 *     log_config_t config = {
 *         .level = LOG_LEVEL_DEBUG,           // Filter out TRACE messages
 *         .format = "[%T] [%L] [%M] %m",      // Custom format pattern
 *         .async_mode = false,                // Synchronous mode
 *         .buffer_size = 0,                   // Not used in sync mode
 *         .max_msg_len = 256,                 // Max message length
 *         .color_enabled = true               // Enable ANSI colors
 *     };
 *
 *     log_init(&config);
 *
 *     LOG_DEBUG("This will be logged");
 *     LOG_TRACE("This will be filtered out");
 *
 *     log_deinit();
 * }
 * \endcode
 */
void custom_config_example(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,      /* Filter out TRACE messages */
        .format = "[%T] [%L] [%M] %m", /* Custom format pattern */
        .async_mode = false,           /* Synchronous mode */
        .buffer_size = 0,              /* Not used in sync mode */
        .max_msg_len = 256,            /* Max message length */
        .color_enabled = true          /* Enable ANSI colors */
    };

    log_init(&config);

    LOG_DEBUG("This will be logged");
    LOG_TRACE("This will be filtered out");

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 3: Multiple Backends                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Multiple backends example
 * \details         Shows how to register multiple output backends.
 *
 * \code{.c}
 * void multiple_backends_example(void) {
 *     log_init(NULL);
 *
 *     // Create and register console backend (stdout)
 *     log_backend_t* console = log_backend_console_create();
 *     log_backend_register(console);
 *
 *     // Create and register memory backend for testing
 *     log_backend_t* memory = log_backend_memory_create(4096);
 *     log_backend_register(memory);
 *
 *     // Create and register UART backend
 *     hal_uart_config_t uart_cfg = {
 *         .baudrate = 115200,
 *         .wordlen = HAL_UART_WORDLEN_8,
 *         .stopbits = HAL_UART_STOPBITS_1,
 *         .parity = HAL_UART_PARITY_NONE,
 *         .flowctrl = HAL_UART_FLOWCTRL_NONE
 *     };
 *     hal_uart_init(HAL_UART_0, &uart_cfg);
 *     log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
 *     log_backend_register(uart);
 *
 *     // Log message goes to all backends
 *     LOG_INFO("Message sent to console, memory, and UART");
 *
 *     // Read from memory backend
 *     char buf[256];
 *     size_t len = log_backend_memory_read(memory, buf, sizeof(buf));
 *
 *     // Clean up
 *     log_backend_unregister("console");
 *     log_backend_unregister("memory");
 *     log_backend_unregister("uart");
 *     log_backend_console_destroy(console);
 *     log_backend_memory_destroy(memory);
 *     log_backend_uart_destroy(uart);
 *     hal_uart_deinit(HAL_UART_0);
 *     log_deinit();
 * }
 * \endcode
 */
void multiple_backends_example(void) {
    log_init(NULL);

    /* Create and register console backend (stdout) */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    /* Create and register memory backend for testing */
    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);

    /* Create and register UART backend */
    hal_uart_config_t uart_cfg = {.baudrate = 115200,
                                  .wordlen = HAL_UART_WORDLEN_8,
                                  .stopbits = HAL_UART_STOPBITS_1,
                                  .parity = HAL_UART_PARITY_NONE,
                                  .flowctrl = HAL_UART_FLOWCTRL_NONE};
    hal_uart_init(HAL_UART_0, &uart_cfg);
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    log_backend_register(uart);

    /* Log message goes to all backends */
    LOG_INFO("Message sent to console, memory, and UART");

    /* Read from memory backend */
    char buf[256];
    size_t len = log_backend_memory_read(memory, buf, sizeof(buf));
    (void)len; /* Suppress unused warning */

    /* Clean up */
    log_backend_unregister("console");
    log_backend_unregister("memory");
    log_backend_unregister("uart");
    log_backend_console_destroy(console);
    log_backend_memory_destroy(memory);
    log_backend_uart_destroy(uart);
    hal_uart_deinit(HAL_UART_0);
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 4: Module-Level Filtering                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Module-level filtering example
 * \details         Shows how to set different log levels for different modules.
 *
 * \code{.c}
 * void module_filtering_example(void) {
 *     log_init(NULL);
 *     log_set_level(LOG_LEVEL_INFO);  // Global level: INFO
 *
 *     // Set specific level for HAL modules
 *     log_module_set_level("hal.*", LOG_LEVEL_DEBUG);
 *
 *     // Set specific level for a single module
 *     log_module_set_level("network", LOG_LEVEL_WARN);
 *
 *     // Messages from "hal.gpio" will show DEBUG and above
 *     // Messages from "network" will show WARN and above
 *     // Messages from other modules will show INFO and above
 *
 *     // Get effective level for a module
 *     log_level_t hal_level = log_module_get_level("hal.gpio");
 *     log_level_t net_level = log_module_get_level("network");
 *     log_level_t app_level = log_module_get_level("app");  // Returns global
 *
 *     // Clear module-specific level
 *     log_module_clear_level("network");
 *
 *     // Clear all module-specific levels
 *     log_module_clear_all();
 *
 *     log_deinit();
 * }
 * \endcode
 */
void module_filtering_example(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO); /* Global level: INFO */

    /* Set specific level for HAL modules */
    log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

    /* Set specific level for a single module */
    log_module_set_level("network", LOG_LEVEL_WARN);

    /* Get effective level for a module */
    log_level_t hal_level = log_module_get_level("hal.gpio");
    log_level_t net_level = log_module_get_level("network");
    log_level_t app_level = log_module_get_level("app"); /* Returns global */

    (void)hal_level;
    (void)net_level;
    (void)app_level;

    /* Clear module-specific level */
    log_module_clear_level("network");

    /* Clear all module-specific levels */
    log_module_clear_all();

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 5: Asynchronous Logging                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Asynchronous logging example
 * \details         Shows how to use async mode for non-blocking logging.
 *
 * \code{.c}
 * void async_logging_example(void) {
 *     log_config_t config = {
 *         .level = LOG_LEVEL_DEBUG,
 *         .format = "[%T] [%L] %m",
 *         .async_mode = true,                          // Enable async mode
 *         .buffer_size = 4096,                         // Async buffer size
 *         .max_msg_len = 128,
 *         .color_enabled = false,
 *         .async_queue_size = 32,                      // Queue depth
 *         .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST // Policy when full
 *     };
 *
 *     log_init(&config);
 *
 *     // Register a backend
 *     log_backend_t* console = log_backend_console_create();
 *     log_backend_register(console);
 *
 *     // Log messages (non-blocking, queued for background processing)
 *     for (int i = 0; i < 100; i++) {
 *         LOG_INFO("Async message %d", i);
 *     }
 *
 *     // Check pending messages
 *     size_t pending = log_async_pending();
 *
 *     // Flush all pending messages (blocking)
 *     log_async_flush();
 *
 *     // Change policy at runtime
 *     log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);
 *
 *     // Clean up (automatically flushes pending messages)
 *     log_backend_unregister("console");
 *     log_backend_console_destroy(console);
 *     log_deinit();
 * }
 * \endcode
 */
void async_logging_example(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .format = "[%T] [%L] %m",
        .async_mode = true,  /* Enable async mode */
        .buffer_size = 4096, /* Async buffer size */
        .max_msg_len = 128,
        .color_enabled = false,
        .async_queue_size = 32,                      /* Queue depth */
        .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST /* Policy when full */
    };

    log_init(&config);

    /* Register a backend */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    /* Log messages (non-blocking, queued for background processing) */
    for (int i = 0; i < 100; i++) {
        LOG_INFO("Async message %d", i);
    }

    /* Check pending messages */
    size_t pending = log_async_pending();
    (void)pending;

    /* Flush all pending messages (blocking) */
    log_async_flush();

    /* Change policy at runtime */
    log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);

    /* Clean up (automatically flushes pending messages) */
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 6: Runtime Reconfiguration                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Runtime reconfiguration example
 * \details         Shows how to change log settings at runtime.
 *
 * \code{.c}
 * void runtime_reconfig_example(void) {
 *     log_init(NULL);
 *
 *     // Change log level at runtime
 *     log_set_level(LOG_LEVEL_DEBUG);
 *     LOG_DEBUG("Debug messages now visible");
 *
 *     log_set_level(LOG_LEVEL_ERROR);
 *     LOG_DEBUG("This debug message is filtered");
 *     LOG_ERROR("Only errors and above are visible");
 *
 *     // Change format pattern at runtime
 *     log_set_format("[%l] %m");  // Short level format
 *     LOG_INFO("Using short format");
 *
 *     log_set_format("[%T] [%L] [%M] [%F:%n] %m");  // Full format
 *     LOG_INFO("Using full format");
 *
 *     // Change max message length at runtime
 *     log_set_max_msg_len(64);
 *     LOG_INFO("This very long message will be truncated if it exceeds 64
 * characters...");
 *
 *     // Reset to default max length
 *     log_set_max_msg_len(0);  // 0 means use default
 *
 *     log_deinit();
 * }
 * \endcode
 */
void runtime_reconfig_example(void) {
    log_init(NULL);

    /* Change log level at runtime */
    log_set_level(LOG_LEVEL_DEBUG);
    LOG_DEBUG("Debug messages now visible");

    log_set_level(LOG_LEVEL_ERROR);
    LOG_DEBUG("This debug message is filtered");
    LOG_ERROR("Only errors and above are visible");

    /* Change format pattern at runtime */
    log_set_format("[%l] %m"); /* Short level format */
    LOG_INFO("Using short format");

    log_set_format("[%T] [%L] [%M] [%F:%n] %m"); /* Full format */
    LOG_INFO("Using full format");

    /* Change max message length at runtime */
    log_set_max_msg_len(64);
    LOG_INFO("This very long message will be truncated if it exceeds 64 "
             "characters...");

    /* Reset to default max length */
    log_set_max_msg_len(0); /* 0 means use default */

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 7: Backend-Level Filtering                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Backend-level filtering example
 * \details         Shows how to set different log levels for different
 * backends.
 *
 * \code{.c}
 * void backend_filtering_example(void) {
 *     log_init(NULL);
 *     log_set_level(LOG_LEVEL_TRACE);  // Global: allow all
 *
 *     // Console backend: show all messages
 *     log_backend_t* console = log_backend_console_create();
 *     console->min_level = LOG_LEVEL_TRACE;
 *     log_backend_register(console);
 *
 *     // UART backend: only show warnings and above
 *     hal_uart_config_t uart_cfg = { .baudrate = 115200 };
 *     hal_uart_init(HAL_UART_0, &uart_cfg);
 *     log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
 *     uart->min_level = LOG_LEVEL_WARN;
 *     log_backend_register(uart);
 *
 *     // This goes to console only
 *     LOG_DEBUG("Debug message");
 *
 *     // This goes to both console and UART
 *     LOG_WARN("Warning message");
 *
 *     // Enable/disable backends at runtime
 *     log_backend_enable("uart", false);  // Disable UART
 *     LOG_ERROR("Error only to console");
 *
 *     log_backend_enable("uart", true);   // Re-enable UART
 *     LOG_ERROR("Error to both");
 *
 *     // Clean up
 *     log_backend_unregister("console");
 *     log_backend_unregister("uart");
 *     log_backend_console_destroy(console);
 *     log_backend_uart_destroy(uart);
 *     hal_uart_deinit(HAL_UART_0);
 *     log_deinit();
 * }
 * \endcode
 */
void backend_filtering_example(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_TRACE); /* Global: allow all */

    /* Console backend: show all messages */
    log_backend_t* console = log_backend_console_create();
    console->min_level = LOG_LEVEL_TRACE;
    log_backend_register(console);

    /* UART backend: only show warnings and above */
    hal_uart_config_t uart_cfg = {.baudrate = 115200,
                                  .wordlen = HAL_UART_WORDLEN_8,
                                  .stopbits = HAL_UART_STOPBITS_1,
                                  .parity = HAL_UART_PARITY_NONE,
                                  .flowctrl = HAL_UART_FLOWCTRL_NONE};
    hal_uart_init(HAL_UART_0, &uart_cfg);
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    uart->min_level = LOG_LEVEL_WARN;
    log_backend_register(uart);

    /* This goes to console only */
    LOG_DEBUG("Debug message");

    /* This goes to both console and UART */
    LOG_WARN("Warning message");

    /* Enable/disable backends at runtime */
    log_backend_enable("uart", false); /* Disable UART */
    LOG_ERROR("Error only to console");

    log_backend_enable("uart", true); /* Re-enable UART */
    LOG_ERROR("Error to both");

    /* Clean up */
    log_backend_unregister("console");
    log_backend_unregister("uart");
    log_backend_console_destroy(console);
    log_backend_uart_destroy(uart);
    hal_uart_deinit(HAL_UART_0);
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 8: Compile-Time Configuration                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Compile-time configuration example
 * \details         Shows how to use compile-time options to reduce code size.
 *
 * To disable TRACE and DEBUG at compile time, add to your build:
 * \code
 * -DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO
 * \endcode
 *
 * This completely removes LOG_TRACE and LOG_DEBUG calls from the binary.
 *
 * To use static allocation (no malloc/free):
 * \code
 * -DLOG_USE_STATIC_ALLOC=1
 * \endcode
 *
 * To customize buffer sizes:
 * \code
 * -DLOG_MAX_MSG_LEN=64
 * -DLOG_MAX_BACKENDS=2
 * -DLOG_MAX_MODULE_FILTERS=8
 * \endcode
 */
void compile_time_config_example(void) {
    /* When LOG_COMPILE_LEVEL=LOG_LEVEL_INFO:
     * - LOG_TRACE() becomes ((void)0) - no code generated
     * - LOG_DEBUG() becomes ((void)0) - no code generated
     * - LOG_INFO() and above work normally
     */

    log_init(NULL);

    /* These may be compiled out depending on LOG_COMPILE_LEVEL */
    LOG_TRACE("May be compiled out");
    LOG_DEBUG("May be compiled out");

    /* These are always compiled in (unless LOG_COMPILE_LEVEL > their level) */
    LOG_INFO("Always compiled in");
    LOG_WARN("Always compiled in");
    LOG_ERROR("Always compiled in");
    LOG_FATAL("Always compiled in");

    log_deinit();
}
