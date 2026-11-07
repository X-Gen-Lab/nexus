/**
 * \file            stm32f407_hal_profile.h
 * \brief           Exact F407 owned-UART compile policy for the pinned HAL
 * \author          Nexus Team
 * \note            Install after the vendor headers in every provider and
 *                  HAL translation unit. Vendor source remains unchanged.
 */
#ifndef NEXUS_STM32F407_HAL_PROFILE_H
#define NEXUS_STM32F407_HAL_PROFILE_H

#include "stm32f407_hal_atomic.h"
#include "stm32f4xx_hal.h"

/** \brief Round an OVER16 divisor exactly without intermediate overflow.
 * \note Cold validation rejects zero baud and divisors outside the BRR field.
 *       The pinned decimal HAL expression truncates some valid nearest values.
 */
static inline uint32_t nx_stm32_hal_brr16(uint32_t clock, uint32_t baud) {
    return baud == 0U ? 0U
                      : clock / baud +
                            (clock % baud >= baud / 2U + baud % 2U ? 1U : 0U);
}

/* UART_SetConfig applies this value before its first UE write. */
#undef UART_BRR_SAMPLING16
#define UART_BRR_SAMPLING16(clock, baud)                                       \
    nx_stm32_hal_brr16((uint32_t)(clock), (uint32_t)(baud))

#endif
