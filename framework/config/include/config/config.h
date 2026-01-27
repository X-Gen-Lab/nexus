/**
 * \file            config.h
 * \brief           Config Manager Core API
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \brief           Configuration management interface for Nexus embedded
 * platform.
 *
 * The Config Manager provides:
 * - Key-value configuration storage
 * - Multiple data types (int, float, bool, string, blob)
 * - Namespace isolation
 * - Default value management
 * - Change notification callbacks
 * - Persistent storage backends
 * - Import/Export functionality
 * - Optional encryption
 *
 * Example usage:
 * \code{.c}
 * #include "config/config.h"
 *
 * void app_init(void) {
 *     config_init(NULL);  // Use default config
 *
 *     // Store values
 *     config_set_i32("app.timeout", 5000);
 *     config_set_str("app.name", "MyApp");
 *
 *     // Read values
 *     int32_t timeout = 0;
 *     config_get_i32("app.timeout", &timeout, 1000);
 * }
 * \endcode
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "config_backend.h"
#include "config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        CONFIG Config Manager
 * \brief           Core configuration management functionality
 * \{
 */

/**
 * \brief           Config Manager configuration structure
 */
typedef struct {
    uint16_t max_keys;       /**< Maximum key count (32-256) */
    uint8_t max_key_len;     /**< Maximum key length (16-64) */
    uint16_t max_value_size; /**< Maximum value size (64-1024) */
    uint8_t max_namespaces;  /**< Maximum namespace count */
    uint8_t max_callbacks;   /**< Maximum callback count */
    bool auto_commit;        /**< Auto-commit mode */
} config_manager_config_t;

/**
 * \brief           Default configuration initializer
 */
#define CONFIG_MANAGER_CONFIG_DEFAULT                                          \
    {.max_keys = CONFIG_DEFAULT_MAX_KEYS,                                      \
     .max_key_len = CONFIG_DEFAULT_MAX_KEY_LEN,                                \
     .max_value_size = CONFIG_DEFAULT_MAX_VALUE_SIZE,                          \
     .max_namespaces = CONFIG_DEFAULT_MAX_NAMESPACES,                          \
     .max_callbacks = CONFIG_DEFAULT_MAX_CALLBACKS,                            \
     .auto_commit = false}

/**
 * \name            Initialization and Configuration
 * \{
 */

/**
 * \brief           Initialize the Config Manager
 * \param[in]       config: Configuration structure, or NULL for defaults
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_init(const config_manager_config_t* config);

/**
 * \brief           Deinitialize the Config Manager
 * \return          CONFIG_OK on success, error code otherwise
 * \note            Releases all resources
 */
config_status_t config_deinit(void);

/**
 * \brief           Check if Config Manager is initialized
 * \return          true if initialized, false otherwise
 */
bool config_is_initialized(void);

/**
 * \brief           Set the storage backend
 * \param[in]       backend: Pointer to backend structure
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_backend(const config_backend_t* backend);

/**
 * \brief           Commit pending changes to storage
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_commit(void);

/**
 * \brief           Load configurations from storage
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_load(void);

/**
 * \}
 */

/**
 * \name            Integer Type Operations
 * \{
 */

/**
 * \brief           Set a 32-bit signed integer value
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_i32(const char* key, int32_t value);

/**
 * \brief           Get a 32-bit signed integer value
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_i32(const char* key, int32_t* value,
                               int32_t default_val);

/**
 * \brief           Set a 32-bit unsigned integer value
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_u32(const char* key, uint32_t value);

/**
 * \brief           Get a 32-bit unsigned integer value
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_u32(const char* key, uint32_t* value,
                               uint32_t default_val);

/**
 * \brief           Set a 64-bit signed integer value
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_i64(const char* key, int64_t value);

/**
 * \brief           Get a 64-bit signed integer value
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_i64(const char* key, int64_t* value,
                               int64_t default_val);

/**
 * \}
 */

/**
 * \name            Float and Boolean Operations
 * \{
 */

/**
 * \brief           Set a float value
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_float(const char* key, float value);

/**
 * \brief           Get a float value
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_float(const char* key, float* value,
                                 float default_val);

/**
 * \brief           Set a boolean value
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_bool(const char* key, bool value);

/**
 * \brief           Get a boolean value
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_bool(const char* key, bool* value, bool default_val);

/**
 * \}
 */

/**
 * \name            String and Blob Operations
 * \{
 */

/**
 * \brief           Set a string value
 * \param[in]       key: Configuration key
 * \param[in]       value: Null-terminated string to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_str(const char* key, const char* value);

/**
 * \brief           Get a string value
 * \param[in]       key: Configuration key
 * \param[out]      buffer: Buffer to store the string
 * \param[in]       buf_size: Size of the buffer
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_str(const char* key, char* buffer, size_t buf_size);

/**
 * \brief           Get the length of a stored string
 * \param[in]       key: Configuration key
 * \param[out]      len: Pointer to store the length (excluding null terminator)
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_str_len(const char* key, size_t* len);

/**
 * \brief           Set a binary blob value
 * \param[in]       key: Configuration key
 * \param[in]       data: Binary data to store
 * \param[in]       size: Size of the data
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_blob(const char* key, const void* data, size_t size);

/**
 * \brief           Get a binary blob value
 * \param[in]       key: Configuration key
 * \param[out]      buffer: Buffer to store the data
 * \param[in]       buf_size: Size of the buffer
 * \param[out]      actual_size: Actual size of the data (optional, can be NULL)
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_blob(const char* key, void* buffer, size_t buf_size,
                                size_t* actual_size);

/**
 * \brief           Get the size of a stored blob
 * \param[in]       key: Configuration key
 * \param[out]      len: Pointer to store the size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_blob_len(const char* key, size_t* len);

/**
 * \}
 */

/**
 * \name            Namespace Operations
 * \{
 */

/**
 * \brief           Namespace handle type
 */
typedef struct config_namespace* config_ns_handle_t;

/**
 * \brief           Open a namespace
 * \param[in]       name: Namespace name
 * \param[out]      handle: Pointer to store the namespace handle
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_open_namespace(const char* name,
                                      config_ns_handle_t* handle);

/**
 * \brief           Close a namespace
 * \param[in]       handle: Namespace handle
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_close_namespace(config_ns_handle_t handle);

/**
 * \brief           Erase all keys in a namespace
 * \param[in]       name: Namespace name
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_erase_namespace(const char* name);

/**
 * \brief           Set a 32-bit signed integer in namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_set_i32(config_ns_handle_t ns, const char* key,
                                  int32_t value);

/**
 * \brief           Get a 32-bit signed integer from namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_get_i32(config_ns_handle_t ns, const char* key,
                                  int32_t* value, int32_t default_val);

/**
 * \brief           Set a 32-bit unsigned integer in namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[in]       value: Value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_set_u32(config_ns_handle_t ns, const char* key,
                                  uint32_t value);

/**
 * \brief           Get a 32-bit unsigned integer from namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_get_u32(config_ns_handle_t ns, const char* key,
                                  uint32_t* value, uint32_t default_val);

/**
 * \brief           Set a string in namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[in]       value: String value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_set_str(config_ns_handle_t ns, const char* key,
                                  const char* value);

/**
 * \brief           Get a string from namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[out]      buffer: Buffer to store the string
 * \param[in]       buf_size: Size of the buffer
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_get_str(config_ns_handle_t ns, const char* key,
                                  char* buffer, size_t buf_size);

/**
 * \brief           Set a boolean in namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[in]       value: Boolean value to store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_set_bool(config_ns_handle_t ns, const char* key,
                                   bool value);

/**
 * \brief           Get a boolean from namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[out]      value: Pointer to store the value
 * \param[in]       default_val: Default value if key not found
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_get_bool(config_ns_handle_t ns, const char* key,
                                   bool* value, bool default_val);

/**
 * \brief           Check if a key exists in namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \param[out]      exists: Pointer to store the result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_exists(config_ns_handle_t ns, const char* key,
                                 bool* exists);

/**
 * \brief           Delete a key from namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       key: Configuration key
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_delete(config_ns_handle_t ns, const char* key);

/**
 * \}
 */

/**
 * \name            Default Value Management
 * \{
 */

/**
 * \brief           Default value definition structure
 */
typedef struct {
    const char* key;    /**< Configuration key */
    config_type_t type; /**< Value type */
    union {
        int32_t i32_val;     /**< 32-bit signed integer */
        uint32_t u32_val;    /**< 32-bit unsigned integer */
        int64_t i64_val;     /**< 64-bit signed integer */
        float float_val;     /**< Float value */
        bool bool_val;       /**< Boolean value */
        const char* str_val; /**< String value */
    } value;                 /**< Default value */
} config_default_t;

/**
 * \brief           Set default value for a 32-bit signed integer
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_i32(const char* key, int32_t value);

/**
 * \brief           Set default value for a 32-bit unsigned integer
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_u32(const char* key, uint32_t value);

/**
 * \brief           Set default value for a 64-bit signed integer
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_i64(const char* key, int64_t value);

/**
 * \brief           Set default value for a float
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_float(const char* key, float value);

/**
 * \brief           Set default value for a boolean
 * \param[in]       key: Configuration key
 * \param[in]       value: Default value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_bool(const char* key, bool value);

/**
 * \brief           Set default value for a string
 * \param[in]       key: Configuration key
 * \param[in]       value: Default string value
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_default_str(const char* key, const char* value);

/**
 * \brief           Reset a key to its default value
 * \param[in]       key: Configuration key
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_reset_to_default(const char* key);

/**
 * \brief           Reset all keys to their default values
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_reset_all_to_defaults(void);

/**
 * \brief           Register multiple default values
 * \param[in]       defaults: Array of default value definitions
 * \param[in]       count: Number of defaults
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_register_defaults(const config_default_t* defaults,
                                         size_t count);

/**
 * \}
 */

/**
 * \name            Change Notification Callbacks
 * \{
 */

/**
 * \brief           Configuration change callback function type
 * \param[in]       key: Key that changed
 * \param[in]       type: Value type
 * \param[in]       old_value: Previous value (may be NULL)
 * \param[in]       new_value: New value
 * \param[in]       user_data: User-provided context
 */
typedef void (*config_change_cb_t)(const char* key, config_type_t type,
                                   const void* old_value, const void* new_value,
                                   void* user_data);

/**
 * \brief           Callback handle type
 */
typedef struct config_callback* config_cb_handle_t;

/**
 * \brief           Register a callback for a specific key
 * \param[in]       key: Configuration key to watch
 * \param[in]       callback: Callback function
 * \param[in]       user_data: User-provided context
 * \param[out]      handle: Pointer to store the callback handle
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_register_callback(const char* key,
                                         config_change_cb_t callback,
                                         void* user_data,
                                         config_cb_handle_t* handle);

/**
 * \brief           Register a wildcard callback for all key changes
 * \param[in]       callback: Callback function
 * \param[in]       user_data: User-provided context
 * \param[out]      handle: Pointer to store the callback handle
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_register_wildcard_callback(config_change_cb_t callback,
                                                  void* user_data,
                                                  config_cb_handle_t* handle);

/**
 * \brief           Unregister a callback
 * \param[in]       handle: Callback handle
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_unregister_callback(config_cb_handle_t handle);

/**
 * \}
 */

/**
 * \name            Query and Enumeration
 * \{
 */

/**
 * \brief           Configuration entry information
 */
typedef struct {
    char key[CONFIG_DEFAULT_MAX_KEY_LEN]; /**< Configuration key */
    config_type_t type;                   /**< Value type */
    uint16_t value_size;                  /**< Value size in bytes */
    uint8_t flags;                        /**< Entry flags */
} config_entry_info_t;

/**
 * \brief           Iteration callback function type
 * \param[in]       info: Entry information
 * \param[in]       user_data: User-provided context
 * \return          true to continue iteration, false to stop
 */
typedef bool (*config_iterate_cb_t)(const config_entry_info_t* info,
                                    void* user_data);

/**
 * \brief           Check if a key exists
 * \param[in]       key: Configuration key
 * \param[out]      exists: Pointer to store the result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_exists(const char* key, bool* exists);

/**
 * \brief           Get the type of a stored value
 * \param[in]       key: Configuration key
 * \param[out]      type: Pointer to store the type
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_type(const char* key, config_type_t* type);

/**
 * \brief           Delete a configuration key
 * \param[in]       key: Configuration key
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_delete(const char* key);

/**
 * \brief           Get the number of stored keys
 * \param[out]      count: Pointer to store the count
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_count(size_t* count);

/**
 * \brief           Iterate over all configuration entries
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_iterate(config_iterate_cb_t callback, void* user_data);

/**
 * \brief           Iterate over entries in a namespace
 * \param[in]       ns: Namespace handle
 * \param[in]       callback: Iteration callback
 * \param[in]       user_data: User-provided context
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_ns_iterate(config_ns_handle_t ns,
                                  config_iterate_cb_t callback,
                                  void* user_data);

/**
 * \}
 */

/**
 * \name            Import/Export Operations
 * \{
 */

/**
 * \brief           Get the required buffer size for export
 * \param[in]       format: Export format
 * \param[in]       flags: Export flags
 * \param[out]      size: Pointer to store the required size
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_get_export_size(config_format_t format,
                                       config_export_flags_t flags,
                                       size_t* size);

/**
 * \brief           Export all configurations
 * \param[in]       format: Export format
 * \param[in]       flags: Export flags
 * \param[out]      buffer: Buffer to store exported data
 * \param[in]       buf_size: Size of the buffer
 * \param[out]      actual_size: Actual size of exported data
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_export(config_format_t format,
                              config_export_flags_t flags, void* buffer,
                              size_t buf_size, size_t* actual_size);

/**
 * \brief           Export configurations from a specific namespace
 * \param[in]       ns_name: Namespace name
 * \param[in]       format: Export format
 * \param[in]       flags: Export flags
 * \param[out]      buffer: Buffer to store exported data
 * \param[in]       buf_size: Size of the buffer
 * \param[out]      actual_size: Actual size of exported data
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_export_namespace(const char* ns_name,
                                        config_format_t format,
                                        config_export_flags_t flags,
                                        void* buffer, size_t buf_size,
                                        size_t* actual_size);

/**
 * \brief           Import configurations
 * \param[in]       format: Import format
 * \param[in]       flags: Import flags
 * \param[in]       data: Data to import
 * \param[in]       size: Size of the data
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_import(config_format_t format,
                              config_import_flags_t flags, const void* data,
                              size_t size);

/**
 * \brief           Import configurations to a specific namespace
 * \param[in]       ns_name: Namespace name
 * \param[in]       format: Import format
 * \param[in]       flags: Import flags
 * \param[in]       data: Data to import
 * \param[in]       size: Size of the data
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_import_namespace(const char* ns_name,
                                        config_format_t format,
                                        config_import_flags_t flags,
                                        const void* data, size_t size);

/**
 * \}
 */

/**
 * \name            Encryption Operations
 * \{
 */

/**
 * \brief           Set the encryption key
 * \param[in]       key: Encryption key
 * \param[in]       key_len: Key length in bytes
 * \param[in]       algo: Encryption algorithm
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_encryption_key(const uint8_t* key, size_t key_len,
                                          config_crypto_algo_t algo);

/**
 * \brief           Clear the encryption key
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_clear_encryption_key(void);

/**
 * \brief           Set an encrypted string value
 * \param[in]       key: Configuration key
 * \param[in]       value: String value to encrypt and store
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_str_encrypted(const char* key, const char* value);

/**
 * \brief           Set an encrypted blob value
 * \param[in]       key: Configuration key
 * \param[in]       data: Data to encrypt and store
 * \param[in]       size: Size of the data
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_set_blob_encrypted(const char* key, const void* data,
                                          size_t size);

/**
 * \brief           Check if a key is encrypted
 * \param[in]       key: Configuration key
 * \param[out]      encrypted: Pointer to store the result
 * \return          CONFIG_OK on success, error code otherwise
 */
config_status_t config_is_encrypted(const char* key, bool* encrypted);

/**
 * \brief           Rotate the encryption key
 * \param[in]       new_key: New encryption key
 * \param[in]       key_len: Key length in bytes
 * \param[in]       algo: Encryption algorithm
 * \return          CONFIG_OK on success, error code otherwise
 * \note            Re-encrypts all encrypted keys with the new key
 */
config_status_t config_rotate_encryption_key(const uint8_t* new_key,
                                             size_t key_len,
                                             config_crypto_algo_t algo);

/**
 * \}
 */

/**
 * \name            Error Handling
 * \{
 */

/**
 * \brief           Get the last error code
 * \return          Last error code
 */
config_status_t config_get_last_error(void);

/**
 * \brief           Convert error code to string
 * \param[in]       status: Error code
 * \return          Error string
 */
const char* config_error_to_str(config_status_t status);

/**
 * \}
 */

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
