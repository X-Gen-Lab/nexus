#ifndef NEXUS_STM32F407_FLASH_H
#define NEXUS_STM32F407_FLASH_H
#include "nexus/storage.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Sector 10/11 only. Requires linker __nexus_storage_start/end reservations.
 * Returns NULL if the image does not reserve exactly this partition.
 * Task context; serialized maintenance window; VDD must be 2.7..3.6 V. */
const nx_flash_port_t* nx_stm32f407_flash_port(void);
#ifdef __cplusplus
}
#endif
#endif
