Log Framework API Reference
===========================

This section documents the Log Framework API.

Overview
--------

The Nexus Log Framework provides a flexible logging system with multiple backends,
async mode support, and module-level filtering. It's designed for embedded systems
with minimal overhead and configurable output destinations.

Usage Examples
--------------

Basic Logging
~~~~~~~~~~~~~

.. code-block:: c

    #include "log/log.h"

    /* Initialize log framework */
    log_init();

    /* Log messages at different levels */
    LOG_ERROR("System", "Critical error occurred: %d", error_code);
    LOG_WARN("System", "Warning: low memory");
    LOG_INFO("System", "System initialized successfully");
    LOG_DEBUG("System", "Debug info: value=%d", value);

Module-Specific Logging
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "log/log.h"

    /* Define module name */
    #define MODULE_NAME "MyModule"

    void my_function(void) {
        LOG_INFO(MODULE_NAME, "Function started");

        /* Your code here */

        LOG_DEBUG(MODULE_NAME, "Processing data: count=%d", count);
    }

Async Logging
~~~~~~~~~~~~~

.. code-block:: c

    #include "log/log.h"

    /* Configure async mode */
    log_config_t config = {
        .async_mode = true,
        .async_buffer_size = 1024,
        .backend = LOG_BACKEND_UART
    };

    log_init_with_config(&config);

    /* Log messages - they're buffered and output asynchronously */
    LOG_INFO("App", "Async logging enabled");

Custom Backend
~~~~~~~~~~~~~~

.. code-block:: c

    #include "log/log.h"
    #include "log/log_backend.h"

    /* Implement custom backend */
    static void my_backend_write(const char* msg, size_t len) {
        /* Write to custom output */
    }

    static log_backend custom_backend = {
        .write = my_backend_write,
        .flush = NULL  /* Optional */
    };

    /* Register custom backend */
    log_set_backend(&custom_backend);

Thread Safety
-------------

The Log Framework is **thread-safe** when async mode is enabled. In synchronous mode,
logging from multiple threads requires external synchronization.

- **Async mode**: Thread-safe, uses internal buffering and locking
- **Sync mode**: Not thread-safe, requires external mutex

Log Definitions
---------------

.. doxygengroup:: LOG_DEF
   :project: nexus
   :content-only:

Log Macros
----------

.. doxygengroup:: LOG_MACROS
   :project: nexus
   :content-only:

Log Backend
-----------

.. doxygengroup:: LOG_BACKEND
   :project: nexus
   :content-only:

Console Backend
~~~~~~~~~~~~~~~

.. doxygengroup:: LOG_BACKEND_CONSOLE
   :project: nexus
   :content-only:

UART Backend
~~~~~~~~~~~~

.. doxygengroup:: LOG_BACKEND_UART
   :project: nexus
   :content-only:

Memory Backend
~~~~~~~~~~~~~~

.. doxygengroup:: LOG_BACKEND_MEMORY
   :project: nexus
   :content-only:


Related APIs
------------

- :doc:`osal` - OS abstraction (used for thread safety in async mode)
- :doc:`init` - Automatic initialization system
- :doc:`config` - Configuration management
- :doc:`shell` - Shell framework (can use log for output)

See Also
--------

- :doc:`../user_guide/log` - Log Framework User Guide
- :doc:`../reference/error_codes` - Error Code Reference
