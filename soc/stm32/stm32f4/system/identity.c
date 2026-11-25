/**
 * \file            identity.c
 * \brief           Read-only selected silicon and observed Flash admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-25
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "private/stm32f407_silicon.h"
#include "stm32f4xx.h"
#include "system/stm32f4_identity.h"

#ifndef NX_STM32_FLASH_KIB
#define NX_STM32_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_STM32_DEVICE_ID() (DBGMCU->IDCODE)
#endif

/** \brief Validate observed density without inferring the package from ID. */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes) {
    if (expected_flash_bytes != NX_STM32_FLASH_MIN_BYTES &&
        expected_flash_bytes != NX_STM32_FLASH_MAX_BYTES)
        return NX_ERROR_INVALID;
    uint32_t observed_bytes = (uint32_t)NX_STM32_FLASH_KIB() * 1024U;
    if (observed_bytes != expected_flash_bytes)
        return NX_ERROR_IO;
    return nx_stm32f407_silicon_admit(NX_STM32_DEVICE_ID());
}
