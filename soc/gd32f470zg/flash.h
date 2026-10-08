#ifndef NEXUS_GD32F470ZG_FLASH_H
#define NEXUS_GD32F470ZG_FLASH_H
#include "nexus/storage.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Last 16 KiB of 1MiB Flash, four independent 4 KiB erase pages (GD32F470
 * special page-erase controller). Linker must reserve exactly this range.
 * Task context, serialized maintenance window; 3.3 V reference board supply.
 * Erase/program stalls code/IRQ fetch from the same bank: not control-safe.
 */
const nx_flash_port_t* nx_gd32f470_flash_port(void);
#ifdef __cplusplus
}
#endif
#endif
