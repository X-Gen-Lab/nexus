/**
 * \file            config_import.c
 * \brief           Config Manager Import Implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements configuration import functionality for the
 *                  Config Manager. Supports JSON and binary import formats.
 *
 *                  Requirements: 11.2, 11.4, 11.6, 11.7, 11.9, 11.10
 */

#include "config_import.h"
#include "config/config.h"
#include "config_namespace.h"
#include "config_store.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Binary Format Constants (must match config_export.c)                      */
/*---------------------------------------------------------------------------*/

#define CONFIG_BINARY_MAGIC   0x43464742 /* "CFGB" */
#define CONFIG_BINARY_VERSION 1

/**
 * \brief           Binary import header structure
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
/* JSON Parser Structures                                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           JSON parser context
 */
typedef struct {
    const char* data;            /**< Input data */
    size_t size;                 /**< Data size */
    size_t pos;                  /**< Current position */
    config_import_flags_t flags; /**< Import flags */
    uint8_t namespace_id;        /**< Target namespace ID */
    config_status_t status;      /**< Parse status */
} json_parser_ctx_t;

/*---------------------------------------------------------------------------*/
/* JSON Parser Helper Functions                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Skip whitespace in JSON
 * \param[in,out]   ctx: Parser context
 */
static void json_skip_whitespace(json_parser_ctx_t* ctx) {
    while (ctx->pos < ctx->size) {
        char c = ctx->data[ctx->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ctx->pos++;
        } else {
            break;
        }
    }
}

/**
 * \brief           Check if current character matches expected
 * \param[in]       ctx: Parser context
 * \param[in]       expected: Expected character
 * \return          true if matches
 */
static bool json_expect_char(json_parser_ctx_t* ctx, char expected) {
    json_skip_whitespace(ctx);
    if (ctx->pos < ctx->size && ctx->data[ctx->pos] == expected) {
        ctx->pos++;
        return true;
    }
    return false;
}

/**
 * \brief           Parse a JSON string
 * \param[in,out]   ctx: Parser context
 * \param[out]      out: Output buffer
 * \param[in]       out_size: Output buffer size
 * \return          true on success
 */
static bool json_parse_string(json_parser_ctx_t* ctx, char* out,
                              size_t out_size) {
    json_skip_whitespace(ctx);

    if (ctx->pos >= ctx->size || ctx->data[ctx->pos] != '"') {
        return false;
    }
    ctx->pos++; /* Skip opening quote */

    size_t out_idx = 0;
    while (ctx->pos < ctx->size && out_idx < out_size - 1) {
        char c = ctx->data[ctx->pos++];

        if (c == '"') {
            out[out_idx] = '\0';
            return true;
        }

        if (c == '\\' && ctx->pos < ctx->size) {
            char escaped = ctx->data[ctx->pos++];
            switch (escaped) {
                case '"':
                    out[out_idx++] = '"';
                    break;
                case '\\':
                    out[out_idx++] = '\\';
                    break;
                case 'n':
                    out[out_idx++] = '\n';
                    break;
                case 'r':
                    out[out_idx++] = '\r';
                    break;
                case 't':
                    out[out_idx++] = '\t';
                    break;
                case 'u':
                    /* Unicode escape - simplified handling */
                    if (ctx->pos + 4 <= ctx->size) {
                        ctx->pos += 4;        /* Skip 4 hex digits */
                        out[out_idx++] = '?'; /* Placeholder */
                    }
                    break;
                default:
                    out[out_idx++] = escaped;
                    break;
            }
        } else {
            out[out_idx++] = c;
        }
    }

    return false; /* Unterminated string */
}

/**
 * \brief           Parse a JSON number (integer)
 * \param[in,out]   ctx: Parser context
 * \param[out]      value: Output value
 * \return          true on success
 */
static bool json_parse_int64(json_parser_ctx_t* ctx, int64_t* value) {
    json_skip_whitespace(ctx);

    if (ctx->pos >= ctx->size) {
        return false;
    }

    char num_buf[32];
    size_t num_idx = 0;
    bool negative = false;

    /* Handle negative sign */
    if (ctx->data[ctx->pos] == '-') {
        negative = true;
        num_buf[num_idx++] = ctx->data[ctx->pos++];
    }

    /* Parse digits */
    while (ctx->pos < ctx->size && num_idx < sizeof(num_buf) - 1) {
        char c = ctx->data[ctx->pos];
        if (isdigit((unsigned char)c)) {
            num_buf[num_idx++] = c;
            ctx->pos++;
        } else {
            break;
        }
    }

    if (num_idx == 0 || (negative && num_idx == 1)) {
        return false;
    }

    num_buf[num_idx] = '\0';
    *value = strtoll(num_buf, NULL, 10);
    return true;
}

/**
 * \brief           Parse a JSON float number
 * \param[in,out]   ctx: Parser context
 * \param[out]      value: Output value
 * \return          true on success
 */
static bool json_parse_float(json_parser_ctx_t* ctx, float* value) {
    json_skip_whitespace(ctx);

    if (ctx->pos >= ctx->size) {
        return false;
    }

    char num_buf[64];
    size_t num_idx = 0;

    /* Parse number including sign, decimal point, and exponent */
    while (ctx->pos < ctx->size && num_idx < sizeof(num_buf) - 1) {
        char c = ctx->data[ctx->pos];
        if (isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.' ||
            c == 'e' || c == 'E') {
            num_buf[num_idx++] = c;
            ctx->pos++;
        } else {
            break;
        }
    }

    if (num_idx == 0) {
        return false;
    }

    num_buf[num_idx] = '\0';
    *value = (float)strtod(num_buf, NULL);
    return true;
}

/**
 * \brief           Parse a JSON boolean
 * \param[in,out]   ctx: Parser context
 * \param[out]      value: Output value
 * \return          true on success
 */
static bool json_parse_bool(json_parser_ctx_t* ctx, bool* value) {
    json_skip_whitespace(ctx);

    if (ctx->pos + 4 <= ctx->size &&
        strncmp(ctx->data + ctx->pos, "true", 4) == 0) {
        ctx->pos += 4;
        *value = true;
        return true;
    }

    if (ctx->pos + 5 <= ctx->size &&
        strncmp(ctx->data + ctx->pos, "false", 5) == 0) {
        ctx->pos += 5;
        *value = false;
        return true;
    }

    return false;
}

/**
 * \brief           Parse hex string to binary data
 * \param[in]       hex: Hex string
 * \param[in]       hex_len: Hex string length
 * \param[out]      out: Output buffer
 * \param[in]       out_size: Output buffer size
 * \param[out]      actual_size: Actual decoded size
 * \return          true on success
 */
static bool hex_decode(const char* hex, size_t hex_len, uint8_t* out,
                       size_t out_size, size_t* actual_size) {
    if (hex_len % 2 != 0) {
        return false;
    }

    size_t decoded_size = hex_len / 2;
    if (decoded_size > out_size) {
        return false;
    }

    for (size_t i = 0; i < decoded_size; ++i) {
        char high = hex[i * 2];
        char low = hex[i * 2 + 1];

        uint8_t high_val, low_val;

        if (high >= '0' && high <= '9') {
            high_val = (uint8_t)(high - '0');
        } else if (high >= 'a' && high <= 'f') {
            high_val = (uint8_t)(high - 'a' + 10);
        } else if (high >= 'A' && high <= 'F') {
            high_val = (uint8_t)(high - 'A' + 10);
        } else {
            return false;
        }

        if (low >= '0' && low <= '9') {
            low_val = (uint8_t)(low - '0');
        } else if (low >= 'a' && low <= 'f') {
            low_val = (uint8_t)(low - 'a' + 10);
        } else if (low >= 'A' && low <= 'F') {
            low_val = (uint8_t)(low - 'A' + 10);
        } else {
            return false;
        }

        out[i] = (uint8_t)((high_val << 4) | low_val);
    }

    *actual_size = decoded_size;
    return true;
}

/**
 * \brief           Get config type from type name string
 * \param[in]       type_name: Type name string
 * \return          Config type, or -1 if invalid
 */
static int get_type_from_name(const char* type_name) {
    if (strcmp(type_name, "i32") == 0) {
        return CONFIG_TYPE_I32;
    }
    if (strcmp(type_name, "u32") == 0) {
        return CONFIG_TYPE_U32;
    }
    if (strcmp(type_name, "i64") == 0) {
        return CONFIG_TYPE_I64;
    }
    if (strcmp(type_name, "float") == 0) {
        return CONFIG_TYPE_FLOAT;
    }
    if (strcmp(type_name, "bool") == 0) {
        return CONFIG_TYPE_BOOL;
    }
    if (strcmp(type_name, "string") == 0) {
        return CONFIG_TYPE_STRING;
    }
    if (strcmp(type_name, "blob") == 0) {
        return CONFIG_TYPE_BLOB;
    }
    return -1;
}

/*---------------------------------------------------------------------------*/
/* JSON Import Implementation                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Parse and import a single JSON entry value
 * \param[in,out]   ctx: Parser context
 * \param[in]       key: Configuration key
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t json_import_entry(json_parser_ctx_t* ctx,
                                         const char* key) {
    char type_name[16] = {0};
    config_type_t type = CONFIG_TYPE_I32;
    bool has_type = false;
    bool has_value = false;
    uint8_t flags = 0;

    /* Temporary storage for value */
    union {
        int32_t i32;
        uint32_t u32;
        int64_t i64;
        float f;
        bool b;
        char str[CONFIG_MAX_MAX_VALUE_SIZE];
        uint8_t blob[CONFIG_MAX_MAX_VALUE_SIZE];
    } value;
    size_t value_size = 0;

    /* Expect opening brace for entry object */
    if (!json_expect_char(ctx, '{')) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Parse entry fields */
    bool first_field = true;
    while (ctx->pos < ctx->size) {
        json_skip_whitespace(ctx);

        /* Check for closing brace */
        if (ctx->pos < ctx->size && ctx->data[ctx->pos] == '}') {
            ctx->pos++;
            break;
        }

        /* Expect comma between fields */
        if (!first_field) {
            if (!json_expect_char(ctx, ',')) {
                return CONFIG_ERROR_INVALID_FORMAT;
            }
        }
        first_field = false;

        /* Parse field name */
        char field_name[32];
        if (!json_parse_string(ctx, field_name, sizeof(field_name))) {
            return CONFIG_ERROR_INVALID_FORMAT;
        }

        /* Expect colon */
        if (!json_expect_char(ctx, ':')) {
            return CONFIG_ERROR_INVALID_FORMAT;
        }

        /* Parse field value based on field name */
        if (strcmp(field_name, "type") == 0) {
            if (!json_parse_string(ctx, type_name, sizeof(type_name))) {
                return CONFIG_ERROR_INVALID_FORMAT;
            }
            int type_val = get_type_from_name(type_name);
            if (type_val < 0) {
                /* Unknown type - mark as invalid but continue parsing to skip
                 * the entry */
                has_type = false;
                /* Skip the rest of this entry by finding the closing brace */
                int depth = 1;
                while (ctx->pos < ctx->size && depth > 0) {
                    char c = ctx->data[ctx->pos];
                    if (c == '{')
                        depth++;
                    else if (c == '}')
                        depth--;
                    else if (c == '"') {
                        /* Skip string content */
                        ctx->pos++;
                        while (ctx->pos < ctx->size &&
                               ctx->data[ctx->pos] != '"') {
                            if (ctx->data[ctx->pos] == '\\' &&
                                ctx->pos + 1 < ctx->size) {
                                ctx->pos++; /* Skip escaped char */
                            }
                            ctx->pos++;
                        }
                    }
                    ctx->pos++;
                }
                return CONFIG_ERROR_INVALID_FORMAT;
            }
            type = (config_type_t)type_val;
            has_type = true;
        } else if (strcmp(field_name, "value") == 0) {
            /* Parse value based on type (need type first) */
            if (!has_type) {
                /* Type not yet parsed, try to infer from JSON value */
                json_skip_whitespace(ctx);
                if (ctx->pos >= ctx->size) {
                    return CONFIG_ERROR_INVALID_FORMAT;
                }

                char c = ctx->data[ctx->pos];
                if (c == '"') {
                    /* String or blob - assume string for now */
                    type = CONFIG_TYPE_STRING;
                    if (!json_parse_string(ctx, value.str, sizeof(value.str))) {
                        return CONFIG_ERROR_INVALID_FORMAT;
                    }
                    value_size = strlen(value.str) + 1;
                } else if (c == 't' || c == 'f') {
                    type = CONFIG_TYPE_BOOL;
                    if (!json_parse_bool(ctx, &value.b)) {
                        return CONFIG_ERROR_INVALID_FORMAT;
                    }
                    value_size = sizeof(bool);
                } else if (c == '-' || isdigit((unsigned char)c)) {
                    /* Number - check if float */
                    size_t start_pos = ctx->pos;
                    bool is_float = false;
                    while (ctx->pos < ctx->size) {
                        char nc = ctx->data[ctx->pos];
                        if (nc == '.' || nc == 'e' || nc == 'E') {
                            is_float = true;
                        }
                        if (!isdigit((unsigned char)nc) && nc != '-' &&
                            nc != '+' && nc != '.' && nc != 'e' && nc != 'E') {
                            break;
                        }
                        ctx->pos++;
                    }
                    ctx->pos = start_pos; /* Reset position */

                    if (is_float) {
                        type = CONFIG_TYPE_FLOAT;
                        if (!json_parse_float(ctx, &value.f)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value_size = sizeof(float);
                    } else {
                        type = CONFIG_TYPE_I32;
                        int64_t i64_val;
                        if (!json_parse_int64(ctx, &i64_val)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value.i32 = (int32_t)i64_val;
                        value_size = sizeof(int32_t);
                    }
                } else {
                    return CONFIG_ERROR_INVALID_FORMAT;
                }
                has_type = true;
            } else {
                /* Type is known, parse accordingly */
                switch (type) {
                    case CONFIG_TYPE_I32: {
                        int64_t i64_val;
                        if (!json_parse_int64(ctx, &i64_val)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value.i32 = (int32_t)i64_val;
                        value_size = sizeof(int32_t);
                        break;
                    }
                    case CONFIG_TYPE_U32: {
                        int64_t i64_val;
                        if (!json_parse_int64(ctx, &i64_val)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value.u32 = (uint32_t)i64_val;
                        value_size = sizeof(uint32_t);
                        break;
                    }
                    case CONFIG_TYPE_I64: {
                        if (!json_parse_int64(ctx, &value.i64)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value_size = sizeof(int64_t);
                        break;
                    }
                    case CONFIG_TYPE_FLOAT: {
                        if (!json_parse_float(ctx, &value.f)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value_size = sizeof(float);
                        break;
                    }
                    case CONFIG_TYPE_BOOL: {
                        if (!json_parse_bool(ctx, &value.b)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value_size = sizeof(bool);
                        break;
                    }
                    case CONFIG_TYPE_STRING: {
                        if (!json_parse_string(ctx, value.str,
                                               sizeof(value.str))) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        value_size = strlen(value.str) + 1;
                        break;
                    }
                    case CONFIG_TYPE_BLOB: {
                        char hex_str[CONFIG_MAX_MAX_VALUE_SIZE * 2 + 1];
                        if (!json_parse_string(ctx, hex_str, sizeof(hex_str))) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        if (!hex_decode(hex_str, strlen(hex_str), value.blob,
                                        sizeof(value.blob), &value_size)) {
                            return CONFIG_ERROR_INVALID_FORMAT;
                        }
                        break;
                    }
                    default:
                        return CONFIG_ERROR_INVALID_FORMAT;
                }
            }
            has_value = true;
        } else if (strcmp(field_name, "encrypted") == 0) {
            bool encrypted;
            if (!json_parse_bool(ctx, &encrypted)) {
                return CONFIG_ERROR_INVALID_FORMAT;
            }
            if (encrypted) {
                flags |= CONFIG_FLAG_ENCRYPTED;
            }
        } else {
            /* Unknown field - skip value */
            json_skip_whitespace(ctx);
            if (ctx->pos >= ctx->size) {
                return CONFIG_ERROR_INVALID_FORMAT;
            }

            char c = ctx->data[ctx->pos];
            if (c == '"') {
                char skip_buf[256];
                json_parse_string(ctx, skip_buf, sizeof(skip_buf));
            } else if (c == 't' || c == 'f') {
                bool skip_bool;
                json_parse_bool(ctx, &skip_bool);
            } else if (c == '-' || isdigit((unsigned char)c)) {
                int64_t skip_num;
                json_parse_int64(ctx, &skip_num);
            } else if (c == '{') {
                /* Skip nested object - simplified */
                int depth = 1;
                ctx->pos++;
                while (ctx->pos < ctx->size && depth > 0) {
                    if (ctx->data[ctx->pos] == '{')
                        depth++;
                    else if (ctx->data[ctx->pos] == '}')
                        depth--;
                    ctx->pos++;
                }
            } else if (c == '[') {
                /* Skip array - simplified */
                int depth = 1;
                ctx->pos++;
                while (ctx->pos < ctx->size && depth > 0) {
                    if (ctx->data[ctx->pos] == '[')
                        depth++;
                    else if (ctx->data[ctx->pos] == ']')
                        depth--;
                    ctx->pos++;
                }
            } else if (c == 'n' && ctx->pos + 4 <= ctx->size &&
                       strncmp(ctx->data + ctx->pos, "null", 4) == 0) {
                ctx->pos += 4;
            }
        }
    }

    /* Validate we have required fields */
    if (!has_value) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Store the value */
    config_status_t status;
    switch (type) {
        case CONFIG_TYPE_I32:
            status = config_store_set(key, type, &value.i32, value_size, flags,
                                      ctx->namespace_id);
            break;
        case CONFIG_TYPE_U32:
            status = config_store_set(key, type, &value.u32, value_size, flags,
                                      ctx->namespace_id);
            break;
        case CONFIG_TYPE_I64:
            status = config_store_set(key, type, &value.i64, value_size, flags,
                                      ctx->namespace_id);
            break;
        case CONFIG_TYPE_FLOAT:
            status = config_store_set(key, type, &value.f, value_size, flags,
                                      ctx->namespace_id);
            break;
        case CONFIG_TYPE_BOOL:
            status = config_store_set(key, type, &value.b, value_size, flags,
                                      ctx->namespace_id);
            break;
        case CONFIG_TYPE_STRING:
            status = config_store_set(key, type, value.str, value_size, flags,
                                      ctx->namespace_id);
            break;
        case CONFIG_TYPE_BLOB:
            status = config_store_set(key, type, value.blob, value_size, flags,
                                      ctx->namespace_id);
            break;
        default:
            status = CONFIG_ERROR_INVALID_FORMAT;
            break;
    }

    return status;
}

/**
 * \brief           Import JSON data
 * \param[in]       data: JSON data
 * \param[in]       size: Data size
 * \param[in]       flags: Import flags
 * \param[in]       namespace_id: Target namespace ID
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t import_json(const void* data, size_t size,
                                   config_import_flags_t flags,
                                   uint8_t namespace_id) {
    json_parser_ctx_t ctx = {.data = (const char*)data,
                             .size = size,
                             .pos = 0,
                             .flags = flags,
                             .namespace_id = namespace_id,
                             .status = CONFIG_OK};

    /* Expect opening brace */
    if (!json_expect_char(&ctx, '{')) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Parse entries */
    bool first_entry = true;
    bool found_closing_brace = false;
    while (ctx.pos < ctx.size) {
        json_skip_whitespace(&ctx);

        /* Check for closing brace */
        if (ctx.pos < ctx.size && ctx.data[ctx.pos] == '}') {
            ctx.pos++;
            found_closing_brace = true;
            break;
        }

        /* Expect comma between entries */
        if (!first_entry) {
            if (!json_expect_char(&ctx, ',')) {
                return CONFIG_ERROR_INVALID_FORMAT;
            }
        }
        first_entry = false;

        /* Parse key */
        char key[CONFIG_MAX_MAX_KEY_LEN];
        if (!json_parse_string(&ctx, key, sizeof(key))) {
            return CONFIG_ERROR_INVALID_FORMAT;
        }

        /* Expect colon */
        if (!json_expect_char(&ctx, ':')) {
            return CONFIG_ERROR_INVALID_FORMAT;
        }

        /* Parse entry value */
        config_status_t status = json_import_entry(&ctx, key);
        if (status != CONFIG_OK) {
            if (flags & CONFIG_IMPORT_FLAG_SKIP_ERRORS) {
                /* Skip this entry and continue */
                continue;
            }
            return status;
        }
    }

    /* Validate that we found the closing brace */
    if (!found_closing_brace) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Binary Import Implementation                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Import binary data
 * \param[in]       data: Binary data
 * \param[in]       size: Data size
 * \param[in]       flags: Import flags
 * \param[in]       namespace_id: Target namespace ID
 * \return          CONFIG_OK on success, error code otherwise
 */
static config_status_t import_binary(const void* data, size_t size,
                                     config_import_flags_t flags,
                                     uint8_t namespace_id) {
    const uint8_t* ptr = (const uint8_t*)data;
    size_t offset = 0;

    /* Validate minimum size for header */
    if (size < sizeof(config_binary_header_t)) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Read and validate header */
    config_binary_header_t header;
    memcpy(&header, ptr, sizeof(header));
    offset += sizeof(header);

    if (header.magic != CONFIG_BINARY_MAGIC) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    if (header.version != CONFIG_BINARY_VERSION) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Validate data size */
    if (size < sizeof(header) + header.data_size) {
        return CONFIG_ERROR_INVALID_FORMAT;
    }

    /* Read entries */
    for (uint32_t i = 0; i < header.entry_count; ++i) {
        /* Check remaining size for entry header */
        if (offset + sizeof(config_binary_entry_header_t) > size) {
            return CONFIG_ERROR_INVALID_FORMAT;
        }

        /* Read entry header */
        config_binary_entry_header_t entry_header;
        memcpy(&entry_header, ptr + offset, sizeof(entry_header));
        offset += sizeof(entry_header);

        /* Validate entry data size */
        size_t entry_data_size =
            (size_t)entry_header.key_len + (size_t)entry_header.value_size;
        if (offset + entry_data_size > size) {
            return CONFIG_ERROR_INVALID_FORMAT;
        }

        /* Read key */
        char key[CONFIG_MAX_MAX_KEY_LEN];
        if (entry_header.key_len >= sizeof(key)) {
            if (flags & CONFIG_IMPORT_FLAG_SKIP_ERRORS) {
                offset += entry_data_size;
                continue;
            }
            return CONFIG_ERROR_KEY_TOO_LONG;
        }
        memcpy(key, ptr + offset, entry_header.key_len);
        key[entry_header.key_len] = '\0';
        offset += entry_header.key_len;

        /* Read value */
        uint8_t value_buf[CONFIG_MAX_MAX_VALUE_SIZE];
        if (entry_header.value_size > sizeof(value_buf)) {
            if (flags & CONFIG_IMPORT_FLAG_SKIP_ERRORS) {
                offset += entry_header.value_size;
                continue;
            }
            return CONFIG_ERROR_VALUE_TOO_LARGE;
        }
        memcpy(value_buf, ptr + offset, entry_header.value_size);
        offset += entry_header.value_size;

        /* Store the value */
        config_status_t status = config_store_set(
            key, (config_type_t)entry_header.type, value_buf,
            entry_header.value_size, entry_header.flags, namespace_id);

        if (status != CONFIG_OK) {
            if (flags & CONFIG_IMPORT_FLAG_SKIP_ERRORS) {
                continue;
            }
            return status;
        }
    }

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

config_status_t config_import(config_format_t format,
                              config_import_flags_t flags, const void* data,
                              size_t size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (data == NULL || size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Clear existing data if requested */
    if (flags & CONFIG_IMPORT_FLAG_CLEAR) {
        config_status_t status = config_store_clear_all();
        if (status != CONFIG_OK) {
            return status;
        }
    }

    /* Import based on format */
    if (format == CONFIG_FORMAT_JSON) {
        return import_json(data, size, flags, CONFIG_DEFAULT_NAMESPACE_ID);
    } else if (format == CONFIG_FORMAT_BINARY) {
        return import_binary(data, size, flags, CONFIG_DEFAULT_NAMESPACE_ID);
    }

    return CONFIG_ERROR_INVALID_PARAM;
}

config_status_t config_import_namespace(const char* ns_name,
                                        config_format_t format,
                                        config_import_flags_t flags,
                                        const void* data, size_t size) {
    if (!config_is_initialized()) {
        return CONFIG_ERROR_NOT_INIT;
    }

    if (ns_name == NULL || data == NULL || size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get or create namespace */
    uint8_t ns_id;
    config_status_t status = config_namespace_create(ns_name, &ns_id);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Clear namespace if requested */
    if (flags & CONFIG_IMPORT_FLAG_CLEAR) {
        status = config_store_clear_namespace(ns_id);
        if (status != CONFIG_OK) {
            return status;
        }
    }

    /* Import based on format */
    if (format == CONFIG_FORMAT_JSON) {
        return import_json(data, size, flags, ns_id);
    } else if (format == CONFIG_FORMAT_BINARY) {
        return import_binary(data, size, flags, ns_id);
    }

    return CONFIG_ERROR_INVALID_PARAM;
}
