Config Manager
==============

Overview
--------

The Config Manager provides a flexible, secure, and persistent key-value
configuration storage for the Nexus embedded platform. It supports multiple
data types, namespaces, change notifications, and optional encryption.

Features
--------

- **Multiple Types**: int32/64, uint32, float, bool, string, blob
- **Namespace Isolation**: Separate configurations by module
- **Default Values**: Register defaults with one-click reset
- **Change Notifications**: Callbacks on configuration changes
- **Persistent Storage**: RAM, Flash, and custom backends
- **Import/Export**: Binary and JSON format support
- **Optional Encryption**: AES-128/256 for sensitive data
- **Thread-Safe**: Safe for multi-task environments
- **Resource Configurable**: Static allocation for constrained systems

Quick Start
-----------

**Basic usage:**

.. code-block:: c

    #include "config/config.h"

    void app_init(void)
    {
        // Initialize with default config
        config_init(NULL);

        // Store configuration values
        config_set_i32("app.timeout", 5000);
        config_set_str("app.name", "MyApp");
        config_set_bool("app.debug", true);
        config_set_float("sensor.threshold", 3.14f);

        // Read configuration values
        int32_t timeout = 0;
        config_get_i32("app.timeout", &timeout, 1000);  // Default: 1000

        char name[32];
        config_get_str("app.name", name, sizeof(name));

        bool debug = false;
        config_get_bool("app.debug", &debug, false);

        // Commit to storage
        config_commit();

        // Cleanup
        config_deinit();
    }

Configuration
-------------

**Custom configuration:**

.. code-block:: c

    config_manager_config_t config = {
        .max_keys = 128,           // Max key count
        .max_key_len = 32,         // Max key length
        .max_value_size = 256,     // Max value size
        .max_namespaces = 8,       // Max namespace count
        .max_callbacks = 16,       // Max callback count
        .auto_commit = true        // Auto-commit mode
    };

    config_init(&config);

**Default configuration:**

+---------------------+----------+---------------------------+
| Parameter           | Default  | Description               |
+=====================+==========+===========================+
| ``max_keys``        | ``64``   | Maximum key count         |
+---------------------+----------+---------------------------+
| ``max_key_len``     | ``32``   | Maximum key length        |
+---------------------+----------+---------------------------+
| ``max_value_size``  | ``256``  | Maximum value size        |
+---------------------+----------+---------------------------+
| ``max_namespaces``  | ``4``    | Maximum namespace count   |
+---------------------+----------+---------------------------+
| ``max_callbacks``   | ``8``    | Maximum callback count    |
+---------------------+----------+---------------------------+
| ``auto_commit``     | ``false``| Auto-commit mode          |
+---------------------+----------+---------------------------+

Data Types
----------

+-----------------------+-------+---------------------------+
| Type                  | Value | Description               |
+=======================+=======+===========================+
| ``CONFIG_TYPE_I32``   | 0     | 32-bit signed integer     |
+-----------------------+-------+---------------------------+
| ``CONFIG_TYPE_U32``   | 1     | 32-bit unsigned integer   |
+-----------------------+-------+---------------------------+
| ``CONFIG_TYPE_I64``   | 2     | 64-bit signed integer     |
+-----------------------+-------+---------------------------+
| ``CONFIG_TYPE_FLOAT`` | 3     | Single-precision float    |
+-----------------------+-------+---------------------------+
| ``CONFIG_TYPE_BOOL``  | 4     | Boolean value             |
+-----------------------+-------+---------------------------+
| ``CONFIG_TYPE_STR``   | 5     | Null-terminated string    |
+-----------------------+-------+---------------------------+
| ``CONFIG_TYPE_BLOB``  | 6     | Binary data               |
+-----------------------+-------+---------------------------+

Type Operations
---------------

**Integer operations:**

.. code-block:: c

    // 32-bit signed
    config_set_i32("app.timeout", 5000);
    int32_t timeout;
    config_get_i32("app.timeout", &timeout, 1000);

    // 32-bit unsigned
    config_set_u32("app.count", 100);
    uint32_t count;
    config_get_u32("app.count", &count, 0);

    // 64-bit signed
    config_set_i64("app.timestamp", 1234567890123LL);
    int64_t timestamp;
    config_get_i64("app.timestamp", &timestamp, 0);

**Float and boolean:**

.. code-block:: c

    // Float
    config_set_float("sensor.threshold", 3.14f);
    float threshold;
    config_get_float("sensor.threshold", &threshold, 0.0f);

    // Boolean
    config_set_bool("app.debug", true);
    bool debug;
    config_get_bool("app.debug", &debug, false);

**String and blob:**

.. code-block:: c

    // String
    config_set_str("app.name", "MyApp");
    char name[32];
    config_get_str("app.name", name, sizeof(name));

    // Get string length
    size_t len;
    config_get_str_len("app.name", &len);

    // Binary blob
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    config_set_blob("app.data", data, sizeof(data));

    uint8_t buffer[64];
    size_t actual_size;
    config_get_blob("app.data", buffer, sizeof(buffer), &actual_size);

Namespaces
----------

Namespaces isolate configurations for different modules:

.. code-block:: c

    // Open namespace
    config_ns_handle_t ns;
    config_open_namespace("network", &ns);

    // Operations within namespace
    config_ns_set_i32(ns, "port", 8080);
    config_ns_set_str(ns, "host", "192.168.1.1");
    config_ns_set_bool(ns, "ssl", true);

    // Read from namespace
    int32_t port = 0;
    config_ns_get_i32(ns, "port", &port, 80);

    // Check if key exists
    bool exists;
    config_ns_exists(ns, "port", &exists);

    // Delete key
    config_ns_delete(ns, "port");

    // Close namespace
    config_close_namespace(ns);

    // Erase entire namespace
    config_erase_namespace("network");

Default Values
--------------

**Single default values:**

.. code-block:: c

    // Register defaults
    config_set_default_i32("app.timeout", 5000);
    config_set_default_str("app.name", "DefaultApp");
    config_set_default_bool("app.debug", false);

    // Reset to default
    config_reset_to_default("app.timeout");

    // Reset all to defaults
    config_reset_all_to_defaults();

**Batch registration:**

.. code-block:: c

    static const config_default_t app_defaults[] = {
        {"app.timeout", CONFIG_TYPE_I32, {.i32_val = 5000}},
        {"app.name", CONFIG_TYPE_STR, {.str_val = "MyApp"}},
        {"app.debug", CONFIG_TYPE_BOOL, {.bool_val = false}},
        {"app.rate", CONFIG_TYPE_FLOAT, {.float_val = 1.5f}},
    };

    config_register_defaults(app_defaults,
                             sizeof(app_defaults) / sizeof(app_defaults[0]));

Change Notifications
--------------------

**Watch specific key:**

.. code-block:: c

    void on_timeout_change(const char* key, config_type_t type,
                           const void* old_val, const void* new_val,
                           void* user_data)
    {
        int32_t old_timeout = old_val ? *(int32_t*)old_val : 0;
        int32_t new_timeout = *(int32_t*)new_val;
        printf("Timeout changed: %d -> %d\n", old_timeout, new_timeout);
    }

    config_cb_handle_t handle;
    config_register_callback("app.timeout", on_timeout_change, NULL, &handle);

    // Triggers callback
    config_set_i32("app.timeout", 10000);

    // Unregister
    config_unregister_callback(handle);

**Watch all changes:**

.. code-block:: c

    void on_any_change(const char* key, config_type_t type,
                       const void* old_val, const void* new_val,
                       void* user_data)
    {
        printf("Config changed: %s\n", key);
    }

    config_cb_handle_t handle;
    config_register_wildcard_callback(on_any_change, NULL, &handle);

Storage Backends
----------------

RAM Backend
^^^^^^^^^^^

For testing and volatile storage:

.. code-block:: c

    #include "config/config_backend.h"

    // Create RAM backend
    config_backend_t* ram = config_backend_ram_create(4096);
    config_set_backend(ram);

    // When done
    config_backend_ram_destroy(ram);

Flash Backend
^^^^^^^^^^^^^

For persistent storage:

.. code-block:: c

    // Create Flash backend
    config_backend_t* flash = config_backend_flash_create(
        FLASH_CONFIG_ADDR,    // Flash start address
        FLASH_CONFIG_SIZE     // Partition size
    );
    config_set_backend(flash);

    // Load from Flash
    config_load();

    // Save to Flash
    config_commit();

Custom Backend
^^^^^^^^^^^^^^

.. code-block:: c

    static config_status_t my_read(void* ctx, const char* key,
                                   void* value, size_t* size)
    {
        // Implement read logic
        return CONFIG_OK;
    }

    static config_status_t my_write(void* ctx, const char* key,
                                    const void* value, size_t size)
    {
        // Implement write logic
        return CONFIG_OK;
    }

    static const config_backend_t my_backend = {
        .read = my_read,
        .write = my_write,
        .erase = my_erase,
        .commit = my_commit,
        .ctx = &my_context
    };

    config_set_backend(&my_backend);

Import/Export
-------------

**Binary format:**

.. code-block:: c

    // Export
    size_t export_size;
    config_get_export_size(CONFIG_FORMAT_BINARY, 0, &export_size);

    uint8_t* buffer = malloc(export_size);
    size_t actual_size;
    config_export(CONFIG_FORMAT_BINARY, 0, buffer, export_size, &actual_size);

    // Import
    config_import(CONFIG_FORMAT_BINARY, 0, buffer, actual_size);

**JSON format:**

.. code-block:: c

    // Export as JSON
    char json_buffer[1024];
    size_t json_size;
    config_export(CONFIG_FORMAT_JSON, 0, json_buffer,
                  sizeof(json_buffer), &json_size);

    // Import JSON
    const char* json = "{\"app.timeout\": 5000, \"app.name\": \"Test\"}";
    config_import(CONFIG_FORMAT_JSON, 0, json, strlen(json));

**Namespace export:**

.. code-block:: c

    // Export specific namespace only
    config_export_namespace("network", CONFIG_FORMAT_JSON, 0,
                            buffer, buf_size, &actual_size);

Encryption
----------

**Set encryption key:**

.. code-block:: c

    uint8_t key[16] = {0x00, 0x01, 0x02, ...};  // AES-128 key
    config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);

**Store encrypted data:**

.. code-block:: c

    // Encrypt and store string
    config_set_str_encrypted("auth.password", "secret123");

    // Encrypt and store binary
    uint8_t token[32] = {...};
    config_set_blob_encrypted("auth.token", token, sizeof(token));

    // Reading auto-decrypts
    char password[64];
    config_get_str("auth.password", password, sizeof(password));

    // Check if key is encrypted
    bool encrypted;
    config_is_encrypted("auth.password", &encrypted);

**Key rotation:**

.. code-block:: c

    // Re-encrypt all encrypted data with new key
    uint8_t new_key[16] = {...};
    config_rotate_encryption_key(new_key, sizeof(new_key), CONFIG_CRYPTO_AES128);

Query Operations
----------------

.. code-block:: c

    // Check if key exists
    bool exists;
    config_exists("app.timeout", &exists);

    // Get value type
    config_type_t type;
    config_get_type("app.timeout", &type);

    // Delete key
    config_delete("app.timeout");

    // Get key count
    size_t count;
    config_get_count(&count);

    // Iterate all entries
    bool print_entry(const config_entry_info_t* info, void* user_data)
    {
        printf("Key: %s, Type: %d, Size: %d\n",
               info->key, info->type, info->value_size);
        return true;  // Continue iteration
    }

    config_iterate(print_entry, NULL);

Error Handling
--------------

.. code-block:: c

    config_status_t status = config_set_i32("key", 100);
    if (status != CONFIG_OK) {
        const char* msg = config_error_to_str(status);
        printf("Error: %s\n", msg);
    }

**Status codes:**

+----------------------------------+---------------------------+
| Status                           | Description               |
+==================================+===========================+
| ``CONFIG_OK``                    | Success                   |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_INVALID_PARAM``   | Invalid parameter         |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_NOT_INIT``        | Not initialized           |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_ALREADY_INIT``    | Already initialized       |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_NOT_FOUND``       | Key not found             |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_TYPE_MISMATCH``   | Type mismatch             |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_BUFFER_TOO_SMALL``| Buffer too small          |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_STORAGE_FULL``    | Storage full              |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_BACKEND``         | Backend error             |
+----------------------------------+---------------------------+
| ``CONFIG_ERROR_CRYPTO``          | Encryption error          |
+----------------------------------+---------------------------+

Compile-Time Configuration
--------------------------

+----------------------------------+----------+---------------------------+
| Macro                            | Default  | Description               |
+==================================+==========+===========================+
| ``CONFIG_DEFAULT_MAX_KEYS``      | ``64``   | Default max key count     |
+----------------------------------+----------+---------------------------+
| ``CONFIG_DEFAULT_MAX_KEY_LEN``   | ``32``   | Default max key length    |
+----------------------------------+----------+---------------------------+
| ``CONFIG_DEFAULT_MAX_VALUE_SIZE``| ``256``  | Default max value size    |
+----------------------------------+----------+---------------------------+
| ``CONFIG_DEFAULT_MAX_NAMESPACES``| ``4``    | Default max namespaces    |
+----------------------------------+----------+---------------------------+
| ``CONFIG_DEFAULT_MAX_CALLBACKS`` | ``8``    | Default max callbacks     |
+----------------------------------+----------+---------------------------+
| ``CONFIG_ENABLE_ENCRYPTION``     | ``1``    | Enable encryption         |
+----------------------------------+----------+---------------------------+
| ``CONFIG_ENABLE_JSON``           | ``1``    | Enable JSON support       |
+----------------------------------+----------+---------------------------+

Dependencies
------------

- **OSAL**: OS Abstraction Layer (Mutex for thread safety)
- **HAL**: Hardware Abstraction Layer (Flash backend)

API Reference
-------------

See :doc:`../api/config` for complete API documentation.
