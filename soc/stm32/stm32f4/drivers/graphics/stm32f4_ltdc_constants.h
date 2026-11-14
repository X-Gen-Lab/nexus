/**
 * \file            stm32f4_ltdc_constants.h
 * \brief           Constant encoding for the locked LTDC HAL field layout
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_LTDC_CONSTANTS_H
#define NX_STM32F4_LTDC_CONSTANTS_H
#include <stdint.h>

/** \brief Encode validated pixel counts as an official constant initializer.
 * \note The locked HAL field order is used in C11 and C++17. Configuration
 *       admits the accumulated bounds; C owns the hardware count-minus-one
 *       encoding. No runtime calculation or background state is introduced.
 */
#define NX_STM32_LTDC_TIMING(width, height, hs, hb, hf, vs, vb, vf, hp, vp,    \
                             dp, cp)                                           \
    {                                                                          \
        (hp), (vp), (dp), (cp), (hs)-1U, (vs)-1U, (hs) + (hb)-1U,              \
            (vs) + (vb)-1U, (hs) + (hb) + (width)-1U,                          \
            (vs) + (vb) + (height)-1U, (hs) + (hb) + (width) + (hf)-1U,        \
            (vs) + (vb) + (height) + (vf)-1U, {                                \
            0U, 0U, 0U, 0U                                                     \
        }                                                                      \
    }

/** \brief Public pixel-format bit in the readonly layer capability mask. */
#define NX_STM32_LTDC_FORMAT(format) (UINT32_C(1) << (format))

/** \brief One or two validated public formats; repeating one is idempotent. */
#define NX_STM32_LTDC_FORMATS(first, second)                                   \
    (NX_STM32_LTDC_FORMAT(first) | NX_STM32_LTDC_FORMAT(second))

#endif /* NX_STM32F4_LTDC_CONSTANTS_H */
