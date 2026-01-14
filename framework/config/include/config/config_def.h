/**
 * \file            config_def.h
 * \brief           Config Manager Common Definitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef CONFIG_DEF_H
#define CONFIG_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        CONFIG_DEF Config Definitions
 * \brief           Common definitions for Config Manager framework
 * \{
 */

/**
 * \brief           Config Manager status codes
 */
typedef enum {
    CONFIG_OK = 0,                       /**< Operation successful */
    CONFIG_ERROR = 1,                    /**< Generic error */
    CONFIG_ERROR_INVALID_PARAM = 2,      /**< Invalid parameter */
    CONFIG_ERROR_NOT_INIT = 3,           /**< Not initialized */
    CONFIG_ERROR_ALREADY_INIT = 4,       /**< Already initialized */
    CONFIG_ERROR_NO_MEMORY = 5,          /**< Out of memory */
    CONFIG_ERROR_NOT_FOUND = 6,          /**< Key not found */
    CONFIG_ERROR_ALREADY_EXISTS = 7,     /**< Key already exists */
    CONFIG_ERROR_TYPE_MISMATCH = 8,      /**< Type mismatch */
    CONFIG_ERROR_KEY_TOO_LONG = 9,       /**< Key name too long */
    CONFIG_ERROR_VALUE_TOO_LARGE = 10,   /**< Value size too large */
    CONFIG_ERROR_BUFFER_TOO_SMALL = 11,  /**< Buffer too small */
    CONFIG_ERROR_NO_SPACE = 12,          /**< Storage space full */
    CONFIG_ERROR_NVS_READ = 13,          /**< NVS read failure */
    CONFIG_ERROR_NVS_WRITE = 14,         /**< NVS write failure */
    CONFIG_ERROR_INVALID_FORMAT = 15,    /**< Invalid format */
    CONFIG_ERROR_NO_ENCRYPTION_KEY = 16, /**< Encryption key not set */
    CONFIG_ERROR_CRYPTO_FAILED = 17,     /**< Encryption/decryption failed */
    CONFIG_ERROR_NO_BACKEND = 18,        /**< Backend not set */
} config_status_t;

/**
 * \brief           Config data types
 */
typedef enum {
    CONFIG_TYPE_I32 = 0,    /**< 32-bit signed integer */
    CONFIG_TYPE_U32 = 1,    /**< 32-bit unsigned integer */
    CONFIG_TYPE_I64 = 2,    /**< 64-bit signed integer */
    CONFIG_TYPE_FLOAT = 3,  /**< Single precision float */
    CONFIG_TYPE_BOOL = 4,   /**< Boolean */
    CONFIG_TYPE_STRING = 5, /**< Null-terminated string */
    CONFIG_TYPE_BLOB = 6,   /**< Binary data */
} config_type_t;

/**
 * \brief           Config entry flags
 */
typedef enum {
    CONFIG_FLAG_NONE = 0,              /**< No flags */
    CONFIG_FLAG_ENCRYPTED = (1 << 0),  /**< Value is encrypted */
    CONFIG_FLAG_READONLY = (1 << 1),   /**< Read-only configuration */
    CONFIG_FLAG_PERSISTENT = (1 << 2), /**< Requires persistence */
} config_flags_t;

/**
 * \brief           Export format types
 */
typedef enum {
    CONFIG_FORMAT_JSON = 0,   /**< JSON format */
    CONFIG_FORMAT_BINARY = 1, /**< Compact binary format */
} config_format_t;

/**
 * \brief           Export flags
 */
typedef enum {
    CONFIG_EXPORT_FLAG_NONE = 0,           /**< No flags */
    CONFIG_EXPORT_FLAG_DECRYPT = (1 << 0), /**< Decrypt values on export */
    CONFIG_EXPORT_FLAG_PRETTY = (1 << 1),  /**< Pretty print JSON */
} config_export_flags_t;

/**
 * \brief           Import flags
 */
typedef enum {
    CONFIG_IMPORT_FLAG_NONE = 0,         /**< No flags */
    CONFIG_IMPORT_FLAG_CLEAR = (1 << 0), /**< Clear existing before import */
    CONFIG_IMPORT_FLAG_SKIP_ERRORS = (1 << 1), /**< Skip errors and continue */
} config_import_flags_t;

/**
 * \brief           Encryption algorithms
 */
typedef enum {
    CONFIG_CRYPTO_AES128 = 0, /**< AES-128-CBC */
    CONFIG_CRYPTO_AES256 = 1, /**< AES-256-CBC */
} config_crypto_algo_t;

/**
 * \brief           Default configuration values
 * \{
 */
#ifndef CONFIG_DEFAULT_MAX_KEYS
#define CONFIG_DEFAULT_MAX_KEYS 64
#endif

#ifndef CONFIG_DEFAULT_MAX_KEY_LEN
#define CONFIG_DEFAULT_MAX_KEY_LEN 32
#endif

#ifndef CONFIG_DEFAULT_MAX_VALUE_SIZE
#define CONFIG_DEFAULT_MAX_VALUE_SIZE 256
#endif

#ifndef CONFIG_DEFAULT_MAX_NAMESPACES
#define CONFIG_DEFAULT_MAX_NAMESPACES 8
#endif

#ifndef CONFIG_DEFAULT_MAX_CALLBACKS
#define CONFIG_DEFAULT_MAX_CALLBACKS 16
#endif

#ifndef CONFIG_DEFAULT_MAX_DEFAULTS
#define CONFIG_DEFAULT_MAX_DEFAULTS 32
#endif

#ifndef CONFIG_MAX_NS_NAME_LEN
#define CONFIG_MAX_NS_NAME_LEN 16
#endif
/** \} */

/**
 * \brief           Configuration limits
 * \{
 */
#define CONFIG_MIN_MAX_KEYS       32
#define CONFIG_MAX_MAX_KEYS       256
#define CONFIG_MIN_MAX_KEY_LEN    16
#define CONFIG_MAX_MAX_KEY_LEN    64
#define CONFIG_MIN_MAX_VALUE_SIZE 64
#define CONFIG_MAX_MAX_VALUE_SIZE 1024
/** \} */

/**
 * \brief           Check if status is OK
 * \param[in]       status: Status to check
 * \return          true if OK, false otherwise
 */
#define CONFIG_IS_OK(status) ((status) == CONFIG_OK)

/**
 * \brief           Check if status is error
 * \param[in]       status: Status to check
 * \return          true if error, false otherwise
 */
#define CONFIG_IS_ERROR(status) ((status) != CONFIG_OK)

/**
 * \brief           Return if status is error
 * \param[in]       status: Status to check
 */
#define CONFIG_RETURN_IF_ERROR(status)                                         \
    do {                                                                       \
        config_status_t __status = (status);                                   \
        if (CONFIG_IS_ERROR(__status)) {                                       \
            return __status;                                                   \
        }                                                                      \
    } while (0)

/**
 * \brief           Unused parameter macro
 * \param[in]       x: Parameter to mark as unused
 */
#define CONFIG_UNUSED(x) ((void)(x))

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_DEF_H */
