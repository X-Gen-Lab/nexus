Config Manager API Reference
============================

This section documents the Configuration Manager API.

Overview
--------

The Nexus Config Manager provides a flexible configuration storage system with
support for multiple data types and storage backends (RAM, Flash). It's designed
for embedded systems requiring persistent configuration management.

Usage Examples
--------------

Basic Configuration
~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "config/config.h"

    /* Initialize config manager */
    config_init();

    /* Set configuration values */
    int32_t value = 42;
    config_set_int32("system.timeout", value);

    config_set_string("system.name", "MyDevice");

    bool enabled = true;
    config_set_bool("feature.enabled", enabled);

    /* Get configuration values */
    int32_t timeout;
    if (config_get_int32("system.timeout", &timeout) == CONFIG_OK) {
        /* Use timeout value */
    }

Namespaces
~~~~~~~~~~

.. code-block:: c

    #include "config/config.h"

    /* Create namespace */
    config_namespace_create("network");

    /* Set values in namespace */
    config_set_string("network.ip", "192.168.1.100");
    config_set_int32("network.port", 8080);

    /* Iterate namespace */
    config_namespace_iterate("network", my_callback, user_data);

Flash Backend
~~~~~~~~~~~~~

.. code-block:: c

    #include "config/config.h"
    #include "config/config_backend.h"

    /* Initialize with Flash backend */
    config_backend* flash_backend = config_flash_backend_create();
    config_set_backend(flash_backend);

    /* Configuration is now persisted to Flash */
    config_set_int32("system.boot_count", boot_count);

    /* Commit changes to Flash */
    config_commit();

Import/Export
~~~~~~~~~~~~~

.. code-block:: c

    #include "config/config.h"

    /* Export configuration to JSON */
    char buffer[1024];
    size_t size = sizeof(buffer);
    config_export_json(buffer, &size);

    /* Import configuration from JSON */
    config_import_json(json_string, strlen(json_string));

Thread Safety
-------------

The Config Manager is **thread-safe** when using the Flash backend with proper
locking. The RAM backend requires external synchronization for multi-threaded access.

- **Flash backend**: Thread-safe with internal locking
- **RAM backend**: Not thread-safe, requires external mutex

Config Core
-----------

.. doxygengroup:: CONFIG
   :project: nexus
   :content-only:

Config Definitions
------------------

.. doxygengroup:: CONFIG_DEF
   :project: nexus
   :content-only:

Backend Interface
-----------------

.. doxygengroup:: CONFIG_BACKEND
   :project: nexus
   :content-only:

RAM Backend
~~~~~~~~~~~

.. doxygengroup:: CONFIG_BACKEND_RAM
   :project: nexus
   :content-only:

Flash Backend
~~~~~~~~~~~~~

.. doxygengroup:: CONFIG_BACKEND_FLASH
   :project: nexus
   :content-only:


Related APIs
------------

- :doc:`hal` - HAL (for Flash backend)
- :doc:`init` - Automatic initialization system
- :doc:`log` - Logging framework
- :doc:`osal` - OS abstraction (for thread safety)

See Also
--------

- :doc:`../user_guide/config` - Config Manager User Guide
- :doc:`../reference/error_codes` - Error Code Reference
