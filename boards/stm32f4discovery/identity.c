#include "identity.h"
#include "stm32f4xx_hal.h"
void nx_stm32f407_identity(nx_stm32f407_identity_t* out) {
    if (!out) return;
    out->uid[0]=HAL_GetUIDw0();
    out->uid[1]=HAL_GetUIDw1();
    out->uid[2]=HAL_GetUIDw2();
    out->silicon_device_id=(uint16_t)HAL_GetDEVID();
    out->silicon_revision_id=(uint16_t)HAL_GetREVID();
    out->flash_kib=*(volatile const uint16_t*)FLASHSIZE_BASE;
}
