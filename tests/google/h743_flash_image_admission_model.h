/**
 * \file            h743_flash_image_admission_model.h
 * \brief           External physical image identity for actual H743 pulses
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_IMAGE_ADMISSION_MODEL_H
#define NX_H743_FLASH_IMAGE_ADMISSION_MODEL_H
#include "h743_peripheral_model.h"
#include "stm32h743_flash_execution.h"
#ifdef __cplusplus
extern "C" {
#endif
const nx_stm32h743_flash_image_t* nx_h743_flash_image_admission_model(void);
#ifdef __cplusplus
}
#endif
/* The current provider does not consume this hardware/linker boundary yet.
 * Adding the model cannot change the current production admission behavior. */
#define NX_H743_FLASH_IMAGE() nx_h743_flash_image_admission_model()
#endif
