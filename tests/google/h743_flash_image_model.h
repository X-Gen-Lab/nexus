/**
 * \file            h743_flash_image_model.h
 * \brief           Shared external physical image for H743 Flash model tests
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_IMAGE_MODEL_H
#define NX_H743_FLASH_IMAGE_MODEL_H
#include "stm32h743_flash_execution.h"
#ifdef __cplusplus
extern "C" {
#endif
const nx_stm32h743_flash_image_t* nx_h743_flash_model_image(void);
#ifdef __cplusplus
}
#endif
#ifndef NX_H743_FLASH_IMAGE
#define NX_H743_FLASH_IMAGE() nx_h743_flash_model_image()
#endif
#endif
