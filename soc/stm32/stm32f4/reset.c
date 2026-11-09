/**
 * \file            reset.c
 * \brief           Captured reset causes independent of watchdog selection
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-11-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

/** \brief Map captured boot flags without clearing or inventing product policy.
 */
uint32_t nx_reset_cause(void) {
    uint32_t raw = g_nx_stm32_system.reset_cause;
    uint32_t result = 0U;
    if ((raw & RCC_CSR_PORRSTF) != 0U) {
        result |= NX_RESET_POWER_ON;
    }
    if ((raw & RCC_CSR_PINRSTF) != 0U) {
        result |= NX_RESET_PIN;
    }
    if ((raw & RCC_CSR_SFTRSTF) != 0U) {
        result |= NX_RESET_SOFTWARE;
    }
    if ((raw & RCC_CSR_IWDGRSTF) != 0U) {
        result |= NX_RESET_IWDG;
    }
    if ((raw & RCC_CSR_WWDGRSTF) != 0U) {
        result |= NX_RESET_WWDG;
    }
    if ((raw & RCC_CSR_LPWRRSTF) != 0U) {
        result |= NX_RESET_LOW_POWER;
    }
    if ((raw & RCC_CSR_BORRSTF) != 0U) {
        result |= NX_RESET_BROWNOUT;
    }
    return result;
}
