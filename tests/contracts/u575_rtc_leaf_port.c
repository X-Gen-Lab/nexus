/**
 * \file            u575_rtc_leaf_port.c
 * \brief           Execute locked U575 LL RTC keys and flag command from C11
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u5xx_ll_rtc.h"
#include "u575_rtc_leaf_model.h"
void nx_u575_rtc_official_unlock(RTC_TypeDef* registers) {
    LL_RTC_DisableWriteProtection(registers);
}
void nx_u575_rtc_official_lock(RTC_TypeDef* registers) {
    LL_RTC_EnableWriteProtection(registers);
}
void nx_u575_rtc_official_clear_alarm(RTC_TypeDef* registers) {
    LL_RTC_ClearFlag_ALRA(registers);
}
