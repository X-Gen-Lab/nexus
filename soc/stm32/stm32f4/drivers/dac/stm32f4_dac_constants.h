/**
 * \file            stm32f4_dac_constants.h
 * \brief           Private constant projections for selected DAC channels
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DAC_CONSTANTS_H
#define NEXUS_STM32F4_DAC_CONSTANTS_H

/** \brief Channel enable bits for a validated one- or two-channel selection.
 * \note Arguments are static constants. Zero represents an absent second slot.
 */
#define NX_STM32_DAC_CHANNEL_MASK(first, second)                               \
    ((((first) == 1u || (second) == 1u) ? 1u : 0u) |                           \
     (((first) == 2u || (second) == 2u) ? 2u : 0u))

/** \brief Map ordered selected codes to one physical channel; absent is zero.
 * \note Arguments are constant expressions, without hardware side effects.
 */
#define NX_STM32_DAC_IDLE_CODE(channel, first, second, first_code,             \
                               second_code)                                    \
    (((channel) == (first)) ? (first_code)                                     \
                            : (((channel) == (second)) ? (second_code) : 0u))

#endif
