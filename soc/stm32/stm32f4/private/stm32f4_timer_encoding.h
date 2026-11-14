/**
 * \file            stm32f4_timer_encoding.h
 * \brief           Constant encodings for admitted timer configuration
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_TIMER_ENCODING_H
#define NEXUS_STM32F4_TIMER_ENCODING_H

/** \brief Bit of an admitted channel; zero denotes an absent array slot.
 * \note Arguments are constant expressions, without hardware side effects.
 */
#define NX_STM32_TIMER_CHANNEL_BIT(channel)                                    \
    ((channel) == 1U                                                           \
         ? 1U                                                                  \
         : ((channel) == 2U                                                    \
                ? 2U                                                           \
                : ((channel) == 3U ? 4U : ((channel) == 4U ? 8U : 0U))))

/** \brief Enable mask of up to four validated, uniquely selected channels. */
#define NX_STM32_TIMER_CHANNEL_MASK(first, second, third, fourth)              \
    (NX_STM32_TIMER_CHANNEL_BIT(first) | NX_STM32_TIMER_CHANNEL_BIT(second) |  \
     NX_STM32_TIMER_CHANNEL_BIT(third) | NX_STM32_TIMER_CHANNEL_BIT(fourth))

/** \brief Prescaler encoding after exact-divider admission by configuration.
 * \note Both arguments are positive constants and the quotient fits 16 bits.
 */
#define NX_STM32_TIMER_PRESCALER(source_hz, tick_hz)                           \
    (((source_hz) / (tick_hz)) - 1U)

/** \brief Pin mask after package/pad admission establishes a 0..15 index. */
#define NX_STM32_TIMER_PIN_MASK(pin) (1U << (pin))

/** \brief Translate the admitted masked idle output into a logical level. */
#define NX_STM32_TIMER_IDLE_HIGH(initial) ((initial) != 0U)

#endif
