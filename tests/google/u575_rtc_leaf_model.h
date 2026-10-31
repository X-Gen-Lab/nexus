/**
 * \file            u575_rtc_leaf_model.h
 * \brief           Real RTC MMIO with external prepared-clock facts
 * \author          Nexus Team
 */
#ifndef NX_U575_RTC_LEAF_MODEL_H
#define NX_U575_RTC_LEAF_MODEL_H
#define NX_U575_RTC_DEFAULT_PORT 1
#include "u575_rtc_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_u575_rtc_official_unlock(RTC_TypeDef* registers);
void nx_u575_rtc_official_lock(RTC_TypeDef* registers);
void nx_u575_rtc_official_clear_alarm(RTC_TypeDef* registers);
extern RTC_TypeDef g_u575_rtc_leaf_decoy;
#ifdef __cplusplus
}
#endif
/* Execute the real volatile commands. RAM does not emulate WPR or W1C. */
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(reg)         (*(reg))
#define NX_U575_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#undef RTC
#define RTC (&g_u575_rtc_leaf_decoy)
#endif
