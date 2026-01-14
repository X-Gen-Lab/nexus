/**
 * \file            config_export.c
 * \brief           Config Manager Export Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements configuration export functionality for the
 *                  Config Manager. Supports JSON and binary export formats.
 *
 *                  Requirements: 11.1, 11.3, 11.5, 11.8, 12.9
 */

#include "config_export.h"
#include "config/config.h"
#include "config_crypto.h"
#include "config_namespace.h"
#include "config_store.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Internal Structures                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Export context for size calculation
 */
typedef struct {
    size_t size;                 /**< Accumulated size */
    config_format_t format;      /**< Export format */
    config_export_flags_t flags; /**< Export flags */
    size_t entry_count;          /**< Number of entries */
    bool first_entry;            /**< First entry flag for JSON */
} export_size_ctx_t;

/**
 * \brief           Export context for data writing
 */
typedef struct {
    char* buffer;                /**< Output buffer */
    size_t buf_size;             /**< Buffer size */
    size_t offset;               /**< Current write offset */
    config_format_t format;      /**< Export format */
    config_export_flags_t flags; /**< Export flags */
    bool first_entry;            /**< First entry flag for JSON */
    config_status_t status;      /**< Operation status */
} export_write_ctx_t;

/*---------------------------------------------------------------------------*/
/* Binary Format Constants                                                   */
/*---------------------------------------------------------------------------*/

#define CONFIG_BINARY_MAGIC   0x43464742 /* "CFGB" */
#define CONFIG_BINARY_VERSION 1

/**
 * \brief           Binary export header structure
 */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct {
    uint32_t magic;       /**< Magic number */
    uint8_t version;      /**< Format version */
    uint8_t reserved[3];  /**< Reserved for alignment */
    uint32_t entry_count; /**< Number of entries */
    uint32_t data_size;   /**< Total data size */
}
#ifdef __GNUC__
__attribute__((packed))
#endif
config_binary_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/**
 * \brief           Binary entry header structure
 */
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
typedef struct {
    uint8_t key_len;      /**< Key length */
    uint8_t type;         /**< Value type */
    uint8_t flags;        /**< Entry flags */
    uint8_t namespace_id; /**< Namespace ID */
    uint16_t value_size;  /**< Value size */
}
#ifdef __GNUC__
__attribute__((packed))
#endif
config_binary_entry_header_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

/*---------------------------------------------------------------------------*/
/* Internal Helper Functions                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Write string to buffer with bounds checking
 * \param[in,out]   ctx: Export write context
 * \param[in]       str: String to write
 * \return          Number of bytes written
 */
static size_t export_write_str(export_write_ctx_t* ctx, const char* str) {
    if (ctx == NULL || str == NULL) {
        return 0;
    }

    size_t len = strlen(str);
    if (ctx->offset + len > ctx->buf_size) {
        ctx->status = CONFIG_ERROR_BUFFER_TOO_SMALL;
        return 0;
    }

    memcpy(ctx->buffer + ctx->offset, str, len);
    ctx->offset += len;
    return len;
}

/**
 * \brief           Write bytes to buffer with bounds checking
 * \param[in,out]   ctx: Export write context
 * \param[in]       data: Data to write
 * \param[in]       size: Size of data
 * \return          Number of bytes written
 */
static size_t export_write_bytes(export_write_ctx_t* ctx, const void* data,
                                 size_t size) {
    if (ctx == NULL || data == NULL || size == 0) {
        return 0;
    }

    if (ctx->offset + size > ctx->buf_size) {
        ctx->status = CONFIG_ERROR_BUFFER_TOO_SMALL;
        return 0;
    }

    memcpy(ctx->buffer + ctx->offset, data, size);
    ctx->offset += size;
    return size;
}

/**
 * \brief           Escape string for JSON output
 * \param[out]      out: Output buffer
 * \param[in]       out_size: Output buffer size
 * \param[in]       str: Input string
 * \return          Length of escaped string
 */
static size_t json_escape_string(char* out, size_t out_size, const char* str) {
    if (out == NULL || str == NULL || out_size == 0) {
        return 0;
    }

    size_t out_idx = 0;
    for (size_t i = 0; str[i] != '\0' && out_idx < out_size - 1; ++i) {
        char c = str[i];
        if (c == '"' || c == '\\') {
            if (out_idx + 2 >= out_size)
                break;
            out[out_idx++] = '\\';
            out[out_idx++] = c;
        } else if (c == '\n') {
            if (out_idx + 2 >= out_size)
                break;
            out[out_idx++] = '\\';
            out[out_idx++] = 'n';
        } else if (c == '\r') {
            if (out_idx + 2 >= out_size)
                break;
            out[out_idx++] = '\\';
            out[out_idx++] = 'r';
        } else if (c == '\t') {
            if (out_idx + 2 >= out_size)
                break;
            out[out_idx++] = '\\';
            out[out_idx++] = 't';
        } else if ((unsigned char)c < 0x20) {
            /* Control character - skip or encode as \uXXXX */
            if (out_idx + 6 >= out_size)
                break;
            out_idx += snprintf(out + out_idx, out_size - out_idx, "\\u%04x",
                                (unsigned char)c);
        } else {
            out[out_idx++] = c;
        }
    }
    out[out_idx] = '\0';
    return out_idx;
}

/**
 * \brief           Get type name string for JSON
 * \param[in]       type: Config type
 * \return          Type name string
 */
static const char* get_type_name(config_type_t type) {
    switch (type) {
        case CONFIG_TYPE_I32:
            return "i32";
        case CONFIG_TYPE_U32:
            return "u32";
        case CONFIG_TYPE_I64:
            return "i64";
        case CONFIG_TYPE_FLOAT:
            return "float";
        case CONFIG_TYPE_BOOL:
            return "bool";
        case CONFIG_TYPE_STRING:
            return "string";
        case CONFIG_TYPE_BLOB:
            return "blob";
        default:
            return "unknown";
    }
}

/**
 * \brief           Calculate JSON size for a single entry
 * \param[in]       info: Entry information
 * \param[in]       flags: Export flags
 * \return          Estimated size in bytes
 */
static size_t calc_json_entry_size(const config_store_entry_info_t* info,
                                   config_export_flags_t flags) {
    size_t size = 0;
    bool pretty = (flags & CONFIG_EXPORT_FLAG_PRETTY) != 0;

    /* Key and structure overhead */
    size += strlen(info->key) + 20; /* "key": { ... } */

    /* Type field */
    size += 20; /* "type": "xxx", */

    /* Value field - estimate based on type */
    switch (info->type) {
        case CONFIG_TYPE_I32:
        case CONFIG_TYPE_U32:
            size += 25; /* "value": -2147483648 */
            break;
        case CONFIG_TYPE_I64:
            size += 35; /* "value": -9223372036854775808 */
            break;
        case CONFIG_TYPE_FLOAT:
            size += 30; /* "value": -1.234567e+38 */
            break;
        case CONFIG_TYPE_BOOL:
            size += 20; /* "value": false */
            break;
        case CONFIG_TYPE_STRING:
            size += info->value_size * 2 + 15; /* Escaped string */
            break;
        case CONFIG_TYPE_BLOB:
            size += info->value_size * 2 + 15; /* Hex encoded */
            break;
        default:
            size += 20;
            break;
    }

    /* Flags if encrypted */
    if (info->flags & CONFIG_FLAG_ENCRYPTED) {
        size += 25; /* "encrypted": true */
    }

    /* Pretty print overhead */
    if (pretty) {
        size += 20; /* Newlines and indentation */
    }

    return size;
}

/*---------------------------------------------------------------------------*/
/* Size Calculation Callbacks                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback for calculating JSON export size
 * \param[in]       info: Entry information
 * \param[in]       user_data: Export size context
 * \return          true to continue iteration
 */
static bool calc_json_size_cb(const config_store_entry_info_t* info,
                              void* user_data) {
    export_size_ctx_t* ctx = (export_size_ctx_t*)user_data;

    ctx->size += calc_json_entry_size(info, ctx->flags);
    ctx->entry_count++;

    return true;
}

/**
 * \brief           Callback for calculating binary export size
 * \param[in]       info: Entry information
 * \param[in]       user_data: Export size context
 * \return          true to continue iteration
 */
static bool calc_binary_size_cb(const config_store_entry_info_t* info,
                                void* user_data) {
    export_size_ctx_t* ctx = (export_size_ctx_t*)user_data;

    /* Entry header + key + value */
    ctx->size += sizeof(config_binary_entry_header_t);
    ctx->size += strlen(info->key);
    ctx->size += info->value_size;
    ctx->entry_count++;

    return true;
}

/*---------------------------------------------------------------------------*/
/* JSON Export Implementation                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback for writing JSON entries
 * \param[in]       info: Entry information
 * \param[in]       user_data: Export write context
 * \return          true to continue iteration
 */
static bool write_json_entry_cb(const config_store_entry_info_t* info,
                                void* user_data) {
    export_write_ctx_t* ctx = (export_write_ctx_t*)user_data;
    char temp_buf[512];
    uint8_t value_buf[CONFIG_MAX_MAX_VALUE_SIZE];
    uint8_t decrypted_buf[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t value_size = sizeof(value_buf);
    bool pretty = (ctx->flags & CONFIG_EXPORT_FLAG_PRETTY) != 0;
    bool decrypt = (ctx->flags & CONFIG_EXPORT_FLAG_DECRYPT) != 0;
    const char* indent = pretty ? "  " : "";
    const char* newline = pretty ? "\n" : "";
    const char* space = pretty ? " " : "";
    bool is_encrypted = (info->flags & CONFIG_FLAG_ENCRYPTED) != 0;
    bool show_encrypted_flag = is_encrypted;

    /* Get the actual value */
    config_status_t status = config_store_get(
        info->key, NULL, value_buf, &value_size, NULL, info->namespace_id);
    if (status != CONFIG_OK) {
        return true; /* Skip this entry but continue */
    }

    /* Decrypt if requested and value is encrypted */
    uint8_t* output_buf = value_buf;
    size_t output_size = value_size;

    if (is_encrypted && decrypt && config_crypto_is_enabled()) {
        size_t decrypted_size = sizeof(decrypted_buf);
        status = config_crypto_decrypt(value_buf, value_size, decrypted_buf,
                                       &decrypted_size);
        if (status == CONFIG_OK) {
            output_buf = decrypted_buf;
            output_size = decrypted_size;
            show_encrypted_flag =
                false; /* Don't show encrypted flag for decrypted values */
        }
        /* If decryption fails, export the encrypted value as-is */
    }

    /* Write comma separator if not first entry */
    if (!ctx->first_entry) {
        export_write_str(ctx, ",");
        export_write_str(ctx, newline);
    }
    ctx->first_entry = false;

    /* Write key */
    export_write_str(ctx, indent);
    export_write_str(ctx, "\"");

    /* Escape key for JSON */
    json_escape_string(temp_buf, sizeof(temp_buf), info->key);
    export_write_str(ctx, temp_buf);

    export_write_str(ctx, "\":");
    export_write_str(ctx, space);
    export_write_str(ctx, "{");
    export_write_str(ctx, newline);

    /* Write type */
    export_write_str(ctx, indent);
    export_write_str(ctx, indent);
    snprintf(temp_buf, sizeof(temp_buf), "\"type\":%s\"%s\",", space,
             get_type_name(info->type));
    export_write_str(ctx, temp_buf);
    export_write_str(ctx, newline);

    /* Write value based on type */
    export_write_str(ctx, indent);
    export_write_str(ctx, indent);
    export_write_str(ctx, "\"value\":");
    export_write_str(ctx, space);

    switch (info->type) {
        case CONFIG_TYPE_I32: {
            int32_t val;
            memcpy(&val, output_buf, sizeof(val));
            snprintf(temp_buf, sizeof(temp_buf), "%d", (int)val);
            export_write_str(ctx, temp_buf);
            break;
        }
        case CONFIG_TYPE_U32: {
            uint32_t val;
            memcpy(&val, output_buf, sizeof(val));
            snprintf(temp_buf, sizeof(temp_buf), "%u", (unsigned)val);
            export_write_str(ctx, temp_buf);
            break;
        }
        case CONFIG_TYPE_I64: {
            int64_t val;
            memcpy(&val, output_buf, sizeof(val));
            snprintf(temp_buf, sizeof(temp_buf), "%lld", (long long)val);
            export_write_str(ctx, temp_buf);
            break;
        }
        case CONFIG_TYPE_FLOAT: {
            float val;
            memcpy(&val, output_buf, sizeof(val));
            snprintf(temp_buf, sizeof(temp_buf), "%g", (double)val);
            export_write_str(ctx, temp_buf);
            break;
        }
        case CONFIG_TYPE_BOOL: {
            bool val;
            memcpy(&val, output_buf, sizeof(val));
            export_write_str(ctx, val ? "true" : "false");
            break;
        }
        case CONFIG_TYPE_STRING: {
            export_write_str(ctx, "\"");
            json_escape_string(temp_buf, sizeof(temp_buf),
                               (const char*)output_buf);
            export_write_str(ctx, temp_buf);
            export_write_str(ctx, "\"");
            break;
        }
        case CONFIG_TYPE_BLOB: {
            /* Encode as hex string */
            export_write_str(ctx, "\"");
            for (size_t i = 0; i < output_size && ctx->status == CONFIG_OK;
                 ++i) {
                snprintf(temp_buf, sizeof(temp_buf), "%02x", output_buf[i]);
                export_write_str(ctx, temp_buf);
            }
            export_write_str(ctx, "\"");
            break;
        }
        default:
            export_write_str(ctx, "null");
            break;
    }

    /* Write encrypted flag if set and not decrypted */
    if (show_encrypted_flag) {
        export_write_str(ctx, ",");
        export_write_str(ctx, newline);
        export_write_str(ctx, indent);
        export_write_str(ctx, indent);
        export_write_str(ctx, "\"encrypted\":");
        export_write_str(ctx, space);
        export_write_str(ctx, "true");
    }

    export_write_str(ctx, newline);
    export_write_str(ctx, indent);
    export_write_str(ctx, "}");

    return ctx->status == CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Binary Export Implementation                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Callback for writing binary entries
 * \param[in]       info: Entry information
 * \param[in]       user_data: Export write context
 * \return          true to continue iteration
 */
static bool write_binary_entry_cb(const config_store_entry_info_t* info,
                                  void* user_data) {
    export_write_ctx_t* ctx = (export_write_ctx_t*)user_data;
    uint8_t value_buf[CONFIG_MAX_MAX_VALUE_SIZE];
    uint8_t decrypted_buf[CONFIG_MAX_MAX_VALUE_SIZE];
    size_t value_size = sizeof(value_buf);
    bool decrypt = (ctx->flags & CONFIG_EXPORT_FLAG_DECRYPT) != 0;
    bool is_encrypted = (info->flags & CONFIG_FLAG_ENCRYPTED) != 0;

    /* Get the actual value */
    config_status_t status = config_store_get(
        info->key, NULL, value_buf, &value_size, NULL, info->namespace_id);
    if (status != CONFIG_OK) {
        return true; /* Skip this entry but continue */
    }

    /* Decrypt if requested and value is encrypted */
    uint8_t* output_buf = value_buf;
    size_t output_size = value_size;
    uint8_t output_flags = info->flags;

    if (is_encrypted && decrypt && config_crypto_is_enabled()) {
        size_t decrypted_size = sizeof(decrypted_buf);
        status = config_crypto_decrypt(value_buf, value_size, decrypted_buf,
                                       &decrypted_size);
        if (status == CONFIG_OK) {
            output_buf = decrypted_buf;
            output_size = decrypted_size;
            output_flags &= ~CONFIG_FLAG_ENCRYPTED; /* Clear encrypted flag */
        }
        /* If decryption fails, export the encrypted value as-is */
    }

    /* Write entry header */
    config_binary_entry_header_t header;
    header.key_len = (uint8_t)strlen(info->key);
    header.type = (uint8_t)info->type;
    header.flags = output_flags;
    header.namespace_id = info->namespace_id;
    header.value_size = (uint16_t)output_size;

    export_write_bytes(ctx, &header, sizeof(header));

    /* Write key (without null terminator) */
    export_write_bytes(ctx, info->key, header.key_len);

    /* Write value */
    export_write_bytes(ctx, output_buf, output_size);

    return ctx->status == CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t config_get_export_size(config_format_t format,
                                       config_export_flags_t flags,
                                       size_t* size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    export_size_ctx_t ctx = {.size = 0,
                             .format = format,
                             .flags = flags,
                             .entry_count = 0,
                             .first_entry = true};

    config_status_t status;

    if (format == CONFIG_FORMAT_JSON) {
        /* JSON overhead: { ... } */
        ctx.size = 3; /* "{}" + null terminator */
        if (flags & CONFIG_EXPORT_FLAG_PRETTY) {
            ctx.size += 2; /* Newlines */
        }

        status = config_store_iterate(calc_json_size_cb, &ctx);
        if (status != CONFIG_OK) {
            return status;
        }

        /* Add comma separators */
        if (ctx.entry_count > 0) {
            ctx.size += (ctx.entry_count - 1) * 2; /* ", " */
        }
    } else if (format == CONFIG_FORMAT_BINARY) {
        /* Binary header */
        ctx.size = sizeof(config_binary_header_t);

        status = config_store_iterate(calc_binary_size_cb, &ctx);
        if (status != CONFIG_OK) {
            return status;
        }
    } else {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    *size = ctx.size;
    return CONFIG_OK;
}

config_status_t config_export(config_format_t format,
                              config_export_flags_t flags, void* buffer,
                              size_t buf_size, size_t* actual_size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (buffer == NULL || actual_size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Check required size first */
    size_t required_size = 0;
    config_status_t status =
        config_get_export_size(format, flags, &required_size);
    if (status != CONFIG_OK) {
        return status;
    }

    if (buf_size < required_size) {
        *actual_size = required_size;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    export_write_ctx_t ctx = {.buffer = (char*)buffer,
                              .buf_size = buf_size,
                              .offset = 0,
                              .format = format,
                              .flags = flags,
                              .first_entry = true,
                              .status = CONFIG_OK};

    if (format == CONFIG_FORMAT_JSON) {
        bool pretty = (flags & CONFIG_EXPORT_FLAG_PRETTY) != 0;
        const char* newline = pretty ? "\n" : "";

        /* Write opening brace */
        export_write_str(&ctx, "{");
        export_write_str(&ctx, newline);

        /* Write entries */
        status = config_store_iterate(write_json_entry_cb, &ctx);
        if (status != CONFIG_OK || ctx.status != CONFIG_OK) {
            return ctx.status != CONFIG_OK ? ctx.status : status;
        }

        /* Write closing brace */
        export_write_str(&ctx, newline);
        export_write_str(&ctx, "}");

        /* Null terminate */
        if (ctx.offset < buf_size) {
            ctx.buffer[ctx.offset] = '\0';
        }
    } else if (format == CONFIG_FORMAT_BINARY) {
        /* Count entries first */
        export_size_ctx_t size_ctx = {0};
        status = config_store_iterate(calc_binary_size_cb, &size_ctx);
        if (status != CONFIG_OK) {
            return status;
        }

        /* Write header */
        config_binary_header_t header = {
            .magic = CONFIG_BINARY_MAGIC,
            .version = CONFIG_BINARY_VERSION,
            .reserved = {0},
            .entry_count = (uint32_t)size_ctx.entry_count,
            .data_size =
                (uint32_t)(size_ctx.size - sizeof(config_binary_header_t))};
        export_write_bytes(&ctx, &header, sizeof(header));

        /* Write entries */
        status = config_store_iterate(write_binary_entry_cb, &ctx);
        if (status != CONFIG_OK || ctx.status != CONFIG_OK) {
            return ctx.status != CONFIG_OK ? ctx.status : status;
        }
    } else {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    *actual_size = ctx.offset;
    return CONFIG_OK;
}

config_status_t config_export_namespace(const char* ns_name,
                                        config_format_t format,
                                        config_export_flags_t flags,
                                        void* buffer, size_t buf_size,
                                        size_t* actual_size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns_name == NULL || buffer == NULL || actual_size == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get namespace ID */
    uint8_t ns_id;
    config_status_t status = config_namespace_get_id(ns_name, &ns_id);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Calculate size for this namespace */
    export_size_ctx_t size_ctx = {.size = 0,
                                  .format = format,
                                  .flags = flags,
                                  .entry_count = 0,
                                  .first_entry = true};

    if (format == CONFIG_FORMAT_JSON) {
        size_ctx.size = 3; /* "{}" + null */
        if (flags & CONFIG_EXPORT_FLAG_PRETTY) {
            size_ctx.size += 2;
        }
        status =
            config_store_iterate_namespace(ns_id, calc_json_size_cb, &size_ctx);
    } else if (format == CONFIG_FORMAT_BINARY) {
        size_ctx.size = sizeof(config_binary_header_t);
        status = config_store_iterate_namespace(ns_id, calc_binary_size_cb,
                                                &size_ctx);
    } else {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Add comma separators for JSON */
    if (format == CONFIG_FORMAT_JSON && size_ctx.entry_count > 0) {
        size_ctx.size += (size_ctx.entry_count - 1) * 2;
    }

    if (buf_size < size_ctx.size) {
        *actual_size = size_ctx.size;
        return CONFIG_ERROR_BUFFER_TOO_SMALL;
    }

    /* Write data */
    export_write_ctx_t ctx = {.buffer = (char*)buffer,
                              .buf_size = buf_size,
                              .offset = 0,
                              .format = format,
                              .flags = flags,
                              .first_entry = true,
                              .status = CONFIG_OK};

    if (format == CONFIG_FORMAT_JSON) {
        bool pretty = (flags & CONFIG_EXPORT_FLAG_PRETTY) != 0;
        const char* newline = pretty ? "\n" : "";

        export_write_str(&ctx, "{");
        export_write_str(&ctx, newline);

        status =
            config_store_iterate_namespace(ns_id, write_json_entry_cb, &ctx);
        if (status != CONFIG_OK || ctx.status != CONFIG_OK) {
            return ctx.status != CONFIG_OK ? ctx.status : status;
        }

        export_write_str(&ctx, newline);
        export_write_str(&ctx, "}");

        if (ctx.offset < buf_size) {
            ctx.buffer[ctx.offset] = '\0';
        }
    } else if (format == CONFIG_FORMAT_BINARY) {
        config_binary_header_t header = {
            .magic = CONFIG_BINARY_MAGIC,
            .version = CONFIG_BINARY_VERSION,
            .reserved = {0},
            .entry_count = (uint32_t)size_ctx.entry_count,
            .data_size =
                (uint32_t)(size_ctx.size - sizeof(config_binary_header_t))};
        export_write_bytes(&ctx, &header, sizeof(header));

        status =
            config_store_iterate_namespace(ns_id, write_binary_entry_cb, &ctx);
        if (status != CONFIG_OK || ctx.status != CONFIG_OK) {
            return ctx.status != CONFIG_OK ? ctx.status : status;
        }
    }

    *actual_size = ctx.offset;
    return CONFIG_OK;
}
