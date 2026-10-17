/**
 * \file            h743_high_factory_model.h
 * \brief           External MMIO/backing for the real generated high assembly
 * \author          Nexus Team
 */
#ifndef NX_H743_HIGH_FACTORY_MODEL_H
#define NX_H743_HIGH_FACTORY_MODEL_H
/* Generated facts precede the model's exact CORE check; high MMIO hooks
 * replace the common external aliases after those aliases are declared. */
#include "nexus_config.h"

#include "h743_peripheral_factory_model.h"

#include "h743_high_profile_model.h"
#ifdef __cplusplus
extern "C" {
#endif
/* External host backing only. Firmware retains the actual FLASH_BASE. */
extern uint32_t g_h743_high_factory_flash_words[524288U];
#ifdef __cplusplus
}
#endif
#undef FLASH_BASE
#define FLASH_BASE ((uintptr_t)g_h743_high_factory_flash_words)
#undef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_h743_high_model_write(&(reg), (uint32_t)(value))
#endif
