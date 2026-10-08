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
#include "config_wire.h"
#include "config_crypto.h"
#include "config_backend_internal.h"
#include "security/crypto.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Binary Format Constants (must match config_export.c)                      */
/*---------------------------------------------------------------------------*/

/* Import is a management transaction: parse/authenticate first, then one RAM
 * swap and optional snapshot commit. Heap use is bounded by store limits. */
typedef struct {
    config_store_replacement_t* items;
    config_store_replacement_t* original;
    bool imported[CONFIG_MAX_MAX_KEYS];
    uint8_t* new_values[CONFIG_MAX_MAX_KEYS];
    size_t count, original_count, collected, new_count;
    bool clear_all, namespace_only;
    int clear_namespace;
    config_status_t status;
} import_transaction_t;

static bool collect_original(const config_store_entry_info_t* info, void* context) {
    import_transaction_t* tx = (import_transaction_t*)context;
    config_store_replacement_t* old = &tx->original[tx->collected++];
    old->info = *info; old->size = info->value_size;
    uint8_t* copy = (uint8_t*)malloc(old->size ? old->size : 1);
    if (!copy) { tx->status = CONFIG_ERROR_NO_MEMORY; return false; }
    old->value = copy;
    tx->status = config_store_get(info->key, NULL, copy, &old->size, NULL, info->namespace_id);
    if (tx->status != CONFIG_OK) return false;
    bool removed = tx->clear_all || tx->clear_namespace == info->namespace_id;
    if (removed && (info->flags & CONFIG_FLAG_READONLY)) {
        tx->status = CONFIG_ERROR_READ_ONLY; return false;
    }
    if (!removed) tx->items[tx->count++] = *old;
    return true;
}

static void destroy_transaction(import_transaction_t* tx) {
    for (size_t i = 0; i < tx->collected; ++i) {
        if (tx->original[i].value) {
            nx_crypto_secure_zero((void*)tx->original[i].value, tx->original[i].size);
            free((void*)tx->original[i].value);
        }
    }
    for (size_t i = 0; i < tx->new_count; ++i) {
        /* Data size is bounded by the record limit. Erase its allocated extent. */
        for (size_t j = 0; j < tx->count; ++j) {
            if (tx->items[j].value == tx->new_values[i]) {
                nx_crypto_secure_zero(tx->new_values[i], tx->items[j].size); break;
            }
        }
        free(tx->new_values[i]);
    }
    free(tx->items); free(tx->original);
}

static config_status_t stage_entry(import_transaction_t* tx, const char* key,
    config_type_t type, const void* value, size_t size, uint8_t flags, uint8_t ns) {
    if (!key[0] || strlen(key) >= CONFIG_MAX_MAX_KEY_LEN || size > CONFIG_MAX_MAX_VALUE_SIZE ||
        (unsigned)type > CONFIG_TYPE_BLOB ||
        (flags & ~(CONFIG_FLAG_ENCRYPTED | CONFIG_FLAG_READONLY | CONFIG_FLAG_PERSISTENT)))
        return CONFIG_ERROR_INVALID_FORMAT;
    if (!(flags & CONFIG_FLAG_ENCRYPTED) && type == CONFIG_TYPE_STRING &&
        (!size || ((const uint8_t*)value)[size - 1] != 0 || memchr(value, 0, size - 1)))
        return CONFIG_ERROR_INVALID_FORMAT;
    /* CLEAR changes which values survive the transaction, not permission to
     * remove a surviving key's security policy. Check the original identity
     * even when collect_original omitted it from the replacement batch. */
    for (size_t i = 0; i < tx->original_count; ++i) {
        const config_store_replacement_t* old = &tx->original[i];
        if (old->info.namespace_id != ns || strcmp(old->info.key, key)) continue;
        if (old->info.flags & CONFIG_FLAG_READONLY) return CONFIG_ERROR_READ_ONLY;
        if ((old->info.flags & CONFIG_FLAG_ENCRYPTED) && !(flags & CONFIG_FLAG_ENCRYPTED))
            return CONFIG_ERROR_CRYPTO_FAILED;
        flags |= old->info.flags & CONFIG_FLAG_PERSISTENT;
        break;
    }
    size_t index = 0;
    while (index < tx->count && (tx->items[index].info.namespace_id != ns ||
           strcmp(tx->items[index].info.key, key))) ++index;
    if (index < tx->count) {
        if (tx->imported[index]) return CONFIG_ERROR_INVALID_FORMAT;
        uint8_t old_flags = tx->items[index].info.flags;
        if (old_flags & CONFIG_FLAG_READONLY) return CONFIG_ERROR_READ_ONLY;
        if ((old_flags & CONFIG_FLAG_ENCRYPTED) && !(flags & CONFIG_FLAG_ENCRYPTED))
            return CONFIG_ERROR_CRYPTO_FAILED;
        flags |= old_flags & CONFIG_FLAG_PERSISTENT;
    } else if (tx->count >= CONFIG_MAX_MAX_KEYS) return CONFIG_ERROR_NO_SPACE;
    if (flags & CONFIG_FLAG_ENCRYPTED) {
        uint8_t plaintext[CONFIG_MAX_MAX_VALUE_SIZE];
        size_t plaintext_size = sizeof(plaintext);
        config_status_t status = config_crypto_decrypt_record((const uint8_t*)value,
            size, plaintext, &plaintext_size, key, ns, type);
        if (status == CONFIG_OK && type == CONFIG_TYPE_STRING &&
            (!plaintext_size || plaintext[plaintext_size - 1] != 0 ||
             memchr(plaintext, 0, plaintext_size - 1))) status = CONFIG_ERROR_INVALID_FORMAT;
        nx_crypto_secure_zero(plaintext, sizeof(plaintext));
        if (status != CONFIG_OK) return status;
    }
    uint8_t* copy = (uint8_t*)malloc(size ? size : 1);
    if (!copy) return CONFIG_ERROR_NO_MEMORY;
    if (size) memcpy(copy, value, size);
    config_store_replacement_t* item = &tx->items[index];
    config_store_replacement_t previous = *item;
    memset(item, 0, sizeof(*item));
    memcpy(item->info.key, key, strlen(key) + 1);
    item->info.namespace_id = ns; item->info.type = type;
    item->info.flags = flags; item->info.value_size = (uint16_t)size;
    item->value = copy; item->size = size;
    bool appended = index == tx->count;
    if (appended) ++tx->count;
    config_status_t valid = config_store_replace_all(tx->items, tx->count, false);
    if (valid != CONFIG_OK) {
        *item = previous;
        if (appended) --tx->count;
        nx_crypto_secure_zero(copy, size); free(copy);
        return valid;
    }
    tx->new_values[tx->new_count++] = copy;
    tx->imported[index] = true;
    return CONFIG_OK;
}

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
    import_transaction_t* transaction;
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
static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool parse_hex4(json_parser_ctx_t* ctx, uint32_t* code) {
    if (ctx->size - ctx->pos < 4) return false;
    *code = 0;
    for (size_t i = 0; i < 4; ++i) {
        int nibble = hex_digit(ctx->data[ctx->pos++]);
        if (nibble < 0) return false;
        *code = (*code << 4) | (uint32_t)nibble;
    }
    return true;
}
static bool json_parse_string(json_parser_ctx_t* ctx, char* out, size_t out_size) {
    if (!out || !out_size || !json_expect_char(ctx, '"')) return false;
    size_t written = 0;
    while (ctx->pos < ctx->size) {
        uint32_t c = (unsigned char)ctx->data[ctx->pos++];
        if (c == '"') {
            out[written] = 0;
            return config_utf8_valid((const uint8_t*)out, written);
        }
        if (c < 0x20) return false;
        if (c == '\\') {
            if (ctx->pos == ctx->size) return false;
            char escaped = ctx->data[ctx->pos++];
            switch (escaped) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'u': {
                    if (!parse_hex4(ctx, &c) || c == 0) return false;
                    if (c >= 0xd800 && c <= 0xdbff) {
                        uint32_t low;
                        if (ctx->size - ctx->pos < 6 || ctx->data[ctx->pos++] != '\\' ||
                            ctx->data[ctx->pos++] != 'u' || !parse_hex4(ctx, &low) ||
                            low < 0xdc00 || low > 0xdfff) return false;
                        c = 0x10000u + ((c - 0xd800u) << 10) + low - 0xdc00u;
                    } else if (c >= 0xdc00 && c <= 0xdfff) return false;
                    unsigned char bytes[4]; size_t count;
                    if (c < 0x80) { bytes[0] = (unsigned char)c; count = 1; }
                    else if (c < 0x800) {
                        bytes[0] = (unsigned char)(0xc0 | (c >> 6));
                        bytes[1] = (unsigned char)(0x80 | (c & 63)); count = 2;
                    } else if (c < 0x10000) {
                        bytes[0] = (unsigned char)(0xe0 | (c >> 12));
                        bytes[1] = (unsigned char)(0x80 | ((c >> 6) & 63));
                        bytes[2] = (unsigned char)(0x80 | (c & 63)); count = 3;
                    } else {
                        bytes[0] = (unsigned char)(0xf0 | (c >> 18));
                        bytes[1] = (unsigned char)(0x80 | ((c >> 12) & 63));
                        bytes[2] = (unsigned char)(0x80 | ((c >> 6) & 63));
                        bytes[3] = (unsigned char)(0x80 | (c & 63)); count = 4;
                    }
                    if (count >= out_size - written) return false;
                    memcpy(out + written, bytes, count); written += count;
                    continue;
                }
                default: return false;
            }
        }
        if (written >= out_size - 1) return false;
        out[written++] = (char)c;
    }
    return false;
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
    errno = 0; char* end;
    long long parsed = strtoll(num_buf, &end, 10);
    if (errno == ERANGE || *end) return false;
    *value = (int64_t)parsed;
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
    errno = 0; char* end;
    double parsed = strtod(num_buf, &end);
    if (errno == ERANGE || *end || !isfinite(parsed)) return false;
    *value = (float)parsed;
    return isfinite(*value);
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
static bool valid_number(const char* text) {
    size_t i = text[0] == '-' ? 1 : 0;
    if (text[i] == '0') ++i;
    else {
        if (text[i] < '1' || text[i] > '9') return false;
        while (isdigit((unsigned char)text[i])) ++i;
    }
    if (text[i] == '.') {
        ++i; if (!isdigit((unsigned char)text[i])) return false;
        while (isdigit((unsigned char)text[i])) ++i;
    }
    if (text[i] == 'e' || text[i] == 'E') {
        ++i; if (text[i] == '+' || text[i] == '-') ++i;
        if (!isdigit((unsigned char)text[i])) return false;
        while (isdigit((unsigned char)text[i])) ++i;
    }
    return text[i] == 0;
}
static config_status_t json_import_entry(json_parser_ctx_t* ctx, const char* key) {
    enum { TEXT, NUMBER, BOOLEAN } kind = TEXT;
    char text[CONFIG_MAX_MAX_VALUE_SIZE * 2 + 1] = {0};
    char number[64] = {0};
    int type = -1;
    bool has_type = false, has_value = false, has_encrypted = false;
    bool boolean = false, encrypted = false, invalid = false, first = true, closed = false;
    /* SKIP_ERRORS applies only after a complete syntactically valid entry.
     * Unconsumed or malformed JSON must never authorize CLEAR or partial data. */
    ctx->status = CONFIG_ERROR_INVALID_FORMAT;
    if (!json_expect_char(ctx, '{')) return CONFIG_ERROR_INVALID_FORMAT;
    while (ctx->pos < ctx->size) {
        json_skip_whitespace(ctx);
        if (ctx->pos < ctx->size && ctx->data[ctx->pos] == '}') {
            ++ctx->pos; closed = true; break;
        }
        if (!first && !json_expect_char(ctx, ',')) return CONFIG_ERROR_INVALID_FORMAT;
        first = false;
        char field[32];
        if (!json_parse_string(ctx, field, sizeof(field)) || !json_expect_char(ctx, ':'))
            return CONFIG_ERROR_INVALID_FORMAT;
        if (!strcmp(field, "type")) {
            char name[16];
            if (!json_parse_string(ctx, name, sizeof(name))) return CONFIG_ERROR_INVALID_FORMAT;
            if (has_type) invalid = true;
            has_type = true; type = get_type_from_name(name);
        } else if (!strcmp(field, "encrypted")) {
            if (has_encrypted) invalid = true;
            has_encrypted = true;
            if (!json_parse_bool(ctx, &encrypted)) return CONFIG_ERROR_INVALID_FORMAT;
        } else if (!strcmp(field, "value")) {
            if (has_value) invalid = true;
            has_value = true; json_skip_whitespace(ctx);
            if (ctx->pos >= ctx->size) return CONFIG_ERROR_INVALID_FORMAT;
            char c = ctx->data[ctx->pos];
            if (c == '"') {
                kind = TEXT;
                if (!json_parse_string(ctx, text, sizeof(text))) return CONFIG_ERROR_INVALID_FORMAT;
            } else if (c == 't' || c == 'f') {
                kind = BOOLEAN;
                if (!json_parse_bool(ctx, &boolean)) return CONFIG_ERROR_INVALID_FORMAT;
            } else {
                kind = NUMBER; size_t length = 0;
                while (ctx->pos < ctx->size) {
                    c = ctx->data[ctx->pos];
                    if (!isdigit((unsigned char)c) && c != '-' && c != '+' && c != '.' && c != 'e' && c != 'E') break;
                    if (length < sizeof(number) - 1) number[length++] = c; else invalid = true;
                    ++ctx->pos;
                }
                number[length] = 0;
                if (!valid_number(number)) return CONFIG_ERROR_INVALID_FORMAT;
            }
        } else {
            /* This schema has no untyped extension fields; reject them explicitly. */
            return CONFIG_ERROR_INVALID_FORMAT;
        }
    }
    if (!closed) return CONFIG_ERROR_INVALID_FORMAT;
    ctx->status = CONFIG_OK;
    if (!has_value || invalid || (has_type && type < 0)) return CONFIG_ERROR_INVALID_FORMAT;
    if (!has_type) type = kind == TEXT ? CONFIG_TYPE_STRING : kind == BOOLEAN ? CONFIG_TYPE_BOOL :
        (strpbrk(number, ".eE") ? CONFIG_TYPE_FLOAT : CONFIG_TYPE_I32);
    union { int32_t i32; uint32_t u32; int64_t i64; float f; bool b;
            uint8_t data[CONFIG_MAX_MAX_VALUE_SIZE]; } value;
    size_t size = 0;
    if (encrypted || type == CONFIG_TYPE_BLOB) {
        if (kind != TEXT || !hex_decode(text, strlen(text), value.data, sizeof(value.data), &size))
            return CONFIG_ERROR_INVALID_FORMAT;
    } else if (type == CONFIG_TYPE_STRING) {
        if (kind != TEXT) return CONFIG_ERROR_INVALID_FORMAT;
        size = strlen(text) + 1;
        if (size > sizeof(value.data)) return CONFIG_ERROR_VALUE_TOO_LARGE;
        memcpy(value.data, text, size);
    } else if (type == CONFIG_TYPE_BOOL) {
        if (kind != BOOLEAN) return CONFIG_ERROR_INVALID_FORMAT;
        value.b = boolean; size = sizeof(value.b);
    } else {
        if (kind != NUMBER) return CONFIG_ERROR_INVALID_FORMAT;
        json_parser_ctx_t number_ctx = {.data = number, .size = strlen(number), .pos = 0};
        if (type == CONFIG_TYPE_FLOAT) {
            if (!json_parse_float(&number_ctx, &value.f)) return CONFIG_ERROR_INVALID_FORMAT;
            size = sizeof(value.f);
        } else {
            int64_t integer;
            if (!json_parse_int64(&number_ctx, &integer) || number_ctx.pos != number_ctx.size)
                return CONFIG_ERROR_INVALID_FORMAT;
            if (type == CONFIG_TYPE_I32) {
                if (integer < INT32_MIN || integer > INT32_MAX) return CONFIG_ERROR_INVALID_FORMAT;
                value.i32 = (int32_t)integer; size = sizeof(value.i32);
            } else if (type == CONFIG_TYPE_U32) {
                if (integer < 0 || (uint64_t)integer > UINT32_MAX) return CONFIG_ERROR_INVALID_FORMAT;
                value.u32 = (uint32_t)integer; size = sizeof(value.u32);
            } else if (type == CONFIG_TYPE_I64) { value.i64 = integer; size = sizeof(value.i64); }
            else return CONFIG_ERROR_INVALID_FORMAT;
        }
    }
    config_status_t result = stage_entry(ctx->transaction, key, (config_type_t)type, &value,
        size, encrypted ? CONFIG_FLAG_ENCRYPTED : CONFIG_FLAG_NONE, ctx->namespace_id);
    nx_crypto_secure_zero(&value, sizeof(value));
    nx_crypto_secure_zero(text, sizeof(text));
    return result;
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
                                   uint8_t namespace_id, import_transaction_t* transaction) {
    json_parser_ctx_t ctx = {.data = (const char*)data,
                             .size = size,
                             .pos = 0,
                             .flags = flags,
                             .namespace_id = namespace_id,
                             .status = CONFIG_OK,
                             .transaction = transaction};

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
            if (ctx.status != CONFIG_OK) return ctx.status;
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

    json_skip_whitespace(&ctx);
    if (ctx.pos < ctx.size && ctx.data[ctx.pos] == '\0') ++ctx.pos;
    if (ctx.pos != ctx.size) return CONFIG_ERROR_INVALID_FORMAT;
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
    config_import_flags_t flags, uint8_t namespace_id, import_transaction_t* tx) {
    const uint8_t* bytes = (const uint8_t*)data;
    if (size < CONFIG_BINARY_HEADER_SIZE || config_wire_get(bytes, 4) != CONFIG_BINARY_MAGIC ||
        bytes[4] != CONFIG_BINARY_VERSION || bytes[5] || bytes[6] || bytes[7] ||
        config_wire_get(bytes + 12, 4) != size - CONFIG_BINARY_HEADER_SIZE) return CONFIG_ERROR_INVALID_FORMAT;
    size_t count = (size_t)config_wire_get(bytes + 8, 4), position = CONFIG_BINARY_HEADER_SIZE;
    if (count > CONFIG_MAX_MAX_KEYS) return CONFIG_ERROR_INVALID_FORMAT;
    for (size_t i = 0; i < count; ++i) {
        if (position > size || size - position < CONFIG_BINARY_ENTRY_SIZE) return CONFIG_ERROR_INVALID_FORMAT;
        const uint8_t* entry = bytes + position;
        size_t key_size = entry[0], value_size = (size_t)config_wire_get(entry + 4, 2);
        config_type_t type = (config_type_t)entry[1]; uint8_t entry_flags = entry[2];
        uint8_t ns = tx->namespace_only ? namespace_id : entry[3];
        position += CONFIG_BINARY_ENTRY_SIZE;
        if (key_size > size - position || value_size > size - position - key_size)
            return CONFIG_ERROR_INVALID_FORMAT;
        config_status_t status = CONFIG_OK;
        char key[CONFIG_MAX_MAX_KEY_LEN]; uint8_t value[CONFIG_MAX_MAX_VALUE_SIZE];
        if (!key_size || key_size >= sizeof(key) || memchr(bytes + position, 0, key_size) ||
            value_size > sizeof(value) || !config_namespace_is_valid_id(ns)) status = CONFIG_ERROR_INVALID_FORMAT;
        if (status == CONFIG_OK) {
            memcpy(key, bytes + position, key_size); key[key_size] = 0;
            if (!config_wire_value(type, entry_flags, bytes + position + key_size,
                                   value_size, value, false)) status = CONFIG_ERROR_INVALID_FORMAT;
            else status = stage_entry(tx, key, type, value, value_size, entry_flags, ns);
            nx_crypto_secure_zero(value, sizeof(value));
        }
        position += key_size + value_size;
        if (status != CONFIG_OK && !(flags & CONFIG_IMPORT_FLAG_SKIP_ERRORS)) return status;
    }
    return position == size ? CONFIG_OK : CONFIG_ERROR_INVALID_FORMAT;
}

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

static config_status_t import_transaction(config_format_t format,
    config_import_flags_t flags, const void* data, size_t size, uint8_t ns, bool namespace_only) {
    if ((flags & ~(CONFIG_IMPORT_FLAG_CLEAR | CONFIG_IMPORT_FLAG_SKIP_ERRORS)) ||
        (format != CONFIG_FORMAT_JSON && format != CONFIG_FORMAT_BINARY)) return CONFIG_ERROR_INVALID_PARAM;
    import_transaction_t tx = {0}; tx.clear_namespace = -1; tx.namespace_only = namespace_only;
    if (flags & CONFIG_IMPORT_FLAG_CLEAR) {
        if (namespace_only) tx.clear_namespace = ns; else tx.clear_all = true;
    }
    config_status_t status = config_store_get_count(&tx.original_count);
    if (status != CONFIG_OK) return status;
    tx.original = (config_store_replacement_t*)calloc(tx.original_count ? tx.original_count : 1,
        sizeof(*tx.original));
    tx.items = (config_store_replacement_t*)calloc(CONFIG_MAX_MAX_KEYS, sizeof(*tx.items));
    if (!tx.original || !tx.items) { free(tx.original); free(tx.items); return CONFIG_ERROR_NO_MEMORY; }
    status = config_store_iterate(collect_original, &tx);
    if (status == CONFIG_OK) status = tx.status;
    if (status == CONFIG_OK) {
        status = format == CONFIG_FORMAT_JSON ? import_json(data, size, flags, ns, &tx) :
                                                import_binary(data, size, flags, ns, &tx);
    }
    if (status == CONFIG_OK) status = config_store_replace_all(tx.items, tx.count, true);
    if (status == CONFIG_OK) {
        config_backend_set_dirty(true);
        status = config_backend_auto_commit_if_enabled();
        if (status != CONFIG_OK) {
            config_status_t rollback = config_store_replace_all(tx.original, tx.original_count, true);
            config_backend_set_dirty(true);
            if (rollback != CONFIG_OK) status = rollback;
        }
    }
    destroy_transaction(&tx);
    return status;
}
config_status_t config_import(config_format_t format,
    config_import_flags_t flags, const void* data, size_t size) {
    if (!config_is_initialized()) return CONFIG_ERROR_NOT_INIT;
    if (!data || !size) return CONFIG_ERROR_INVALID_PARAM;
    return import_transaction(format, flags, data, size, CONFIG_DEFAULT_NAMESPACE_ID, false);
}
config_status_t config_import_namespace(const char* name, config_format_t format,
    config_import_flags_t flags, const void* data, size_t size) {
    if (!config_is_initialized()) return CONFIG_ERROR_NOT_INIT;
    if (!name || !data || !size) return CONFIG_ERROR_INVALID_PARAM;
    uint8_t ns;
    bool existing = config_namespace_get_id(name, &ns) == CONFIG_OK;
    config_status_t status = config_namespace_create(name, &ns);
    if (status != CONFIG_OK) return status;
    status = import_transaction(format, flags, data, size, ns, true);
    if (status != CONFIG_OK && !existing) (void)config_erase_namespace(name);
    return status;
}
