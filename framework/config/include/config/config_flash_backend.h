#ifndef NEXUS_CONFIG_FLASH_BACKEND_H
#define NEXUS_CONFIG_FLASH_BACKEND_H
#include "config/config_backend.h"
#include "nexus/storage.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Bind caller-owned opened storage while the backend is deinitialized.
 * No default partition or host path is selected; unbound use is UNSUPPORTED. */
const config_backend_t* config_backend_flash_get(void);
config_status_t config_backend_flash_bind(nx_storage_t* storage);
#ifdef __cplusplus
}
#endif
#endif
