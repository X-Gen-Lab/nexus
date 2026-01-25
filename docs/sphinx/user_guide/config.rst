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

32-bit Signed Integer (int32)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    // Store 32-bit signed integer
    config_set_i32("app.timeout", 5000);
    config_set_i32("sensor.offset", -100);

    // Read with default value
    int32_t timeout;
    config_get_i32("app.timeout", &timeout, 1000);  // Default: 1000

    // Check if exists before reading
    bool exists;
    config_exists("app.timeout", &exists);
    if (exists) {
        config_get_i32("app.timeout", &timeout, 0);
    }

32-bit Unsigned Integer (uint32)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    // Store 32-bit unsigned integer
    config_set_u32("app.count", 100);
    config_set_u32("network.retry_count", 5);

    // Read with default value
    uint32_t count;
    config_get_u32("app.count", &count, 0);  // Default: 0

    // Use for counters and IDs
    uint32_t device_id;
    config_get_u32("device.id", &device_id, 0xFFFFFFFF);

64-bit Signed Integer (int64)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    // Store 64-bit signed integer (timestamps, large values)
    config_set_i64("app.timestamp", 1234567890123LL);
    config_set_i64("system.boot_time", -1000000LL);

    // Read with default value
    int64_t timestamp;
    config_get_i64("app.timestamp", &timestamp, 0);

    // Use for timestamps
    int64_t last_sync;
    config_get_i64("network.last_sync", &last_sync, 0);

Float (Single-Precision)
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

    // Store floating-point values
    config_set_float("sensor.threshold", 3.14f);
    config_set_float("calibration.offset", -0.5f);
    config_set_float("pid.kp", 1.2f);

    // Read with default value
    float threshold;
    config_get_float("sensor.threshold", &threshold, 0.0f);

    // Use for sensor calibration
    float temp_offset;
    config_get_float("sensor.temp_offset", &temp_offset, 0.0f);

Boolean
^^^^^^^

.. code-block:: c

    // Store boolean values
    config_set_bool("app.debug", true);
    config_set_bool("network.dhcp_enabled", false);
    config_set_bool("sensor.auto_calibrate", true);

    // Read with default value
    bool debug;
    config_get_bool("app.debug", &debug, false);

    // Use for feature flags
    bool use_ssl;
    config_get_bool("network.use_ssl", &use_ssl, true);

String
^^^^^^

.. code-block:: c

    // Store string values
    config_set_str("app.name", "MyApp");
    config_set_str("network.hostname", "device-001");
    config_set_str("wifi.ssid", "MyNetwork");

    // Read string
    char name[32];
    config_get_str("app.name", name, sizeof(name));

    // Get string length first
    size_t len;
    config_get_str_len("app.name", &len);
    char* dynamic_buf = malloc(len + 1);
    config_get_str("app.name", dynamic_buf, len + 1);

    // Use for configuration strings
    char server_url[128];
    config_get_str("network.server_url", server_url, sizeof(server_url));

Binary Blob
^^^^^^^^^^^

.. code-block:: c

    // Store binary data (certificates, keys, raw data)
    uint8_t cert_data[] = {0x30, 0x82, 0x01, 0x0A, ...};
    config_set_blob("security.certificate", cert_data, sizeof(cert_data));

    // Store encryption key
    uint8_t aes_key[16] = {0x00, 0x01, 0x02, ...};
    config_set_blob("security.aes_key", aes_key, sizeof(aes_key));

    // Read binary data
    uint8_t buffer[256];
    size_t actual_size;
    config_get_blob("security.certificate", buffer, sizeof(buffer), &actual_size);

    // Get blob size first
    size_t blob_size;
    config_get_blob_len("security.certificate", &blob_size);
    uint8_t* dynamic_buf = malloc(blob_size);
    config_get_blob("security.certificate", dynamic_buf, blob_size, &actual_size);

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

The Config Manager supports multiple storage backends for different use cases.
Each backend implements the same interface, allowing easy switching between
storage types.

RAM Backend (Volatile)
^^^^^^^^^^^^^^^^^^^^^^

The RAM backend stores configuration in memory. Data is lost on power cycle.
Ideal for testing, temporary storage, and development.

.. code-block:: c

    #include "config/config_backend.h"

    // Create RAM backend with 4KB buffer
    config_backend_t* ram = config_backend_ram_create(4096);
    if (ram == NULL) {
        printf("Failed to create RAM backend\n");
        return -1;
    }

    // Set as active backend
    config_set_backend(ram);

    // Use configuration normally
    config_set_i32("app.timeout", 5000);
    config_set_str("app.name", "TestApp");

    // Data is stored in RAM
    config_commit();  // No-op for RAM backend

    // When done, destroy backend
    config_backend_ram_destroy(ram);

**Use cases:**
- Unit testing
- Temporary configuration
- Development and debugging
- Systems without persistent storage

Flash Backend (Persistent)
^^^^^^^^^^^^^^^^^^^^^^^^^^

The Flash backend stores configuration in non-volatile Flash memory.
Data persists across power cycles. Requires HAL Flash driver.

.. code-block:: c

    #include "config/config_backend.h"
    #include "hal/hal_flash.h"

    // Define Flash partition for configuration
    #define CONFIG_FLASH_ADDR   0x08080000  // Flash address
    #define CONFIG_FLASH_SIZE   0x00010000  // 64KB partition

    // Initialize Flash HAL
    hal_flash_init();

    // Create Flash backend
    config_backend_t* flash = config_backend_flash_create(
        CONFIG_FLASH_ADDR,
        CONFIG_FLASH_SIZE
    );
    if (flash == NULL) {
        printf("Failed to create Flash backend\n");
        return -1;
    }

    // Set as active backend
    config_set_backend(flash);

    // Load existing configuration from Flash
    config_status_t status = config_load();
    if (status != CONFIG_OK) {
        printf("No existing config, using defaults\n");
        // Set default values
        config_set_i32("app.timeout", 5000);
        config_set_str("app.name", "MyApp");
    }

    // Modify configuration
    config_set_bool("app.debug", true);

    // Save to Flash
    config_commit();

    // When done, destroy backend
    config_backend_flash_destroy(flash);
    hal_flash_deinit();

**Use cases:**
- Production systems
- Persistent user settings
- Device configuration
- Calibration data

**Flash considerations:**
- Wear leveling: Flash has limited write cycles
- Erase granularity: Flash erases in sectors/pages
- Write time: Flash writes are slower than RAM
- Power loss: Use atomic writes or checksums

Custom Backend
^^^^^^^^^^^^^^

Implement a custom backend for specialized storage needs (EEPROM, SD card,
network storage, etc.).

.. code-block:: c

    #include "config/config_backend.h"

    // Custom context (your storage state)
    typedef struct {
        void* storage_handle;
        uint32_t base_address;
    } my_backend_ctx_t;

    // Read function
    static config_status_t my_backend_read(void* ctx, const char* key,
                                           void* value, size_t* size)
    {
        my_backend_ctx_t* backend = (my_backend_ctx_t*)ctx;

        // Implement read logic
        // 1. Find key in storage
        // 2. Read value
        // 3. Copy to value buffer
        // 4. Set size

        return CONFIG_OK;
    }

    // Write function
    static config_status_t my_backend_write(void* ctx, const char* key,
                                            const void* value, size_t size)
    {
        my_backend_ctx_t* backend = (my_backend_ctx_t*)ctx;

        // Implement write logic
        // 1. Find or allocate space for key
        // 2. Write value
        // 3. Update metadata

        return CONFIG_OK;
    }

    // Erase function
    static config_status_t my_backend_erase(void* ctx, const char* key)
    {
        my_backend_ctx_t* backend = (my_backend_ctx_t*)ctx;

        // Implement erase logic
        // 1. Find key
        // 2. Mark as deleted or reclaim space

        return CONFIG_OK;
    }

    // Commit function (flush to storage)
    static config_status_t my_backend_commit(void* ctx)
    {
        my_backend_ctx_t* backend = (my_backend_ctx_t*)ctx;

        // Implement commit logic
        // 1. Flush any cached writes
        // 2. Update checksums
        // 3. Ensure data integrity

        return CONFIG_OK;
    }

    // Create custom backend
    config_backend_t* my_backend_create(void)
    {
        // Allocate context
        my_backend_ctx_t* ctx = malloc(sizeof(my_backend_ctx_t));
        if (ctx == NULL) {
            return NULL;
        }

        // Initialize context
        ctx->storage_handle = open_storage();
        ctx->base_address = 0x1000;

        // Allocate backend structure
        config_backend_t* backend = malloc(sizeof(config_backend_t));
        if (backend == NULL) {
            free(ctx);
            return NULL;
        }

        // Set function pointers
        backend->read = my_backend_read;
        backend->write = my_backend_write;
        backend->erase = my_backend_erase;
        backend->commit = my_backend_commit;
        backend->ctx = ctx;

        return backend;
    }

    // Destroy custom backend
    void my_backend_destroy(config_backend_t* backend)
    {
        if (backend != NULL) {
            my_backend_ctx_t* ctx = (my_backend_ctx_t*)backend->ctx;
            if (ctx != NULL) {
                close_storage(ctx->storage_handle);
                free(ctx);
            }
            free(backend);
        }
    }

    // Usage
    config_backend_t* backend = my_backend_create();
    config_set_backend(backend);

    // Use configuration
    config_set_i32("app.value", 42);
    config_commit();

    // Cleanup
    my_backend_destroy(backend);

**Custom backend use cases:**
- EEPROM storage
- SD card configuration files
- Network-based configuration (cloud sync)
- Database storage
- Encrypted storage with custom crypto

Backend Comparison
^^^^^^^^^^^^^^^^^^

+----------------+------------+------------+-------------+---------------+
| Feature        | RAM        | Flash      | Custom      | Notes         |
+================+============+============+=============+===============+
| Persistent     | No         | Yes        | Depends     | Across reboot |
+----------------+------------+------------+-------------+---------------+
| Speed          | Fast       | Medium     | Varies      | Read/write    |
+----------------+------------+------------+-------------+---------------+
| Wear leveling  | N/A        | Required   | Depends     | Write cycles  |
+----------------+------------+------------+-------------+---------------+
| Size limit     | RAM size   | Flash size | Varies      | Storage cap   |
+----------------+------------+------------+-------------+---------------+
| Power loss     | Data lost  | Safe       | Depends     | Data integrity|
+----------------+------------+------------+-------------+---------------+
| Complexity     | Simple     | Medium     | High        | Implementation|
+----------------+------------+------------+-------------+---------------+

Backend Selection Guide
^^^^^^^^^^^^^^^^^^^^^^^

**Choose RAM backend when:**
- Testing and development
- Temporary configuration
- No persistent storage needed
- Maximum performance required

**Choose Flash backend when:**
- Production deployment
- Configuration must persist
- Limited RAM available
- Standard embedded system

**Choose Custom backend when:**
- Special storage requirements
- External storage (SD, EEPROM)
- Network/cloud synchronization
- Custom encryption or compression

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
