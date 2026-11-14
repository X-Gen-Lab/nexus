/**
 * \file            stm32f4_aux_clock.h
 * \brief           Selected System-owned PLLSAI outputs and read-only identity
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_AUX_CLOCK_H
#define NEXUS_STM32F4_AUX_CLOCK_H
#include "nexus_config.h"
#include "stm32f4xx.h"
#include <stdbool.h>
#include <stddef.h>

#ifndef NEXUS_CLOCK_PLLSAI_ENABLED
#define NEXUS_CLOCK_PLLSAI_ENABLED 0U
#endif
#ifndef NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
#define NEXUS_CLOCK_PLLSAI_SAI1_ENABLED 0U
#endif
#ifndef NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
#define NEXUS_CLOCK_PLLSAI_LTDC_ENABLED 0U
#endif
#ifndef NEXUS_CLOCK_PLLSAI_SAI2_ENABLED
#define NEXUS_CLOCK_PLLSAI_SAI2_ENABLED 0U
#endif
#if NEXUS_CLOCK_PLLSAI_SAI2_ENABLED &&                                         \
    (!defined(STM32F446xx) || !NEXUS_CLOCK_PLLSAI_SAI1_ENABLED)
#error "Selected SAI2 master requires the reviewed F446 shared PLLSAI Q output"
#endif

#if NEXUS_CLOCK_PLLSAI_ENABLED
#if !defined(STM32F446xx) && !defined(STM32F479xx)
#error "Selected PLLSAI requires the reviewed F446 or F479 hardware"
#endif
#if !NEXUS_CLOCK_PLLSAI_SAI1_ENABLED && !NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
#error "Selected PLLSAI must have a declared output consumer"
#endif
#if NEXUS_CLOCK_PLLSAI_M < 2U || NEXUS_CLOCK_PLLSAI_M > 63U ||                 \
    NEXUS_CLOCK_PLLSAI_N < 50U || NEXUS_CLOCK_PLLSAI_N > 432U ||               \
    NEXUS_CLOCK_INPUT_HZ < 1000000ULL * NEXUS_CLOCK_PLLSAI_M ||                \
    NEXUS_CLOCK_INPUT_HZ > 2000000ULL * NEXUS_CLOCK_PLLSAI_M
#error "Selected PLLSAI input or multiplier is outside its encoded range"
#endif
#if defined(STM32F446xx)
#define NX_STM32_PLLSAI_VCO_MIN 100000000ULL
#define NX_STM32_PLLSAI_M_MASK  RCC_PLLSAICFGR_PLLSAIM
#define NX_STM32_PLLSAI_M_BITS                                                 \
    (NEXUS_CLOCK_PLLSAI_M << RCC_PLLSAICFGR_PLLSAIM_Pos)
#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
#error "The reviewed F446 has no PLLSAI R output or LTDC"
#endif
#else
#define NX_STM32_PLLSAI_VCO_MIN 192000000ULL
#define NX_STM32_PLLSAI_M_MASK  0U
#define NX_STM32_PLLSAI_M_BITS  0U
#if NEXUS_CLOCK_PLLSAI_M != NEXUS_CLOCK_PLLM
#error "F479 PLLSAI shares the selected System source and M"
#endif
#endif
#if 1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N <                       \
        NX_STM32_PLLSAI_VCO_MIN * NEXUS_CLOCK_PLLSAI_M ||                      \
    1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N >                       \
        432000000ULL * NEXUS_CLOCK_PLLSAI_M
#error "Selected PLLSAI VCO exceeds the exact device's electrical limits"
#endif

#if NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
#if NEXUS_CLOCK_PLLSAI_Q < 2U || NEXUS_CLOCK_PLLSAI_Q > 15U ||                 \
    NEXUS_CLOCK_PLLSAI_DIVQ < 1U || NEXUS_CLOCK_PLLSAI_DIVQ > 32U ||           \
    1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N >                       \
        216000000ULL * NEXUS_CLOCK_PLLSAI_M * NEXUS_CLOCK_PLLSAI_Q ||          \
    (1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N) %                     \
            (NEXUS_CLOCK_PLLSAI_M * NEXUS_CLOCK_PLLSAI_Q *                     \
             NEXUS_CLOCK_PLLSAI_DIVQ) !=                                       \
        0ULL ||                                                                \
    (1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N) /                     \
            (NEXUS_CLOCK_PLLSAI_M * NEXUS_CLOCK_PLLSAI_Q *                     \
             NEXUS_CLOCK_PLLSAI_DIVQ) !=                                       \
        NEXUS_CLOCK_PLLSAI_SAI1_HZ
#error "Selected PLLSAI Q/divider requires an exact legal SAI1 kernel"
#endif
#define NX_STM32_PLLSAI_Q_MASK RCC_PLLSAICFGR_PLLSAIQ
#define NX_STM32_PLLSAI_Q_BITS                                                 \
    (NEXUS_CLOCK_PLLSAI_Q << RCC_PLLSAICFGR_PLLSAIQ_Pos)
#if defined(STM32F446xx)
#if NEXUS_CLOCK_PLLSAI_SAI2_ENABLED
#define NX_STM32_PLLSAI_Q_MUX (RCC_DCKCFGR_SAI1SRC | RCC_DCKCFGR_SAI2SRC)
#else
#define NX_STM32_PLLSAI_Q_MUX RCC_DCKCFGR_SAI1SRC
#endif
#else
#define NX_STM32_PLLSAI_Q_MUX (RCC_DCKCFGR_SAI1ASRC | RCC_DCKCFGR_SAI1BSRC)
#endif
#define NX_STM32_PLLSAI_Q_MUX_MASK                                             \
    (RCC_DCKCFGR_PLLSAIDIVQ | NX_STM32_PLLSAI_Q_MUX)
#define NX_STM32_PLLSAI_Q_MUX_BITS                                             \
    ((NEXUS_CLOCK_PLLSAI_DIVQ - 1U) << RCC_DCKCFGR_PLLSAIDIVQ_Pos)
#else
#define NX_STM32_PLLSAI_Q_MASK     0U
#define NX_STM32_PLLSAI_Q_BITS     0U
#define NX_STM32_PLLSAI_Q_MUX_MASK 0U
#define NX_STM32_PLLSAI_Q_MUX_BITS 0U
#endif

#if NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
#if NEXUS_CLOCK_PLLSAI_R < 2U || NEXUS_CLOCK_PLLSAI_R > 7U ||                  \
    (NEXUS_CLOCK_PLLSAI_DIVR != 2U && NEXUS_CLOCK_PLLSAI_DIVR != 4U &&         \
     NEXUS_CLOCK_PLLSAI_DIVR != 8U && NEXUS_CLOCK_PLLSAI_DIVR != 16U) ||       \
    1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N >                       \
        216000000ULL * NEXUS_CLOCK_PLLSAI_M * NEXUS_CLOCK_PLLSAI_R ||          \
    (1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N) %                     \
            (NEXUS_CLOCK_PLLSAI_M * NEXUS_CLOCK_PLLSAI_R *                     \
             NEXUS_CLOCK_PLLSAI_DIVR) !=                                       \
        0ULL ||                                                                \
    (1ULL * NEXUS_CLOCK_INPUT_HZ * NEXUS_CLOCK_PLLSAI_N) /                     \
            (NEXUS_CLOCK_PLLSAI_M * NEXUS_CLOCK_PLLSAI_R *                     \
             NEXUS_CLOCK_PLLSAI_DIVR) !=                                       \
        NEXUS_CLOCK_PLLSAI_LTDC_HZ
#error "Selected PLLSAI R/divider requires an exact legal LTDC pixel clock"
#endif
#define NX_STM32_PLLSAI_R_MASK RCC_PLLSAICFGR_PLLSAIR
#define NX_STM32_PLLSAI_R_BITS                                                 \
    (NEXUS_CLOCK_PLLSAI_R << RCC_PLLSAICFGR_PLLSAIR_Pos)
#define NX_STM32_PLLSAI_R_MUX_MASK RCC_DCKCFGR_PLLSAIDIVR
#define NX_STM32_PLLSAI_R_MUX_BITS                                             \
    ((NEXUS_CLOCK_PLLSAI_DIVR == 2U   ? 0U                                     \
      : NEXUS_CLOCK_PLLSAI_DIVR == 4U ? 1U                                     \
      : NEXUS_CLOCK_PLLSAI_DIVR == 8U ? 2U                                     \
                                      : 3U)                                    \
     << RCC_DCKCFGR_PLLSAIDIVR_Pos)
#else
#define NX_STM32_PLLSAI_R_MASK     0U
#define NX_STM32_PLLSAI_R_BITS     0U
#define NX_STM32_PLLSAI_R_MUX_MASK 0U
#define NX_STM32_PLLSAI_R_MUX_BITS 0U
#endif
#define NX_STM32_PLLSAI_FIELD_MASK                                             \
    (NX_STM32_PLLSAI_M_MASK | RCC_PLLSAICFGR_PLLSAIN |                         \
     RCC_PLLSAICFGR_PLLSAIP | NX_STM32_PLLSAI_Q_MASK | NX_STM32_PLLSAI_R_MASK)
#define NX_STM32_PLLSAI_FIELDS                                                 \
    (NX_STM32_PLLSAI_M_BITS | RCC_PLLSAICFGR_PLLSAIP_0 |                       \
     (NEXUS_CLOCK_PLLSAI_N << RCC_PLLSAICFGR_PLLSAIN_Pos) |                    \
     NX_STM32_PLLSAI_Q_BITS | NX_STM32_PLLSAI_R_BITS)
#define NX_STM32_PLLSAI_MUX_MASK                                               \
    (NX_STM32_PLLSAI_Q_MUX_MASK | NX_STM32_PLLSAI_R_MUX_MASK)
#define NX_STM32_PLLSAI_MUX_BITS                                               \
    (NX_STM32_PLLSAI_Q_MUX_BITS | NX_STM32_PLLSAI_R_MUX_BITS)
#endif

/** \brief A declared CLK48 consumer is never silently moved to safe P.
 * \param[in] rcc: Accessible RCC registers, no concurrent clock writer.
 * \return True when PLLSAI P is not selected by an external CLK48 owner.
 * \note RM0390/RM0386 6.3.24 limit even unused P to 120 MHz. System uses
 *       derived P=/4 (25..108 MHz for the reviewed VCO range), saving/restoring
 *       its old bits; this does not grant or change a CLK48 source selection.
 */
static inline bool nx_stm32_aux_clock48_unselected(const RCC_TypeDef* rcc) {
#if NEXUS_CLOCK_PLLSAI_ENABLED && defined(STM32F446xx)
    return rcc != NULL && (rcc->DCKCFGR2 & RCC_DCKCFGR2_CK48MSEL) == 0U;
#elif NEXUS_CLOCK_PLLSAI_ENABLED && defined(STM32F479xx)
    return rcc != NULL && (rcc->DCKCFGR & RCC_DCKCFGR_CK48MSEL) == 0U;
#else
    return rcc != NULL;
#endif
}

/** \brief Unselected Q/R fields must be legal without being taken over.
 * \param[in] rcc: Accessible RCC registers before PLLSAI is enabled.
 * \return False for a foreign CLK48 owner or invalid parked divider encoding.
 * \note Only selected Q/R fields are written or restored. Legal unselected
 *       divisors are at least two, bounding either physical output to216 MHz.
 */
static inline bool nx_stm32_aux_clock_outputs_valid(const RCC_TypeDef* rcc) {
    if (!nx_stm32_aux_clock48_unselected(rcc))
        return false;
#if NEXUS_CLOCK_PLLSAI_ENABLED && !NEXUS_CLOCK_PLLSAI_SAI1_ENABLED
    if (((rcc->PLLSAICFGR & RCC_PLLSAICFGR_PLLSAIQ) >>
         RCC_PLLSAICFGR_PLLSAIQ_Pos) < 2U)
        return false;
#endif
#if NEXUS_CLOCK_PLLSAI_ENABLED && defined(STM32F479xx) &&                      \
    !NEXUS_CLOCK_PLLSAI_LTDC_ENABLED
    if (((rcc->PLLSAICFGR & RCC_PLLSAICFGR_PLLSAIR) >>
         RCC_PLLSAICFGR_PLLSAIR_Pos) < 2U)
        return false;
#endif
    return true;
}

/** \brief Inspect only the selected auxiliary outputs without RCC mutation.
 * \param[in] rcc: Accessible RCC snapshot, no concurrent clock writers.
 * \return True only when the selected common PLL and declared muxes match.
 * \note No selected auxiliary output adds no read or runtime storage. This
 *       is a nominal hardware identity, never oscillator accuracy or HIL.
 */
static inline bool nx_stm32_aux_clock_profile_matches(const RCC_TypeDef* rcc) {
#if NEXUS_CLOCK_PLLSAI_ENABLED
    return nx_stm32_aux_clock_outputs_valid(rcc) &&
           (rcc->CR & (RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY)) ==
               (RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY) &&
           (rcc->PLLSAICFGR & NX_STM32_PLLSAI_FIELD_MASK) ==
               NX_STM32_PLLSAI_FIELDS &&
           (rcc->DCKCFGR & NX_STM32_PLLSAI_MUX_MASK) ==
               NX_STM32_PLLSAI_MUX_BITS;
#else
    (void)rcc;
    return true;
#endif
}
#endif
