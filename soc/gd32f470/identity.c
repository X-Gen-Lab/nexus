#include "identity.h"
#include "gd32f4xx.h"

void nx_gd32f470_identity(nx_gd32f470_identity_t* identity) {
    if (!identity) { return; }
    /* GD32F4xx UM Rev3.3 1.6: never infer identity from another MCU family. */
    const volatile uint32_t* uid = (const volatile uint32_t*)0x1FFF7A10u;
    identity->uid[0] = uid[0];
    identity->uid[1] = uid[1];
    identity->uid[2] = uid[2];
    identity->silicon_id = DBG_ID;
    uint32_t density = *(volatile const uint32_t*)0x1FFF7A20u;
    identity->flash_kib = (uint16_t)(density >> 16);
    identity->sram_kib = (uint16_t)density;
}
