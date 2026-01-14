/**
 * \file            config_example.c
 * \brief           Config Manager Usage Examples
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file demonstrates various usage patterns for the
 *                  Nexus Config Manager. It covers basic configuration,
 *                  namespaces, callbacks, persistence, and import/export.
 *
 * \note            This is example code for documentation purposes.
 *                  It may not compile standalone without the full Nexus SDK.
 */

#include "config/config.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Example 1: Basic Configuration                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Basic configuration example
 * \details         Shows how to initialize Config Manager and store/retrieve
 * values.
 *
 * \code{.c}
 * void basic_config_example(void) {
 *     // Initialize with default configuration
 *     config_init(NULL);
 *
 *     // Store different data types
 *     config_set_i32("app.timeout", 5000);
 *     config_set_u32("app.retry_count", 3);
 *     config_set_float("sensor.threshold", 25.5f);
 *     config_set_bool("feature.enabled", true);
 *     config_set_str("device.name", "Nexus-001");
 *
 *     // Read values back
 *     int32_t timeout = 0;
 *     uint32_t retry = 0;
 *     float threshold = 0.0f;
 *     bool enabled = false;
 *     char name[32];
 *
 *     config_get_i32("app.timeout", &timeout, 1000);      // default: 1000
 *     config_get_u32("app.retry_count", &retry, 1);       // default: 1
 *     config_get_float("sensor.threshold", &threshold, 20.0f);
 *     config_get_bool("feature.enabled", &enabled, false);
 *     config_get_str("device.name", name, sizeof(name));
 *
 *     // Clean up
 *     config_deinit();
 * }
 * \endcode
 */
void basic_config_example(void) {
    /* Initialize with default configuration */
    config_init(NULL);

    /* Store different data types */
    config_set_i32("app.timeout", 5000);
    config_set_u32("app.retry_count", 3);
    config_set_float("sensor.threshold", 25.5f);
    config_set_bool("feature.enabled", true);
    config_set_str("device.name", "Nexus-001");

    /* Read values back */
    int32_t timeout = 0;
    uint32_t retry = 0;
    float threshold = 0.0f;
    bool enabled = false;
    char name[32];

    config_get_i32("app.timeout", &timeout, 1000);
    config_get_u32("app.retry_count", &retry, 1);
    config_get_float("sensor.threshold", &threshold, 20.0f);
    config_get_bool("feature.enabled", &enabled, false);
    config_get_str("device.name", name, sizeof(name));

    (void)timeout;
    (void)retry;
    (void)threshold;
    (void)enabled;

    /* Clean up */
    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 2: Custom Configuration                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Custom configuration example
 * \details         Shows how to configure Config Manager with custom settings.
 *
 * \code{.c}
 * void custom_config_example(void) {
 *     config_manager_config_t config = {
 *         .max_keys = 128,          // Maximum 128 keys
 *         .max_key_len = 32,        // Max key length 32 chars
 *         .max_value_size = 256,    // Max value size 256 bytes
 *         .max_namespaces = 8,      // Maximum 8 namespaces
 *         .max_callbacks = 16,      // Maximum 16 callbacks
 *         .auto_commit = true       // Auto-commit changes
 *     };
 *
 *     config_init(&config);
 *
 *     // Use Config Manager...
 *
 *     config_deinit();
 * }
 * \endcode
 */
void custom_config_example(void) {
    config_manager_config_t config = {.max_keys = 128,
                                      .max_key_len = 32,
                                      .max_value_size = 256,
                                      .max_namespaces = 8,
                                      .max_callbacks = 16,
                                      .auto_commit = true};

    config_init(&config);

    /* Use Config Manager... */
    config_set_str("app.version", "1.0.0");

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 3: Namespace Isolation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Namespace isolation example
 * \details         Shows how to use namespaces to organize configurations.
 *
 * \code{.c}
 * void namespace_example(void) {
 *     config_init(NULL);
 *
 *     // Open namespaces for different modules
 *     config_ns_handle_t wifi_ns, ble_ns;
 *     config_open_namespace("wifi", &wifi_ns);
 *     config_open_namespace("ble", &ble_ns);
 *
 *     // Store WiFi settings
 *     config_ns_set_str(wifi_ns, "ssid", "MyNetwork");
 *     config_ns_set_str(wifi_ns, "password", "secret123");
 *     config_ns_set_bool(wifi_ns, "auto_connect", true);
 *
 *     // Store BLE settings
 *     config_ns_set_str(ble_ns, "device_name", "Nexus-BLE");
 *     config_ns_set_u32(ble_ns, "adv_interval", 100);
 *
 *     // Read from namespaces
 *     char ssid[32];
 *     bool auto_conn = false;
 *     config_ns_get_str(wifi_ns, "ssid", ssid, sizeof(ssid));
 *     config_ns_get_bool(wifi_ns, "auto_connect", &auto_conn, false);
 *
 *     // Close namespaces
 *     config_close_namespace(wifi_ns);
 *     config_close_namespace(ble_ns);
 *
 *     // Erase all keys in a namespace
 *     config_erase_namespace("wifi");
 *
 *     config_deinit();
 * }
 * \endcode
 */
void namespace_example(void) {
    config_init(NULL);

    /* Open namespaces for different modules */
    config_ns_handle_t wifi_ns, ble_ns;
    config_open_namespace("wifi", &wifi_ns);
    config_open_namespace("ble", &ble_ns);

    /* Store WiFi settings */
    config_ns_set_str(wifi_ns, "ssid", "MyNetwork");
    config_ns_set_str(wifi_ns, "password", "secret123");
    config_ns_set_bool(wifi_ns, "auto_connect", true);

    /* Store BLE settings */
    config_ns_set_str(ble_ns, "device_name", "Nexus-BLE");
    config_ns_set_u32(ble_ns, "adv_interval", 100);

    /* Read from namespaces */
    char ssid[32];
    bool auto_conn = false;
    config_ns_get_str(wifi_ns, "ssid", ssid, sizeof(ssid));
    config_ns_get_bool(wifi_ns, "auto_connect", &auto_conn, false);

    (void)auto_conn;

    /* Close namespaces */
    config_close_namespace(wifi_ns);
    config_close_namespace(ble_ns);

    /* Erase all keys in a namespace */
    config_erase_namespace("wifi");

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 4: Default Values                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Default values example
 * \details         Shows how to register and use default values.
 *
 * \code{.c}
 * void default_values_example(void) {
 *     config_init(NULL);
 *
 *     // Register individual defaults
 *     config_set_default_i32("app.timeout", 5000);
 *     config_set_default_str("app.name", "DefaultApp");
 *     config_set_default_bool("debug.enabled", false);
 *
 *     // Or register multiple defaults at once
 *     static const config_default_t defaults[] = {
 *         { "net.port", CONFIG_TYPE_U32, { .u32_val = 8080 } },
 *         { "net.timeout", CONFIG_TYPE_I32, { .i32_val = 30000 } },
 *         { "net.retry", CONFIG_TYPE_U32, { .u32_val = 3 } }
 *     };
 *     config_register_defaults(defaults, 3);
 *
 *     // Set a value
 *     config_set_i32("app.timeout", 10000);
 *
 *     // Reset to default
 *     config_reset_to_default("app.timeout");
 *
 *     // Reset all to defaults
 *     config_reset_all_to_defaults();
 *
 *     config_deinit();
 * }
 * \endcode
 */
void default_values_example(void) {
    config_init(NULL);

    /* Register individual defaults */
    config_set_default_i32("app.timeout", 5000);
    config_set_default_str("app.name", "DefaultApp");
    config_set_default_bool("debug.enabled", false);

    /* Or register multiple defaults at once */
    static const config_default_t defaults[] = {
        {"net.port", CONFIG_TYPE_U32, {.u32_val = 8080}},
        {"net.timeout", CONFIG_TYPE_I32, {.i32_val = 30000}},
        {"net.retry", CONFIG_TYPE_U32, {.u32_val = 3}}};
    config_register_defaults(defaults, 3);

    /* Set a value */
    config_set_i32("app.timeout", 10000);

    /* Reset to default */
    config_reset_to_default("app.timeout");

    /* Reset all to defaults */
    config_reset_all_to_defaults();

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 5: Change Callbacks                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Change callback handler
 */
static void on_config_change(const char* key, config_type_t type,
                             const void* old_value, const void* new_value,
                             void* user_data) {
    (void)old_value;
    (void)user_data;

    printf("Config changed: %s (type=%d)\n", key, type);
    if (type == CONFIG_TYPE_I32 && new_value) {
        printf("  New value: %d\n", *(const int32_t*)new_value);
    }
}

/**
 * \brief           Change callbacks example
 * \details         Shows how to register callbacks for configuration changes.
 *
 * \code{.c}
 * void callback_example(void) {
 *     config_init(NULL);
 *
 *     // Register callback for specific key
 *     config_cb_handle_t cb_handle;
 *     config_register_callback("app.timeout", on_config_change, NULL,
 * &cb_handle);
 *
 *     // Register wildcard callback for all changes
 *     config_cb_handle_t wildcard_handle;
 *     config_register_wildcard_callback(on_config_change, NULL,
 * &wildcard_handle);
 *
 *     // This triggers the callbacks
 *     config_set_i32("app.timeout", 5000);
 *
 *     // Unregister callbacks
 *     config_unregister_callback(cb_handle);
 *     config_unregister_callback(wildcard_handle);
 *
 *     config_deinit();
 * }
 * \endcode
 */
void callback_example(void) {
    config_init(NULL);

    /* Register callback for specific key */
    config_cb_handle_t cb_handle;
    config_register_callback("app.timeout", on_config_change, NULL, &cb_handle);

    /* Register wildcard callback for all changes */
    config_cb_handle_t wildcard_handle;
    config_register_wildcard_callback(on_config_change, NULL, &wildcard_handle);

    /* This triggers the callbacks */
    config_set_i32("app.timeout", 5000);

    /* Unregister callbacks */
    config_unregister_callback(cb_handle);
    config_unregister_callback(wildcard_handle);

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 6: Persistence with Backend                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Persistence example
 * \details         Shows how to use storage backends for persistent
 * configuration.
 *
 * \code{.c}
 * void persistence_example(void) {
 *     config_init(NULL);
 *
 *     // Set RAM backend (volatile storage)
 *     config_set_backend(&config_ram_backend);
 *
 *     // Or set Flash backend (persistent storage)
 *     // config_set_backend(&config_flash_backend);
 *
 *     // Load existing configurations from storage
 *     config_load();
 *
 *     // Make changes
 *     config_set_str("device.serial", "SN12345678");
 *     config_set_u32("boot.count", 42);
 *
 *     // Commit changes to storage
 *     config_commit();
 *
 *     config_deinit();
 * }
 * \endcode
 */
void persistence_example(void) {
    config_init(NULL);

    /* Set RAM backend (volatile storage) */
    config_set_backend(&config_ram_backend);

    /* Load existing configurations from storage */
    config_load();

    /* Make changes */
    config_set_str("device.serial", "SN12345678");
    config_set_u32("boot.count", 42);

    /* Commit changes to storage */
    config_commit();

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 7: Query and Enumeration                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Iteration callback for listing configs
 */
static bool list_config_cb(const config_entry_info_t* info, void* user_data) {
    (void)user_data;
    printf("Key: %s, Type: %d, Size: %u\n", info->key, info->type,
           info->value_size);
    return true; /* Continue iteration */
}

/**
 * \brief           Query and enumeration example
 * \details         Shows how to query and enumerate configurations.
 *
 * \code{.c}
 * void query_example(void) {
 *     config_init(NULL);
 *
 *     // Store some values
 *     config_set_i32("app.timeout", 5000);
 *     config_set_str("app.name", "MyApp");
 *
 *     // Check if key exists
 *     bool exists = false;
 *     config_exists("app.timeout", &exists);
 *
 *     // Get value type
 *     config_type_t type;
 *     config_get_type("app.timeout", &type);
 *
 *     // Get total count
 *     size_t count = 0;
 *     config_get_count(&count);
 *
 *     // Iterate over all entries
 *     config_iterate(list_config_cb, NULL);
 *
 *     // Delete a key
 *     config_delete("app.timeout");
 *
 *     config_deinit();
 * }
 * \endcode
 */
void query_example(void) {
    config_init(NULL);

    /* Store some values */
    config_set_i32("app.timeout", 5000);
    config_set_str("app.name", "MyApp");

    /* Check if key exists */
    bool exists = false;
    config_exists("app.timeout", &exists);

    /* Get value type */
    config_type_t type;
    config_get_type("app.timeout", &type);

    (void)type;

    /* Get total count */
    size_t count = 0;
    config_get_count(&count);

    (void)count;

    /* Iterate over all entries */
    config_iterate(list_config_cb, NULL);

    /* Delete a key */
    config_delete("app.timeout");

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 8: JSON Import/Export                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           JSON import/export example
 * \details         Shows how to export and import configurations in JSON
 * format.
 *
 * \code{.c}
 * void json_export_import_example(void) {
 *     config_init(NULL);
 *
 *     // Store some values
 *     config_set_i32("app.timeout", 5000);
 *     config_set_str("app.name", "MyApp");
 *     config_set_bool("debug.enabled", true);
 *
 *     // Get required buffer size for export
 *     size_t export_size = 0;
 *     config_get_export_size(CONFIG_FORMAT_JSON, 0, &export_size);
 *
 *     // Export to JSON
 *     char buffer[1024];
 *     size_t actual_size = 0;
 *     config_export(CONFIG_FORMAT_JSON, 0, buffer, sizeof(buffer),
 * &actual_size);
 *
 *     // JSON output example:
 *     // {"app.timeout":5000,"app.name":"MyApp","debug.enabled":true}
 *
 *     // Clear and reimport
 *     config_deinit();
 *     config_init(NULL);
 *
 *     // Import from JSON
 *     config_import(CONFIG_FORMAT_JSON, 0, buffer, actual_size);
 *
 *     // Verify imported values
 *     int32_t timeout = 0;
 *     config_get_i32("app.timeout", &timeout, 0);
 *
 *     config_deinit();
 * }
 * \endcode
 */
void json_export_import_example(void) {
    config_init(NULL);

    /* Store some values */
    config_set_i32("app.timeout", 5000);
    config_set_str("app.name", "MyApp");
    config_set_bool("debug.enabled", true);

    /* Get required buffer size for export */
    size_t export_size = 0;
    config_get_export_size(CONFIG_FORMAT_JSON, 0, &export_size);

    (void)export_size;

    /* Export to JSON */
    char buffer[1024];
    size_t actual_size = 0;
    config_export(CONFIG_FORMAT_JSON, 0, buffer, sizeof(buffer), &actual_size);

    /* Clear and reimport */
    config_deinit();
    config_init(NULL);

    /* Import from JSON */
    config_import(CONFIG_FORMAT_JSON, 0, buffer, actual_size);

    /* Verify imported values */
    int32_t timeout = 0;
    config_get_i32("app.timeout", &timeout, 0);

    (void)timeout;

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 9: Binary Import/Export                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Binary import/export example
 * \details         Shows how to export and import configurations in binary
 * format.
 *
 * \code{.c}
 * void binary_export_import_example(void) {
 *     config_init(NULL);
 *
 *     // Store values including binary data
 *     config_set_i32("sensor.id", 12345);
 *     uint8_t calibration[] = {0x01, 0x02, 0x03, 0x04};
 *     config_set_blob("sensor.cal", calibration, sizeof(calibration));
 *
 *     // Export to binary format (more compact than JSON)
 *     uint8_t buffer[512];
 *     size_t actual_size = 0;
 *     config_export(CONFIG_FORMAT_BINARY, 0, buffer, sizeof(buffer),
 * &actual_size);
 *
 *     // Clear and reimport
 *     config_deinit();
 *     config_init(NULL);
 *
 *     // Import from binary
 *     config_import(CONFIG_FORMAT_BINARY, 0, buffer, actual_size);
 *
 *     config_deinit();
 * }
 * \endcode
 */
void binary_export_import_example(void) {
    config_init(NULL);

    /* Store values including binary data */
    config_set_i32("sensor.id", 12345);
    uint8_t calibration[] = {0x01, 0x02, 0x03, 0x04};
    config_set_blob("sensor.cal", calibration, sizeof(calibration));

    /* Export to binary format (more compact than JSON) */
    uint8_t buffer[512];
    size_t actual_size = 0;
    config_export(CONFIG_FORMAT_BINARY, 0, buffer, sizeof(buffer),
                  &actual_size);

    /* Clear and reimport */
    config_deinit();
    config_init(NULL);

    /* Import from binary */
    config_import(CONFIG_FORMAT_BINARY, 0, buffer, actual_size);

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 10: Encryption                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Encryption example
 * \details         Shows how to store encrypted configuration values.
 *
 * \code{.c}
 * void encryption_example(void) {
 *     config_init(NULL);
 *
 *     // Set encryption key (AES-128)
 *     uint8_t key[16] = {
 *         0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
 *         0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
 *     };
 *     config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);
 *
 *     // Store encrypted values
 *     config_set_str_encrypted("wifi.password", "MySecretPassword");
 *     config_set_blob_encrypted("api.key", "api-key-12345", 13);
 *
 *     // Check if a key is encrypted
 *     bool is_encrypted = false;
 *     config_is_encrypted("wifi.password", &is_encrypted);
 *
 *     // Read encrypted values (automatically decrypted)
 *     char password[64];
 *     config_get_str("wifi.password", password, sizeof(password));
 *
 *     // Clear encryption key when done
 *     config_clear_encryption_key();
 *
 *     config_deinit();
 * }
 * \endcode
 */
void encryption_example(void) {
    config_init(NULL);

    /* Set encryption key (AES-128) */
    uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    config_set_encryption_key(key, sizeof(key), CONFIG_CRYPTO_AES128);

    /* Store encrypted values */
    config_set_str_encrypted("wifi.password", "MySecretPassword");
    config_set_blob_encrypted("api.key", "api-key-12345", 13);

    /* Check if a key is encrypted */
    bool is_encrypted = false;
    config_is_encrypted("wifi.password", &is_encrypted);

    (void)is_encrypted;

    /* Read encrypted values (automatically decrypted) */
    char password[64];
    config_get_str("wifi.password", password, sizeof(password));

    /* Clear encryption key when done */
    config_clear_encryption_key();

    config_deinit();
}

/*---------------------------------------------------------------------------*/
/* Example 11: Error Handling                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Error handling example
 * \details         Shows how to handle errors from Config Manager.
 *
 * \code{.c}
 * void error_handling_example(void) {
 *     config_status_t status;
 *
 *     // Initialize
 *     status = config_init(NULL);
 *     if (status != CONFIG_OK) {
 *         printf("Init failed: %s\n", config_error_to_str(status));
 *         return;
 *     }
 *
 *     // Try to get non-existent key
 *     int32_t value = 0;
 *     status = config_get_i32("nonexistent.key", &value, 0);
 *     if (status == CONFIG_ERR_NOT_FOUND) {
 *         printf("Key not found, using default\n");
 *     }
 *
 *     // Get last error
 *     config_status_t last_error = config_get_last_error();
 *     printf("Last error: %s\n", config_error_to_str(last_error));
 *
 *     config_deinit();
 * }
 * \endcode
 */
void error_handling_example(void) {
    config_status_t status;

    /* Initialize */
    status = config_init(NULL);
    if (status != CONFIG_OK) {
        printf("Init failed: %s\n", config_error_to_str(status));
        return;
    }

    /* Try to get non-existent key */
    int32_t value = 0;
    status = config_get_i32("nonexistent.key", &value, 0);
    if (status == CONFIG_ERR_NOT_FOUND) {
        printf("Key not found, using default\n");
    }

    /* Get last error */
    config_status_t last_error = config_get_last_error();
    printf("Last error: %s\n", config_error_to_str(last_error));

    (void)value;

    config_deinit();
}
