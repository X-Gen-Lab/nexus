#include "config/config_backend.h"

static nx_storage_t* g_partition;
static bool g_initialized;
static config_status_t translate(nx_storage_status_t status, bool writing) {
    switch (status) {
        case NX_STORAGE_OK: return CONFIG_OK;
        case NX_STORAGE_NOT_FOUND: return CONFIG_ERROR_NOT_FOUND;
        case NX_STORAGE_CORRUPT: return CONFIG_ERROR_INVALID_FORMAT;
        case NX_STORAGE_NO_SPACE: return CONFIG_ERROR_NO_SPACE;
        case NX_STORAGE_UNSUPPORTED: return CONFIG_ERROR_UNSUPPORTED;
        case NX_STORAGE_INVALID: return CONFIG_ERROR_INVALID_PARAM;
        default: return writing ? CONFIG_ERROR_NVS_WRITE : CONFIG_ERROR_NVS_READ;
    }
}
static config_status_t init(void* ctx) {
    (void)ctx;
    if (!g_partition) return CONFIG_ERROR_UNSUPPORTED;
    nx_flash_port_t port = g_partition->flash;
    nx_storage_status_t status = nx_storage_open(g_partition, &port,
                                                g_partition->offset,
                                                g_partition->bank_size);
    g_initialized = status == NX_STORAGE_OK;
    return translate(status, false);
}
static config_status_t deinit(void* ctx) {
    (void)ctx; g_initialized = false; return CONFIG_OK;
}
static config_status_t save(void* ctx, const void* data, size_t size) {
    (void)ctx;
    if (!g_initialized) return CONFIG_ERROR_NOT_INIT;
    return translate(nx_storage_save(g_partition, data, size), true);
}
static config_status_t load(void* ctx, void* data, size_t* size) {
    (void)ctx;
    if (!g_initialized) return CONFIG_ERROR_NOT_INIT;
    config_status_t status = init(NULL);
    if (status != CONFIG_OK) return status;
    return translate(nx_storage_load(g_partition, data, size), false);
}
static config_status_t read(void* c, const char* k, void* d, size_t* s) {
    (void)c; (void)k; (void)d; (void)s; return CONFIG_ERROR_UNSUPPORTED;
}
static config_status_t write(void* c, const char* k, const void* d, size_t s) {
    (void)c; (void)k; (void)d; (void)s; return CONFIG_ERROR_UNSUPPORTED;
}
static config_status_t erase(void* c, const char* k) {
    (void)c; (void)k; return CONFIG_ERROR_UNSUPPORTED;
}
static const config_backend_t g_backend = {
    .name = "flash", .init = init, .deinit = deinit, .read = read,
    .write = write, .erase = erase, .save_snapshot = save, .load_snapshot = load
};
config_status_t config_backend_flash_bind(nx_storage_t* storage) {
    if (g_initialized) return CONFIG_ERROR_ALREADY_INIT;
    if (storage && !storage->opened) return CONFIG_ERROR_INVALID_PARAM;
    g_partition = storage; return CONFIG_OK;
}
const config_backend_t* config_backend_flash_get(void) { return &g_backend; }
