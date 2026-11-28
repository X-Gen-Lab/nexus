/**
 * \file            instance.h
 * \brief           Private F103xB retained RTC and backup read owner.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_RTC_INSTANCE_H
#define NEXUS_STM32F1_RTC_INSTANCE_H

#include "nexus/io/rtc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Uncopied state for one exclusive PWR/BKP owner, initially zero. */
typedef struct {
    uint8_t effects;
} nx_stm32f1_rtc_t;

/** \brief Adopt a pre-running LSE crystal and resynchronize the APB interface.
 * \note The assembly owns both interface gates and serializes Thread access.
 *       The RTC source, divider, counter, alarm and backup words are retained.
 *       Close after success or error to finish any pending gate withdrawal.
 */
nx_result_t nx_stm32f1_rtc_prepare(nx_stm32f1_rtc_t* state);

/** \brief Restore only the interface effects added by this owner. */
nx_result_t nx_stm32f1_rtc_close(nx_stm32f1_rtc_t* state);

/** \brief Read the raw modulo-2^32 counter, with no unit or epoch promise. */
nx_result_t nx_stm32f1_rtc_ticks(const nx_stm32f1_rtc_t* state, uint32_t* out);

/** \brief Read zero-based BKP words as zero-extended 16-bit data, and stop. */
extern const nx_rtc_ops_t nx_stm32f1_rtc_backup_ops;

#ifdef __cplusplus
}
#endif
#endif
