#ifndef NEXUS_STM32F407_IDENTITY_H
#define NEXUS_STM32F407_IDENTITY_H
#include <stdint.h>
typedef struct {
    uint32_t uid[3];
    uint16_t silicon_device_id;
    uint16_t silicon_revision_id;
    uint16_t flash_kib;
} nx_stm32f407_identity_t;
/* Observed silicon identity. PCB revision is a separate fixture-owned value. */
void nx_stm32f407_identity(nx_stm32f407_identity_t* identity);
#endif
