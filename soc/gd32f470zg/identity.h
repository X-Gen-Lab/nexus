#ifndef NEXUS_GD32F470ZG_IDENTITY_H
#define NEXUS_GD32F470ZG_IDENTITY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t uid[3];
    uint32_t silicon_id;
    uint16_t flash_kib;
    uint16_t sram_kib;
} nx_gd32f470_identity_t;
/* Silicon observations are distinct from the product-owned PCB revision. */
void nx_gd32f470_identity(nx_gd32f470_identity_t* identity);
#ifdef __cplusplus
}
#endif
#endif
