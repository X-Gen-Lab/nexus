/**
 * \file            h743_flash_fixture_high_model.h
 * \brief           Existing high-kernel CPU/MPU hardware composition
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_FIXTURE_HIGH_MODEL_H
#define NX_H743_FLASH_FIXTURE_HIGH_MODEL_H
#include "h743_high_profile_model.h"
#define NX_H743_FLASH_MPU_BASE_READ(reg) nx_h743_clock_read(reg)
#define NX_H743_FLASH_MPU_BASE_WRITE(reg, value)                               \
    nx_h743_high_model_write(reg, value)
#include "h743_flash_mpu_model.h"
#endif
