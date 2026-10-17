/**
 * \file            g474_adc_group_model.cpp
 * \brief           External selected register semantics, not a provider mock
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_adc_group_model.h"
#include "stm32g474_system.h"
#include <atomic>
#include <cstring>
extern "C" {
nx_ag_model_t g_ag;
#ifndef NX_G474_ADC_GROUP_EXTERNAL_CORE
nx_stm32g474_system_t g_nx_stm32g474_system;
#endif
}
namespace {
constexpr uint16_t samples_x2[8]{5U, 13U, 25U, 49U, 95U, 185U, 495U, 1281U};
bool inside(const volatile uint32_t* p, const void* base, size_t bytes) {
    return (uintptr_t)p >= (uintptr_t)base &&
           (uintptr_t)p < (uintptr_t)base + bytes;
}
void event(nx_ag_event_t kind, unsigned rank) {
    if (g_ag.event != nullptr) {
        g_ag.event(kind, rank);
    }
}
unsigned clock_divider() {
    constexpr unsigned prescalers[12]{1U,  2U,  4U,  6U,  8U,   10U,
                                      12U, 16U, 32U, 64U, 128U, 256U};
    unsigned code = static_cast<unsigned>((ADC12_COMMON->CCR & ADC_CCR_PRESC) >>
                                          ADC_CCR_PRESC_Pos);
    return code < 12U ? prescalers[code] : 256U;
}
unsigned channel(unsigned rank, bool injected) {
    if (injected) {
        return (ADC1->JSQR >> (ADC_JSQR_JSQ1_Pos + 6U * rank)) & 31U;
    }
    if (rank < 4U) {
        return (ADC1->SQR1 >> (6U + 6U * rank)) & 31U;
    }
    if (rank < 9U) {
        return (ADC1->SQR2 >> (6U * (rank - 4U))) & 31U;
    }
    if (rank < 14U) {
        return (ADC1->SQR3 >> (6U * (rank - 9U))) & 31U;
    }
    return (ADC1->SQR4 >> (6U * (rank - 14U))) & 31U;
}
unsigned sample_code(unsigned ch) {
    return ch < 10U ? ((ADC1->SMPR1 >> (3U * ch)) & 7U)
                    : ((ADC1->SMPR2 >> (3U * (ch - 10U))) & 7U);
}
void schedule_rank(bool latency) {
    unsigned ch = channel(g_ag.rank, g_ag.injected);
    unsigned sample = ch < 19U ? samples_x2[sample_code(ch)] : 1281U;
    g_ag.due = g_ag.cycles + ((uint64_t)sample + 25U + (latency ? 5U : 0U)) *
                                 clock_divider() / 2U;
}
void start(bool injected, bool hardware) {
    if (g_ag.converting || (ADC1->CR & ADC_CR_ADEN) == 0U ||
        (!injected && g_ag.limited)) {
        return;
    }
    g_ag.injected = injected;
    g_ag.rank = 0U;
    g_ag.count = injected ? ((ADC1->JSQR & ADC_JSQR_JL) + 1U)
                          : ((ADC1->SQR1 & ADC_SQR1_L) + 1U);
    g_ag.converting = true;
    g_ag.long_idle_bad = g_ag.last_conversion == 0U ||
                         g_ag.cycles - g_ag.last_conversion > 170000U;
    schedule_rank(hardware);
}
void edge() {
    ++g_ag.trigger_edges;
    event(NX_AG_TIMER_EDGE, 0U);
    g_ag.trigger_pending = true;
    g_ag.trigger_due = g_ag.cycles + 5U * clock_divider() / 2U;
}
void synchronized_trigger() {
    unsigned ext = static_cast<unsigned>((ADC1->CFGR & ADC_CFGR_EXTSEL) >>
                                         ADC_CFGR_EXTSEL_Pos);
    unsigned jext = static_cast<unsigned>((ADC1->JSQR & ADC_JSQR_JEXTSEL) >>
                                          ADC_JSQR_JEXTSEL_Pos);
    if ((ADC1->CR & ADC_CR_ADSTART) != 0U && ext == 13U &&
        (ADC1->CFGR & ADC_CFGR_EXTEN) == ADC_CFGR_EXTEN_0) {
        start(false, false);
    }
    if ((ADC1->CR & ADC_CR_JADSTART) != 0U && jext == 14U &&
        (ADC1->JSQR & ADC_JSQR_JEXTEN) == ADC_JSQR_JEXTEN_0) {
        start(true, false);
    }
}
void dma(uint16_t data) {
    if ((ADC1->CFGR & ADC_CFGR_DMAEN) == 0U || g_ag.hold_dma ||
        (DMA1_Channel1->CCR & DMA_CCR_EN) == 0U || DMA1_Channel1->CNDTR == 0U ||
        (ADC1->ISR & ADC_ISR_OVR) != 0U) {
        return;
    }
    if (DMAMUX1_Channel0->CCR != 5U ||
        DMA1_Channel1->CPAR != (uint32_t)(uintptr_t)&ADC1->DR) {
        DMA1->ISR |= DMA_ISR_TEIF1;
        return;
    }
    auto* output = reinterpret_cast<uint16_t*>((uintptr_t)DMA1_Channel1->CMAR);
    const unsigned index = g_ag.dma_initial - DMA1_Channel1->CNDTR;
    output[index] = data;
    --DMA1_Channel1->CNDTR;
    ++g_ag.beats;
    ADC1->ISR &= ~(uint32_t)ADC_ISR_EOC;
    event(NX_AG_DMA_BEAT, g_ag.rank);
    if (DMA1_Channel1->CNDTR == 0U) {
        DMA1->ISR |= DMA_ISR_TCIF1;
        event(NX_AG_TC, g_ag.rank);
        if ((ADC1->CFGR & ADC_CFGR_DMACFG) == 0U) {
            g_ag.limited = true;
            g_ag.needs_reenable = (DBGMCU->IDCODE >> 16U) == 0x2002U;
        }
    }
}
void complete_rank() {
    unsigned ch = channel(g_ag.rank, g_ag.injected);
    uint16_t data = ch < 19U ? g_ag.values[ch] : 0xFFEU;
    if (ch >= 16U && ch <= 18U) {
        const unsigned source = ch - 16U;
        const uint32_t enable[3]{ADC_CCR_VSENSESEL, ADC_CCR_VBATSEL,
                                 ADC_CCR_VREFEN};
        const uint32_t startup_us[3]{120U, 0U, 12U};
        const unsigned min_code[3]{6U, 7U, 5U};
        if ((ADC12_COMMON->CCR & enable[source]) == 0U ||
            g_ag.cycles - g_ag.internal_enabled_at[source] <
                (uint64_t)startup_us[source] * 170U ||
            sample_code(ch) < min_code[source]) {
            data = 0U;
        }
    }
    if (g_ag.long_idle_bad || g_ag.needs_reenable) {
        data = 0U;
        g_ag.long_idle_bad = false;
        g_ag.needs_reenable = false;
    }
    /* Z MUX disturbance corrupts only the rank preceding a channel switch.
     * Repeating each channel twice makes only padding carry this disturbance.
     */
    unsigned next = g_ag.rank + 1U < g_ag.count
                        ? channel(g_ag.rank + 1U, g_ag.injected)
                        : 31U;
    if ((DBGMCU->IDCODE >> 16U) == 0x2001U && next != ch &&
        (ADC1->CFGR2 & ADC_CFGR2_BULB) == 0U &&
        (ADC1->CFGR & ADC_CFGR_CONT) == 0U) {
        data = (uint16_t)(data ^ 3U);
        g_ag.duplicate_disturbance = true;
    }
    g_ag.last_conversion = g_ag.cycles;
    event(NX_AG_RANK, g_ag.rank);
    if (g_ag.injected) {
        if (g_ag.rank == 0U) {
            ADC1->JDR1 = data;
        } else if (g_ag.rank == 1U) {
            ADC1->JDR2 = data;
        } else if (g_ag.rank == 2U) {
            ADC1->JDR3 = data;
        } else {
            ADC1->JDR4 = data;
        }
        ADC1->ISR |= ADC_ISR_JEOC;
    } else {
        if ((ADC1->ISR & ADC_ISR_EOC) != 0U) {
            ADC1->ISR |= ADC_ISR_OVR;
        }
        if ((ADC1->ISR & ADC_ISR_OVR) == 0U ||
            (ADC1->CFGR & ADC_CFGR_OVRMOD) != 0U) {
            ADC1->DR = data;
        }
        ADC1->ISR |= ADC_ISR_EOC;
        dma(data);
    }
    if ((ADC1->CFGR & (g_ag.injected ? ADC_CFGR_JAWD1EN : ADC_CFGR_AWD1EN)) !=
            0U &&
        (data < (ADC1->TR1 & 4095U) || data > (ADC1->TR1 >> 16U))) {
        ADC1->ISR |= ADC_ISR_AWD1;
    }
    ++g_ag.rank;
    if (g_ag.rank == g_ag.count) {
        ++g_ag.frames;
        ADC1->ISR |= g_ag.injected ? ADC_ISR_JEOS : ADC_ISR_EOS;
        event(g_ag.injected ? NX_AG_JEOS : NX_AG_EOS, g_ag.rank);
        const bool software = g_ag.injected
                                  ? (ADC1->JSQR & ADC_JSQR_JEXTEN) == 0U
                                  : (ADC1->CFGR & ADC_CFGR_EXTEN) == 0U;
        if (software && !g_ag.injected && (ADC1->CFGR & ADC_CFGR_CONT) != 0U &&
            !g_ag.limited) {
            g_ag.rank = 0U;
            schedule_rank(false);
            return;
        }
        /* Limited DMA resets the scan, but CONT1 is not a single-conversion
         * command. Its ADSTART lease still requires an actual ADSTP. */
        if (software && (g_ag.injected || (ADC1->CFGR & ADC_CFGR_CONT) == 0U)) {
            ADC1->CR &=
                ~(uint32_t)(g_ag.injected ? ADC_CR_JADSTART : ADC_CR_ADSTART);
        }
        g_ag.converting = false;
    } else if (g_ag.limited) {
        /* RM21.4.26: one-shot terminal count resets even a partial scan. */
        g_ag.converting = false;
    } else {
        schedule_rank(false);
    }
}
void update(bool software) {
    if ((TIM6->CR1 & TIM_CR1_UDIS) != 0U) {
        return;
    }
    g_ag.active_psc = TIM6->PSC;
    g_ag.active_arr = TIM6->ARR;
    TIM6->SR |= TIM_SR_UIF;
    if ((TIM6->CR2 & TIM_CR2_MMS) == TIM_CR2_MMS_1 ||
        (software && (TIM6->CR2 & TIM_CR2_MMS) == 0U)) {
        edge();
    }
}
} /* namespace */
extern "C" void nx_ag_reset(uint16_t revision) {
    std::memset(&g_ag, 0, sizeof(g_ag));
    g_ag.flash_kib = 512U;
    std::memset(&g_nx_stm32g474_system, 0, sizeof(g_nx_stm32g474_system));
    g_ag.privileged = true;
    g_ag.last_channel = 31U;
    for (unsigned ch = 0U; ch != 19U; ++ch) {
        g_ag.values[ch] = (uint16_t)(100U + ch * 31U);
    }
    DBGMCU->IDCODE = ((uint32_t)revision << 16U) | 0x469U;
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_ADC12EN;
    RCC->AHB1ENR = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;
    RCC->APB1ENR1 = RCC_APB1ENR1_TIM6EN;
    RCC->CCIPR = RCC_CCIPR_ADC12SEL_1;
    GPIOA->MODER = 15U;
    ADC1->CR = ADC_CR_DEEPPWD;
    ADC2->CR = ADC_CR_DEEPPWD;
    g_nx_stm32g474_system.rcc = RCC;
    g_nx_stm32g474_system.started = true;
    g_nx_stm32g474_system.core_hz = 170000000U;
}
extern "C" void nx_ag_advance(uint64_t cycles) {
    for (uint64_t i = 0U; i != cycles; ++i) {
        ++g_ag.cycles;
        if (g_ag.mask != 0U) {
            ++g_ag.masked_run;
            if (g_ag.masked_run > g_ag.max_masked_run) {
                g_ag.max_masked_run = g_ag.masked_run;
            }
        } else {
            g_ag.masked_run = 0U;
        }
        if ((ADC1->CR & ADC_CR_ADCAL) != 0U &&
            g_ag.cycles >= g_ag.calibration_due) {
            ADC1->CR &= ~(uint32_t)ADC_CR_ADCAL;
            g_ag.last_conversion = g_ag.cycles;
        }
        if ((ADC1->CR & ADC_CR_ADEN) != 0U &&
            g_ag.cycles - g_ag.enabled_at >= clock_divider()) {
            ADC1->ISR |= ADC_ISR_ADRDY;
        }
        if ((ADC1->CR & ADC_CR_ADSTP) != 0U && !g_ag.hold_adc_stop) {
            if (!g_ag.injected) {
                g_ag.converting = false;
            }
            ADC1->CR &= ~(uint32_t)(ADC_CR_ADSTART | ADC_CR_ADSTP);
            g_ag.needs_reenable = (DBGMCU->IDCODE >> 16U) == 0x2002U;
        }
        if ((ADC1->CR & ADC_CR_JADSTP) != 0U && !g_ag.hold_adc_stop) {
            if (g_ag.injected) {
                g_ag.converting = false;
            }
            ADC1->CR &= ~(uint32_t)(ADC_CR_JADSTART | ADC_CR_JADSTP);
            g_ag.needs_reenable = (DBGMCU->IDCODE >> 16U) == 0x2002U;
        }
        if ((ADC1->CR & ADC_CR_ADDIS) != 0U && !g_ag.hold_adc_disable &&
            (ADC1->CR & (ADC_CR_ADSTART | ADC_CR_JADSTART)) == 0U) {
            ADC1->CR &= ~(uint32_t)(ADC_CR_ADEN | ADC_CR_ADDIS);
        }
        if ((TIM6->CR1 & TIM_CR1_CEN) != 0U &&
            ++g_ag.divider > g_ag.active_psc) {
            g_ag.divider = 0U;
            if (TIM6->CNT >= g_ag.active_arr) {
                TIM6->CNT = 0U;
                update(false);
                if ((TIM6->CR1 & TIM_CR1_OPM) != 0U) {
                    TIM6->CR1 &= ~(uint32_t)TIM_CR1_CEN;
                }
            } else {
                ++TIM6->CNT;
            }
        }
        if (g_ag.trigger_pending && !g_ag.hold_trigger_sync &&
            g_ag.cycles >= g_ag.trigger_due) {
            g_ag.trigger_pending = false;
            synchronized_trigger();
        }
        if (g_ag.converting && !g_ag.hold_conversion &&
            g_ag.cycles >= g_ag.due) {
            complete_rank();
        }
        if ((DMA1->ISR & DMA_ISR_TCIF1) != 0U &&
            (DMA1_Channel1->CCR & DMA_CCR_TCIE) != 0U) {
            g_ag.pending[DMA1_Channel1_IRQn] = true;
        }
        if ((DMA1->ISR & DMA_ISR_TEIF1) != 0U &&
            (DMA1_Channel1->CCR & DMA_CCR_TEIE) != 0U) {
            g_ag.pending[DMA1_Channel1_IRQn] = true;
        }
        if ((ADC1->ISR & ADC1->IER &
             (ADC_ISR_OVR | ADC_ISR_JEOS | ADC_ISR_AWD1)) != 0U) {
            g_ag.pending[ADC1_2_IRQn] = true;
        }
    }
}
extern "C" uint32_t nx_ag_read(volatile uint32_t* reg) {
    if ((inside(reg, ADC1, sizeof(*ADC1)) || inside(reg, ADC2, sizeof(*ADC2)) ||
         inside(reg, ADC12_COMMON, sizeof(*ADC12_COMMON))) &&
        (RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN) == 0U) {
        ++g_ag.inaccessible_reads;
        return 0U;
    }
    if (g_ag.before_read != nullptr) {
        g_ag.before_read(reg);
    }
    const uint32_t value = *reg;
    if (reg == &ADC1->DR) {
        ADC1->ISR &= ~(uint32_t)ADC_ISR_EOC;
    }
    if (inside(reg, (const void*)&ADC1->JDR1, 16U)) {
        ADC1->ISR &= ~(uint32_t)ADC_ISR_JEOC;
    }
    return value;
}
extern "C" void nx_ag_write(volatile uint32_t* reg, uint32_t value) {
    ++g_ag.writes;
    if (g_ag.before_write != nullptr) {
        g_ag.before_write(reg, value);
    }
    const uint32_t old = *reg;
    if (reg == g_ag.refused) {
        value = (value & ~g_ag.held_bits) | (old & g_ag.held_bits);
    }
    if (reg == &ADC1->ISR) {
        ADC1->ISR &= ~value;
        return;
    }
    if (reg == &DMA1->IFCR) {
        DMA1->ISR &= ~((value & 1U) != 0U ? 15U : value & 14U);
        return;
    }
    if (reg == &TIM6->SR) {
        TIM6->SR &= value;
        return;
    }
    if (reg == &DMA1_Channel1->CCR && g_ag.hold_dma_disable &&
        (value & DMA_CCR_EN) == 0U) {
        value |= old & DMA_CCR_EN;
    }
    if (reg == &TIM6->CR1 && g_ag.hold_counter_stop &&
        (value & TIM_CR1_CEN) == 0U) {
        value |= old & TIM_CR1_CEN;
    }
    if (reg == &DMA1_Channel1->CMAR || reg == &DMA1_Channel1->CPAR ||
        reg == &DMA1_Channel1->CNDTR) {
        if ((DMA1_Channel1->CCR & DMA_CCR_EN) != 0U) {
            ++g_ag.forbidden_writes;
            return;
        }
    }
    if (reg == &ADC1->CR) {
        constexpr uint32_t commands =
            ADC_CR_ADEN | ADC_CR_ADDIS | ADC_CR_ADSTART | ADC_CR_JADSTART |
            ADC_CR_ADSTP | ADC_CR_JADSTP | ADC_CR_ADCAL;
        uint32_t admitted = value & ~old & commands;
        const bool enabled = (old & ADC_CR_ADEN) != 0U;
        const bool disabling = (old & ADC_CR_ADDIS) != 0U;
        const uint32_t starts = old & (ADC_CR_ADSTART | ADC_CR_JADSTART);
        const uint32_t illegal_starts =
            (!enabled || disabling)
                ? admitted & (ADC_CR_ADSTART | ADC_CR_JADSTART)
                : 0U;
        uint32_t illegal = illegal_starts;
        if (!enabled || disabling || (old & ADC_CR_ADSTART) == 0U) {
            illegal |= admitted & ADC_CR_ADSTP;
        }
        if (!enabled || disabling || (old & ADC_CR_JADSTART) == 0U ||
            (ADC1->CFGR & ADC_CFGR_JAUTO) != 0U) {
            illegal |= admitted & ADC_CR_JADSTP;
        }
        if (!enabled || starts != 0U) {
            illegal |= admitted & ADC_CR_ADDIS;
        }
        if (enabled || disabling) {
            illegal |= admitted & ADC_CR_ADCAL;
        }
        if ((old & ~(uint32_t)ADC_CR_ADVREGEN) != 0U ||
            (old & ADC_CR_ADVREGEN) == 0U) {
            illegal |= admitted & ADC_CR_ADEN;
        }
        if (illegal != 0U) {
            ++g_ag.forbidden_writes;
            admitted &= ~illegal;
        }
        /* These command/status bits cannot be cleared by an ordinary zero
         * store. Hardware completion/stop/disable advances them separately. */
        value = (value & ~commands) | (old & commands) | admitted;
    }
    if ((reg == &ADC1->SQR1 || reg == &ADC1->SQR2 || reg == &ADC1->SQR3 ||
         reg == &ADC1->SQR4) &&
        (ADC1->CR & ADC_CR_ADSTART) != 0U) {
        ++g_ag.forbidden_writes;
    }
    if (reg == &ADC1->JSQR && (ADC1->CR & ADC_CR_JADSTART) != 0U &&
        (ADC1->CFGR & ADC_CFGR_JQDIS) != 0U) {
        ++g_ag.forbidden_writes;
    }
    *reg = value;
    if (reg == &ADC12_COMMON->CCR) {
        const uint32_t bits[3]{ADC_CCR_VSENSESEL, ADC_CCR_VBATSEL,
                               ADC_CCR_VREFEN};
        for (unsigned i = 0U; i != 3U; ++i) {
            if ((value & bits[i]) != 0U && (old & bits[i]) == 0U) {
                g_ag.internal_enabled_at[i] = g_ag.cycles;
            }
        }
    }
    if (reg == &DMA1_Channel1->CNDTR) {
        g_ag.dma_initial = value;
    }
    if (reg == &ADC1->CR) {
        if ((value & ADC_CR_ADCAL) != 0U && (old & ADC_CR_ADCAL) == 0U) {
            g_ag.calibration_due = g_ag.cycles + 116U * clock_divider();
        }
        if ((value & ADC_CR_ADEN) != 0U && (old & ADC_CR_ADEN) == 0U) {
            g_ag.enabled_at = g_ag.cycles;
            g_ag.limited = false;
            g_ag.needs_reenable = false;
        }
        if ((value & ADC_CR_ADSTART) != 0U && (old & ADC_CR_ADSTART) == 0U &&
            (ADC1->CFGR & ADC_CFGR_EXTEN) == 0U) {
            start(false, false);
        }
        if ((value & ADC_CR_JADSTART) != 0U && (old & ADC_CR_JADSTART) == 0U &&
            (ADC1->JSQR & ADC_JSQR_JEXTEN) == 0U) {
            start(true, false);
        }
    }
    if (reg == &TIM6->EGR && (value & TIM_EGR_UG) != 0U) {
        TIM6->CNT = 0U;
        g_ag.divider = 0U;
        update(true);
    }
}
extern "C" void nx_ag_poll(void*) {
    nx_ag_advance(170U);
}
extern "C" nx_result_t nx_ag_irq_control(int irq, bool enable,
                                         uint8_t priority) {
    if (irq < 0 || irq >= 102 || priority >= 16U) {
        return NX_ERROR_INVALID;
    }
    if (!enable && g_ag.hold_irq_disable) {
        return NX_ERROR_IO;
    }
    g_ag.enabled[irq] = enable;
    if (enable) {
        g_ag.priority[irq] = priority;
    } else {
        g_ag.pending[irq] = false;
        if (g_ag.active[irq]) {
            return NX_ERROR_BUSY;
        }
    }
    return NX_SUCCESS;
}
extern "C" void nx_ag_priority_write(int irq, uint32_t priority) {
    if (irq >= 0 && irq < 102 && priority < 16U) {
        g_ag.priority[irq] = (uint8_t)priority;
    }
}
#ifndef NX_G474_ADC_GROUP_EXTERNAL_CORE
extern "C" bool nx_arch_is_privileged(void) {
    return g_ag.privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_ag.in_isr;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return g_ag.in_isr ? 16U : 0U;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_ag.mask != 0U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {g_ag.mask};
    g_ag.mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_ag.mask = previous.value;
}
extern "C" void nx_arch_dsb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" void nx_arch_isb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return g_ag.cycles / 170U;
}

#endif
