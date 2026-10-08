#include "config_backend_internal.h"
#include "config/config.h"
#include "config_namespace.h"
#include "config_store.h"
#include "config_crypto.h"
#include "security/crypto.h"
#include <string.h>
#include <stdatomic.h>

static const config_backend_t* g_backend;
static bool g_initialized, g_auto_commit, g_dirty;
static uint8_t g_default_buffer[CONFIG_PERSISTENCE_BUFFER_SIZE];
static uint8_t* g_buffer = g_default_buffer;
static size_t g_capacity = sizeof(g_default_buffer);
/* Management-owner workspace: avoids a 25 KiB temporary on MCU task stacks.
 * Product profiles bound CONFIG_MAX_MAX_KEYS to their static memory budget. */
static config_store_replacement_t g_import_items[CONFIG_MAX_MAX_KEYS];
static atomic_flag g_workspace_busy = ATOMIC_FLAG_INIT;
static void put16(uint8_t* p, size_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
static size_t get16(const uint8_t* p) { return p[0] | ((size_t)p[1] << 8); }
static void scalar_to_wire(config_type_t type, uint8_t* value, size_t size) {
    if ((type == CONFIG_TYPE_I32 || type == CONFIG_TYPE_U32 ||
         type == CONFIG_TYPE_FLOAT) && size == 4) {
        uint32_t scalar; memcpy(&scalar, value, 4);
        for (unsigned i = 0; i < 4; ++i) value[i] = (uint8_t)(scalar >> (8u * i));
    } else if (type == CONFIG_TYPE_I64 && size == 8) {
        uint64_t scalar; memcpy(&scalar, value, 8);
        for (unsigned i = 0; i < 8; ++i) value[i] = (uint8_t)(scalar >> (8u * i));
    }
}
static void scalar_from_wire(config_type_t type, uint8_t* value, size_t size) {
    if ((type == CONFIG_TYPE_I32 || type == CONFIG_TYPE_U32 ||
         type == CONFIG_TYPE_FLOAT) && size == 4) {
        uint32_t scalar = 0;
        for (unsigned i = 0; i < 4; ++i) scalar |= (uint32_t)value[i] << (8u * i);
        memcpy(value, &scalar, 4);
    } else if (type == CONFIG_TYPE_I64 && size == 8) {
        uint64_t scalar = 0;
        for (unsigned i = 0; i < 8; ++i) scalar |= (uint64_t)value[i] << (8u * i);
        memcpy(value, &scalar, 8);
    }
}
typedef struct { size_t pos, count; config_status_t status; } save_ctx_t;
static bool save_entry(const config_store_entry_info_t* info, void* ctx_ptr) {
    save_ctx_t* ctx = ctx_ptr;
    size_t keylen = strlen(info->key), value_size = info->value_size;
    if (keylen > UINT8_MAX || value_size > UINT16_MAX ||
        ctx->pos > g_capacity || 6 + keylen + value_size > g_capacity - ctx->pos) {
        ctx->status = CONFIG_ERROR_NO_SPACE; return false;
    }
    uint8_t* out = g_buffer + ctx->pos;
    out[0] = info->namespace_id; out[1] = (uint8_t)info->type;
    out[2] = info->flags; out[3] = (uint8_t)keylen;
    put16(out + 4, value_size);
    memcpy(out + 6, info->key, keylen);
    ctx->status = config_store_get(info->key, NULL, out + 6 + keylen,
                                   &value_size, NULL, info->namespace_id);
    if (ctx->status != CONFIG_OK) return false;
    if (!(info->flags & CONFIG_FLAG_ENCRYPTED))
        scalar_to_wire(info->type, out + 6 + keylen, value_size);
    ctx->pos += 6 + keylen + value_size; ++ctx->count;
    return true;
}
config_status_t config_backend_set_snapshot_buffer(void* buffer, size_t size) {
    if (config_is_initialized()) return CONFIG_ERROR_ALREADY_INIT;
    if (buffer && size < 8) return CONFIG_ERROR_INVALID_PARAM;
    g_buffer = buffer ? buffer : g_default_buffer;
    g_capacity = buffer ? size : sizeof(g_default_buffer);
    return CONFIG_OK;
}
static config_status_t set_impl(const config_backend_t* backend) {
    if (!backend || ((!backend->read || !backend->write || !backend->erase) &&
                     (!backend->save_snapshot || !backend->load_snapshot)))
        return CONFIG_ERROR_INVALID_PARAM;
    if (g_backend && g_initialized && g_backend->deinit) {
        config_status_t status = g_backend->deinit(g_backend->ctx);
        if (status != CONFIG_OK) return status;
    }
    g_backend = NULL; g_initialized = false;
    if (backend->init) {
        config_status_t status = backend->init(backend->ctx);
        if (status != CONFIG_OK) return status;
    }
    g_backend = backend; g_initialized = true;
    return CONFIG_OK;
}
const config_backend_t* config_backend_get(void) { return g_backend; }
bool config_backend_is_set(void) { return g_backend && g_initialized; }
static config_status_t commit_impl(void) {
    if (!g_backend) return CONFIG_ERROR_NO_BACKEND;
    if (!g_initialized || !config_store_is_initialized())
        return CONFIG_ERROR_NOT_INIT;
    if (!g_backend->save_snapshot) return CONFIG_ERROR_UNSUPPORTED;
    if (g_capacity < 8) return CONFIG_ERROR_NO_SPACE;
    memcpy(g_buffer, "NXCS", 4); g_buffer[4] = 1; g_buffer[5] = 0;
    save_ctx_t ctx = {8, 0, CONFIG_OK};
    for (uint8_t id = 0; id < CONFIG_DEFAULT_MAX_NAMESPACES; ++id) {
        if (!config_namespace_is_valid_id(id)) continue;
        char name[CONFIG_MAX_NS_NAME_LEN];
        config_status_t status = config_namespace_get_name(id, name, sizeof(name));
        if (status != CONFIG_OK) return status;
        size_t len = strlen(name);
        if (ctx.pos + 2 + len > g_capacity) return CONFIG_ERROR_NO_SPACE;
        g_buffer[ctx.pos++] = id; g_buffer[ctx.pos++] = (uint8_t)len;
        memcpy(g_buffer + ctx.pos, name, len); ctx.pos += len;
        ++g_buffer[5];
    }
    config_status_t status = config_store_iterate(save_entry, &ctx);
    if (status != CONFIG_OK) return status;
    if (ctx.status != CONFIG_OK) return ctx.status;
    put16(g_buffer + 6, ctx.count);
    status = g_backend->save_snapshot(g_backend->ctx, g_buffer, ctx.pos);
    if (status == CONFIG_OK) g_dirty = false;
    memset(g_buffer, 0, ctx.pos);
    return status;
}
static config_status_t load_impl(void) {
    if (!g_backend) return CONFIG_ERROR_NO_BACKEND;
    if (!g_initialized || !config_store_is_initialized())
        return CONFIG_ERROR_NOT_INIT;
    if (!g_backend->load_snapshot) return CONFIG_ERROR_UNSUPPORTED;
    size_t size = g_capacity;
    config_status_t status = g_backend->load_snapshot(g_backend->ctx, g_buffer,
                                                     &size);
    if (status == CONFIG_ERROR_NOT_FOUND) return CONFIG_OK;
    if (status != CONFIG_OK) return status;
    if (size < 8 || memcmp(g_buffer, "NXCS", 4) || g_buffer[4] != 1 ||
        !g_buffer[5] || g_buffer[5] > CONFIG_DEFAULT_MAX_NAMESPACES ||
        get16(g_buffer + 6) > CONFIG_MAX_MAX_KEYS)
        return CONFIG_ERROR_INVALID_FORMAT;
    config_namespace_snapshot_t ns[CONFIG_DEFAULT_MAX_NAMESPACES];
    config_store_replacement_t* items = g_import_items;
    size_t ns_count = g_buffer[5], count = get16(g_buffer + 6), pos = 8;
    memset(ns, 0, sizeof(ns)); memset(items, 0, sizeof(g_import_items));
    for (size_t i = 0; i < ns_count; ++i) {
        if (pos > size || size - pos < 2) return CONFIG_ERROR_INVALID_FORMAT;
        ns[i].id = g_buffer[pos++]; size_t len = g_buffer[pos++];
        if (!len || len >= CONFIG_MAX_NS_NAME_LEN || len > size - pos ||
            memchr(g_buffer + pos, 0, len)) return CONFIG_ERROR_INVALID_FORMAT;
        memcpy(ns[i].name, g_buffer + pos, len); pos += len;
    }
    for (size_t i = 0; i < count; ++i) {
        if (pos > size || size - pos < 6) return CONFIG_ERROR_INVALID_FORMAT;
        uint8_t* in = g_buffer + pos;
        items[i].info.namespace_id = in[0];
        items[i].info.type = (config_type_t)in[1]; items[i].info.flags = in[2];
        size_t keylen = in[3], valuelen = get16(in + 4); pos += 6;
        if (!keylen || keylen >= CONFIG_MAX_MAX_KEY_LEN ||
            keylen > size - pos || valuelen > size - pos - keylen ||
            memchr(g_buffer + pos, 0, keylen)) return CONFIG_ERROR_INVALID_FORMAT;
        bool found = false;
        for (size_t j = 0; j < ns_count; ++j)
            if (ns[j].id == items[i].info.namespace_id) found = true;
        if (!found) return CONFIG_ERROR_INVALID_FORMAT;
        memcpy(items[i].info.key, g_buffer + pos, keylen); pos += keylen;
        items[i].value = g_buffer + pos; items[i].size = valuelen;
        items[i].info.value_size = (uint16_t)valuelen; pos += valuelen;
        if (items[i].info.flags & CONFIG_FLAG_ENCRYPTED) {
            uint8_t plaintext[CONFIG_MAX_MAX_VALUE_SIZE];
            size_t plaintext_size = sizeof(plaintext);
            status = config_crypto_decrypt_record(items[i].value, items[i].size,
                plaintext, &plaintext_size, items[i].info.key,
                items[i].info.namespace_id, items[i].info.type);
            if (status == CONFIG_OK) {
                config_store_replacement_t decrypted = items[i];
                decrypted.info.flags &= (uint8_t)~CONFIG_FLAG_ENCRYPTED;
                decrypted.value = plaintext; decrypted.size = plaintext_size;
                status = config_store_replace_all(&decrypted, 1, false);
            }
            nx_crypto_secure_zero(plaintext, sizeof(plaintext));
            if (status != CONFIG_OK) return status;
        } else {
            scalar_from_wire(items[i].info.type, g_buffer + pos - valuelen,
                              valuelen);
        }
    }
    if (pos != size) return CONFIG_ERROR_INVALID_FORMAT;
    status = config_namespace_replace_all(ns, ns_count, false);
    if (status != CONFIG_OK) return status;
    status = config_store_replace_all(items, count, false);
    if (status != CONFIG_OK) return status;
    config_namespace_replace_all(ns, ns_count, true);
    config_store_replace_all(items, count, true);
    memset(g_buffer, 0, size); g_dirty = false;
    return CONFIG_OK;
}
config_status_t config_backend_commit(void) {
    if (atomic_flag_test_and_set_explicit(&g_workspace_busy, memory_order_acquire))
        return CONFIG_ERROR_BUSY;
    config_status_t status = commit_impl();
    nx_crypto_secure_zero(g_buffer, g_capacity);
    atomic_flag_clear_explicit(&g_workspace_busy, memory_order_release);
    return status;
}
config_status_t config_backend_load(void) {
    if (atomic_flag_test_and_set_explicit(&g_workspace_busy, memory_order_acquire))
        return CONFIG_ERROR_BUSY;
    config_status_t status = load_impl();
    nx_crypto_secure_zero(g_buffer, g_capacity);
    memset(g_import_items, 0, sizeof(g_import_items));
    atomic_flag_clear_explicit(&g_workspace_busy, memory_order_release);
    return status;
}
void config_backend_set_auto_commit(bool value) { g_auto_commit = value; }
bool config_backend_get_auto_commit(void) { return g_auto_commit; }
void config_backend_set_dirty(bool value) { g_dirty = value; }
bool config_backend_is_dirty(void) { return g_dirty; }
config_status_t config_backend_auto_commit_if_enabled(void) {
    return g_auto_commit && g_dirty && g_backend ? config_backend_commit() : CONFIG_OK;
}
static config_status_t deinit_impl(void) {
    if (g_backend && g_initialized && g_backend->deinit) {
        config_status_t status = g_backend->deinit(g_backend->ctx);
        if (status != CONFIG_OK) return status;
    }
    g_backend = NULL; g_initialized = false; g_auto_commit = false; g_dirty = false;
    memset(g_buffer, 0, g_capacity);
    return CONFIG_OK;
}
config_status_t config_backend_set(const config_backend_t* backend) {
    if (atomic_flag_test_and_set_explicit(&g_workspace_busy, memory_order_acquire))
        return CONFIG_ERROR_BUSY;
    config_status_t status = set_impl(backend);
    atomic_flag_clear_explicit(&g_workspace_busy, memory_order_release);
    return status;
}
config_status_t config_backend_deinit(void) {
    if (atomic_flag_test_and_set_explicit(&g_workspace_busy, memory_order_acquire))
        return CONFIG_ERROR_BUSY;
    config_status_t status = deinit_impl();
    atomic_flag_clear_explicit(&g_workspace_busy, memory_order_release);
    return status;
}
