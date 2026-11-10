/**
 * \file            f407_rtc_model.cpp
 * \brief           External F407 RTC protection and clock crossing semantics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_rtc_model.h"
#include <cstring>
nx_f407_rtc_model_t g_f407_rtc_model;
nx_stm32_system_t g_nx_stm32_system;
static uint32_t bcd(uint8_t n) {
    return (n / 10U) * 16U + n % 10U;
}
void nx_f407_rtc_model_calendar(const nx_rtc_calendar_t* c) {
    auto& m = g_f407_rtc_model;
    m.rtc.TR = bcd(c->hour) << 16U | bcd(c->minute) << 8U | bcd(c->second);
    m.rtc.DR = bcd(c->year) << 16U | uint32_t{c->weekday} << 13U |
               bcd(c->month) << 8U | bcd(c->day);
    m.rtc.ISR =
        (m.rtc.ISR & ~uint32_t{RTC_ISR_INITS}) | (c->year ? RTC_ISR_INITS : 0U);
}
void nx_f407_rtc_model_reset(bool retained) {
    std::memset(&g_f407_rtc_model, 0, sizeof(g_f407_rtc_model));
    std::memset(&g_nx_stm32_system, 0, sizeof(g_nx_stm32_system));
    auto& m = g_f407_rtc_model;
    m.privileged = m.locked = true;
    m.rtc.PRER = 0x007f00ffU;
    m.rtc.DR = 0x2101U;
    m.rtc.SSR = 255U;
    m.rtc.ISR = RTC_ISR_ALRAWF | RTC_ISR_ALRBWF | RTC_ISR_WUTWF;
    if (retained) {
        m.rcc.APB1ENR = RCC_APB1ENR_PWREN;
        m.power.CR = PWR_CR_DBP;
        m.rcc.BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_RTCSEL_0 |
                     RCC_BDCR_RTCEN;
        m.rtc.CR = RTC_CR_BYPSHAD;
        nx_rtc_calendar_t c{99U, 12U, 31U, 5U, 23U, 59U, 58U};
        nx_f407_rtc_model_calendar(&c);
    }
    g_nx_stm32_system.started = true;
    g_nx_stm32_system.timer_error = NX_SUCCESS;
    g_nx_stm32_system.rcc = &m.rcc;
}
uint32_t nx_f407_rtc_model_read(volatile uint32_t* p) {
    auto& m = g_f407_rtc_model;
    if (p == &m.rcc.BDCR && (m.rcc.BDCR & RCC_BDCR_LSEON) && !m.hold_lse)
        m.rcc.BDCR |= RCC_BDCR_LSERDY;
    if (p == &m.rtc.ISR) {
        if ((m.rtc.ISR & RTC_ISR_INIT) && !m.hold_init)
            m.rtc.ISR |= RTC_ISR_INITF;
        if (!(m.rtc.CR & RTC_CR_ALRAE) && !m.hold_alarm &&
            !m.hold_standby_flags)
            m.rtc.ISR |= RTC_ISR_ALRAWF;
        if (!(m.rtc.CR & RTC_CR_ALRBE) && !m.hold_alarm_b &&
            !m.hold_standby_flags)
            m.rtc.ISR |= RTC_ISR_ALRBWF;
        if (!(m.rtc.CR & RTC_CR_WUTE) && !m.hold_wakeup &&
            !m.hold_standby_flags)
            m.rtc.ISR |= RTC_ISR_WUTWF;
        if ((m.rtc.ISR & RTC_ISR_RECALPF) && !m.hold_recalibration &&
            ++m.recalibration_reads >= 3U)
            m.rtc.ISR &= ~uint32_t{RTC_ISR_RECALPF};
    }
    if (p == &m.power.CSR && (m.power.CSR & PWR_CSR_BRE) && !m.hold_regulator)
        m.power.CSR |= PWR_CSR_BRR;
    if (p == &m.rtc.SSR || p == &m.rtc.TR || p == &m.rtc.DR) {
        ++m.calendar_reads;
        if (m.reload_pending && m.hold_reload) {
            if (p == &m.rtc.TR)
                return m.prior_time;
            if (p == &m.rtc.DR)
                return m.prior_date;
        }
        if (m.incoherent && p == &m.rtc.SSR)
            return m.calendar_reads & 1U;
    }
    return *p;
}
void nx_f407_rtc_model_write(volatile uint32_t* p, uint32_t v) {
    auto& m = g_f407_rtc_model;
    ++m.writes;
    if (m.observe_write != nullptr)
        m.observe_write(p, v);
    if (p == &m.rcc.AHB1ENR) {
        if ((v & RCC_AHB1ENR_BKPSRAMEN) && m.hold_sram_gate)
            return;
        if (!(v & RCC_AHB1ENR_BKPSRAMEN) && m.hold_sram_gate_disable)
            return;
        if ((v & RCC_AHB1ENR_GPIOCEN) && m.hold_gpio_clock_enable)
            return;
        if (!(v & RCC_AHB1ENR_GPIOCEN) && m.hold_gpio_clock_restore)
            return;
    }
    if (p == &m.power.CSR && !(v & PWR_CSR_BRE) && m.hold_regulator_disable)
        return;
    if (p == &m.exti.PR) {
        if (!m.hold_clear)
            m.exti.PR &= ~v;
        return;
    }
    if (p == &m.rcc.BDCR && (v & RCC_BDCR_BDRST))
        ++m.backup_resets;
    if (p == &m.rtc.WPR) {
        if (v == 0xcaU)
            m.key = 1U;
        else if (v == 0x53U && m.key == 1U) {
            m.locked = false;
            m.key = 0U;
        } else {
            m.locked = true;
            m.key = 0U;
        }
        return;
    }
    const uintptr_t address = reinterpret_cast<uintptr_t>(p);
    const uintptr_t rtc = reinterpret_cast<uintptr_t>(&m.rtc);
    if (address >= rtc && address < rtc + sizeof(m.rtc)) {
        if (m.locked) {
            ++m.illegal_writes;
            return;
        }
        if (m.hold_write && p != &m.rtc.ISR)
            return;
        if (p == &m.rtc.ISR) {
            if ((v & RTC_ISR_INIT) && !(m.rtc.ISR & RTC_ISR_INIT)) {
                m.prior_time = m.rtc.TR;
                m.prior_date = m.rtc.DR;
                m.reload_pending = false;
            }
            if (!(v & RTC_ISR_INIT) && (m.rtc.ISR & RTC_ISR_INIT))
                m.reload_pending = true;
            const uint32_t w0c = RTC_ISR_ALRAF | RTC_ISR_ALRBF | RTC_ISR_WUTF |
                                 RTC_ISR_TSF | RTC_ISR_TSOVF | RTC_ISR_TAMP1F |
                                 RTC_ISR_TAMP2F;
            uint32_t flags = m.rtc.ISR & w0c;
            if (!m.hold_clear)
                flags &= v;
            m.rtc.ISR =
                (m.rtc.ISR & ~uint32_t{w0c | RTC_ISR_INIT | RTC_ISR_INITF}) |
                flags | (v & RTC_ISR_INIT);
            if (v & RTC_ISR_INIT) {
                if (!m.hold_init)
                    m.rtc.ISR |= RTC_ISR_INITF;
            }
            return;
        }
        if ((p == &m.rtc.TR || p == &m.rtc.DR || p == &m.rtc.PRER ||
             p == &m.rtc.CALIBR) &&
            !(m.rtc.ISR & RTC_ISR_INITF)) {
            ++m.illegal_writes;
            return;
        }
        if (p == &m.rtc.PRER) {
            if (m.prer_writes < 4U)
                m.prer_values[m.prer_writes] = v;
            ++m.prer_writes;
        }
        if ((p == &m.rtc.ALRMAR || p == &m.rtc.ALRMASSR) &&
            !(m.rtc.ISR & RTC_ISR_ALRAWF)) {
            ++m.illegal_writes;
            return;
        }
        if ((p == &m.rtc.ALRMBR || p == &m.rtc.ALRMBSSR) &&
            !(m.rtc.ISR & RTC_ISR_ALRBWF)) {
            ++m.illegal_writes;
            return;
        }
        if (p == &m.rtc.WUTR && !(m.rtc.ISR & RTC_ISR_WUTWF)) {
            ++m.illegal_writes;
            return;
        }
        if (p == &m.rtc.CALR) {
            if (m.rtc.ISR & RTC_ISR_RECALPF) {
                ++m.illegal_writes;
                return;
            }
            m.rtc.ISR |= RTC_ISR_RECALPF;
            m.recalibration_reads = 0U;
        }
        if (p == &m.rtc.CR) {
            if (m.hold_alarm && !(v & RTC_CR_ALRAE) &&
                (m.rtc.CR & RTC_CR_ALRAE))
                return;
            if (v & RTC_CR_ALRAE)
                m.rtc.ISR &= ~uint32_t{RTC_ISR_ALRAWF};
            if (m.hold_alarm_b && !(v & RTC_CR_ALRBE) &&
                (m.rtc.CR & RTC_CR_ALRBE))
                return;
            if (m.hold_wakeup && !(v & RTC_CR_WUTE) && (m.rtc.CR & RTC_CR_WUTE))
                return;
            if (v & RTC_CR_ALRBE)
                m.rtc.ISR &= ~uint32_t{RTC_ISR_ALRBWF};
            if (v & RTC_CR_WUTE)
                m.rtc.ISR &= ~uint32_t{RTC_ISR_WUTWF};
        }
    }
    *p = v;
}
bool nx_f407_rtc_privileged(void) {
    return g_f407_rtc_model.privileged;
}
bool nx_f407_rtc_masked(void) {
    return g_f407_rtc_model.masked || g_f407_rtc_model.basepri != 0U ||
           g_f407_rtc_model.faultmask != 0U;
}
nx_arch_irq_masks_t nx_f407_rtc_masks(void) {
    return {g_f407_rtc_model.masked ? 1U : 0U, g_f407_rtc_model.basepri,
            g_f407_rtc_model.faultmask};
}
bool nx_f407_rtc_in_isr(void) {
    return g_f407_rtc_model.in_isr;
}
void nx_f407_rtc_dsb(void) {
}

static unsigned irq_index(IRQn_Type irq) {
    return irq == TAMP_STAMP_IRQn ? 0U : irq == RTC_WKUP_IRQn ? 1U : 2U;
}
uint32_t nx_f407_rtc_model_irq_enabled(IRQn_Type irq) {
    auto& m = g_f407_rtc_model;
    return m.irq_busy ? 1U : m.irq_enabled[irq_index(irq)];
}
uint32_t nx_f407_rtc_model_irq_pending(IRQn_Type irq) {
    return g_f407_rtc_model.irq_pending[irq_index(irq)];
}
uint32_t nx_f407_rtc_model_irq_active(IRQn_Type irq) {
    return g_f407_rtc_model.irq_active[irq_index(irq)];
}

#if !defined(NX_F407_RTC_REAL_SYSTEM)
extern "C" nx_time_us_t nx_stm32_system_now(nx_stm32_system_t* system) {
    if (system != &g_nx_stm32_system)
        return 0U;
    ++g_f407_rtc_model.clock_reads;
    return g_f407_rtc_model.now;
}
#endif
