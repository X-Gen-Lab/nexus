/**
 * \file            u575_rtc_model.cpp
 * \brief           RTC external register semantics and delayed source removal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_rtc_model.h"
#include <cstring>
nx_u575_rtc_model_t g_u575_rtc_model;
nx_stm32u575_system_t g_nx_stm32u575_system;
static uint32_t bcd(uint8_t value) {
    return (value / 10U) * 16U + value % 10U;
}
void nx_u575_rtc_model_calendar(const nx_rtc_calendar_t* c) {
    g_u575_rtc_model.rtc.TR =
        bcd(c->hour) << 16U | bcd(c->minute) << 8U | bcd(c->second);
    g_u575_rtc_model.rtc.DR = bcd(c->year) << 16U | c->weekday << 13U |
                              bcd(c->month) << 8U | bcd(c->day);
    auto& m = g_u575_rtc_model;
    uint32_t pattern = bcd(c->day) << 24U | bcd(c->hour) << 16U |
                       bcd(c->minute) << 8U | bcd(c->second);
    if (m.source_running && m.rtc.ALRMAR == pattern)
        m.rtc.SR |= RTC_SR_ALRAF;
}
void nx_u575_rtc_model_reset(void) {
    std::memset(&g_u575_rtc_model, 0, sizeof(g_u575_rtc_model));
    std::memset(&g_nx_stm32u575_system, 0, sizeof(g_nx_stm32u575_system));
    auto& m = g_u575_rtc_model;
    m.rcc.CR = RCC_CR_MSISON | RCC_CR_MSISRDY;
    m.rcc.CSR = 4U << RCC_CSR_MSISSRANGE_Pos;
    m.rcc.AHB3ENR = RCC_AHB3ENR_PWREN;
    m.rcc.APB3ENR = RCC_APB3ENR_RTCAPBEN;
    m.rcc.BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 |
                 RCC_BDCR_RTCEN | RCC_BDCR_LSEDRV_1;
    m.power.DBPR = PWR_DBPR_DBP;
    m.rtc.CR = RTC_CR_BYPSHAD;
    m.rtc.PRER = 0x007f00ffU;
    m.rtc.ICSR = RTC_ICSR_INITS;
    m.rtc.SSR = 255U;
    m.rtc.ALRMAR = 0x81234567U;
    m.rtc.ALRMASSR = 0x0300000fU;
    m.privileged = true;
    m.wpr_locked = true;
    g_nx_stm32u575_system.rcc = &m.rcc;
    g_nx_stm32u575_system.started = true;
    g_nx_stm32u575_system.timer_error = NX_SUCCESS;
    nx_rtc_calendar_t calendar = {99, 12, 31, 5, 23, 59, 58};
    nx_u575_rtc_model_calendar(&calendar);
}
void nx_u575_rtc_model_step(uint64_t us) {
    auto& m = g_u575_rtc_model;
    m.now += us;
    if ((m.rtc.CR & RTC_CR_ALRAE) == 0U && !m.hold_disable &&
        m.now >= m.disable_at)
        m.source_running = false;
}
uint32_t nx_u575_rtc_model_read(volatile uint32_t* reg) {
    auto& m = g_u575_rtc_model;
    ++m.reads;
    if ((uintptr_t)reg >= (uintptr_t)&m.rtc &&
        (uintptr_t)reg < (uintptr_t)&m.rtc + sizeof(m.rtc))
        ++m.rtc_reads;
    if (reg == &m.rtc.SSR || reg == &m.rtc.TR || reg == &m.rtc.DR) {
        ++m.calendar_reads;
        if (m.unstable_calendar || m.calendar_race_reads != 0U) {
            if (m.calendar_race_reads != 0U)
                --m.calendar_race_reads;
            if (reg == &m.rtc.SSR)
                return m.calendar_reads & 1U;
        }
    }
    return *reg;
}
void nx_u575_rtc_model_write(volatile uint32_t* reg, uint32_t value) {
    auto& m = g_u575_rtc_model;
    ++m.writes;
    if (reg == &m.rtc.WPR) {
        if (value == 0xcaU) {
            m.wpr_key = 1U;
        } else if (value == 0x53U && m.wpr_key == 1U && !m.hold_unlock) {
            m.wpr_locked = false;
            m.wpr_key = 0U;
            ++m.wpr_unlocks;
        } else {
            m.wpr_locked = true;
            m.wpr_key = 0U;
            ++m.wpr_locks;
        }
        return;
    }
    if ((reg == &m.rtc.CR || reg == &m.rtc.ALRMAR || reg == &m.rtc.ALRMASSR) &&
        m.wpr_locked) {
        ++m.protected_write_refusals;
        return;
    }
    if (reg == &m.rtc.CR) {
        if ((value & RTC_CR_ALRAE) == 0U && m.hold_disable)
            return;
        if ((value & RTC_CR_ALRAE) != 0U)
            m.source_running = true;
        else if ((m.rtc.CR & RTC_CR_ALRAE) != 0U)
            m.disable_at = m.now + 63U;
        *reg = value;
    } else if (reg == &m.rtc.SCR) {
        if (!m.hold_clear)
            m.rtc.SR &= ~value;
        *reg = value;
    } else if (reg == &m.rtc.ALRMAR || reg == &m.rtc.ALRMASSR) {
        if (m.source_running) {
            ++m.forbidden_writes;
            return;
        }
        if (!m.hold_restore)
            *reg = value;
    } else {
        ++m.forbidden_writes;
    }
}
