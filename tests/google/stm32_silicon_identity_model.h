/**
 * \file            stm32_silicon_identity_model.h
 * \brief           Observed silicon IDCODE and Flash-density input ports
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_SILICON_IDENTITY_MODEL_H
#define NEXUS_STM32_SILICON_IDENTITY_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_f407_identity_model_idcode(void);
uint16_t nx_f407_identity_model_flash_kib(void);
#ifdef __cplusplus
}
#endif
#define NX_STM32_DEVICE_ID() nx_f407_identity_model_idcode()
#define NX_STM32_FLASH_KIB() nx_f407_identity_model_flash_kib()
#endif /* NEXUS_STM32_SILICON_IDENTITY_MODEL_H */
