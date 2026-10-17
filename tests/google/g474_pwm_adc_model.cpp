/**
 * \file            g474_pwm_adc_model.cpp
 * \brief           External register/event graph for finite joint acquisition
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_adc_model.h"
#include "nexus/core/time.h"
#include "stm32g474_system.h"
#include <atomic>
#include <cstring>
extern "C" {
nx_pa_model_t g_pa;
#ifndef NX_G474_PWM_ADC_EXTERNAL_CORE
nx_stm32g474_system_t g_nx_stm32g474_system;
#endif
}
namespace {
constexpr uint64_t kernel_per_us = 170U;
constexpr uint64_t adc_per_cycle = 128U;
constexpr uint64_t conversion_cycles = 15U * adc_per_cycle;
void event(nx_pa_event_t kind) {
    if (g_pa.event != nullptr) {
        g_pa.event(kind);
    }
}
bool inside(const volatile uint32_t* p, const void* base, size_t bytes) {
    return (uintptr_t)p >= (uintptr_t)base &&
           (uintptr_t)p < (uintptr_t)base + bytes;
}
void complete_adc() {
    g_pa.converting = false;
    uint16_t data = g_pa.analog_value;
    if (g_pa.software_conversion) {
        ++g_pa.primes;
        data = (g_pa.primes & 1U) != 0U ? 0xBADU : 0xBEEU;
        ADC1->CR &= ~(uint32_t)ADC_CR_ADSTART;
        event(NX_PA_PRIME_EOS);
    }
    if (!g_pa.software_conversion && (ADC1->ISR & ADC_ISR_EOC) != 0U) {
        ADC1->ISR |= ADC_ISR_OVR;
        if ((ADC1->CFGR & ADC_CFGR_OVRMOD) == 0U) {
            return;
        }
    }
    ADC1->DR = data;
    ADC1->ISR |= ADC_ISR_EOC | ADC_ISR_EOS;
    if ((ADC1->CFGR & ADC_CFGR_DMAEN) == 0U ||
        (DMA1_Channel1->CCR & DMA_CCR_EN) == 0U || DMA1_Channel1->CNDTR == 0U ||
        g_pa.hold_dma_beat || (ADC1->ISR & ADC_ISR_OVR) != 0U) {
        return;
    }
    if (DMAMUX1_Channel0->CCR != 5U ||
        DMA1_Channel1->CPAR != (uint32_t)(uintptr_t)&ADC1->DR) {
        DMA1->ISR |= DMA_ISR_TEIF1 | DMA_ISR_GIF1;
        DMA1_Channel1->CCR &= ~(uint32_t)DMA_CCR_EN;
        if ((DMA1_Channel1->CCR & DMA_CCR_TEIE) != 0U) {
            g_pa.irq_pending[DMA1_Channel1_IRQn] = true;
        }
        return;
    }
    auto* target = reinterpret_cast<uint16_t*>((uintptr_t)DMA1_Channel1->CMAR);
    size_t index = g_pa.dma_initial - DMA1_Channel1->CNDTR;
    target[index] = data;
    --DMA1_Channel1->CNDTR;
    ++g_pa.beats;
    ADC1->ISR &= ~(uint32_t)ADC_ISR_EOC;
    event(NX_PA_DMA_WRITE);
    if (DMA1_Channel1->CNDTR == 0U) {
        DMA1->ISR |= DMA_ISR_TCIF1 | DMA_ISR_GIF1;
        if ((DMA1_Channel1->CCR & DMA_CCR_TCIE) != 0U) {
            g_pa.irq_pending[DMA1_Channel1_IRQn] = true;
        }
        if ((ADC1->CFGR & ADC_CFGR_DMACFG) == 0U) {
            /* RM641: no further request, DR freeze, scan reset; ADSTART is
             * deliberately not invented as a cleared software command. */
            g_pa.limited_complete = true;
        }
    }
}
void edge() {
    ++g_pa.edges;
    event(NX_PA_TIMER_EDGE);
    if ((ADC1->CR & (ADC_CR_ADEN | ADC_CR_ADSTART)) !=
            (ADC_CR_ADEN | ADC_CR_ADSTART) ||
        (ADC1->CFGR & ADC_CFGR_EXTEN) != ADC_CFGR_EXTEN_0 ||
        ((ADC1->CFGR & ADC_CFGR_EXTSEL) >> ADC_CFGR_EXTSEL_Pos) != 10U ||
        g_pa.limited_complete) {
        return;
    }
    if (g_pa.converting) {
        ++g_pa.ignored_edges;
        return;
    }
    g_pa.converting = true;
    g_pa.software_conversion = false;
    /* DS asynchronous trigger latency chooses conservative2.5cycles.
     * SMP2.5 + conversion12.5 + latency2.5 =17.5 ADCcycles. */
    g_pa.conversion_done = g_pa.cycles + conversion_cycles + 320U;
}
bool output_trigger() {
    const uint32_t selection = (TIM1->CR2 & TIM_CR2_MMS2) >> 20U;
    if (selection == 1U) {
        return (TIM1->CR1 & TIM_CR1_CEN) != 0U;
    }
    if (selection == 7U && ((TIM1->CCMR2 >> 12U) & 7U) == 7U) {
        return TIM1->CNT >= g_pa.active_ccr[3];
    }
    return false;
}
void update_trigger() {
    bool next = output_trigger();
    if (!g_pa.trgo && next) {
        edge();
    }
    g_pa.trgo = next;
}
void update_event() {
    if ((TIM1->CR1 & TIM_CR1_UDIS) != 0U) {
        return;
    }
    g_pa.active_psc = TIM1->PSC;
    g_pa.active_arr = TIM1->ARR;
    g_pa.active_ccr[0] = TIM1->CCR1;
    g_pa.active_ccr[1] = TIM1->CCR2;
    g_pa.active_ccr[2] = TIM1->CCR3;
    g_pa.active_ccr[3] = TIM1->CCR4;
    TIM1->SR |= TIM_SR_UIF;
}
} /* namespace */
extern "C" void nx_pa_reset(void) {
    std::memset(&g_pa, 0, sizeof(g_pa));
    std::memset(&g_nx_stm32g474_system, 0, sizeof(g_nx_stm32g474_system));
    g_pa.privileged = true;
    g_pa.analog_value = 0x321U;
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_ADC12EN;
    RCC->AHB1ENR = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;
    RCC->APB2ENR = RCC_APB2ENR_TIM1EN | RCC_APB2ENR_SYSCFGEN;
    RCC->CCIPR = RCC_CCIPR_ADC12SEL_1;
    GPIOA->MODER = 3U | (3U << 2U) | (3U << 8U) | (42U << 16U);
    GPIOA->AFR[1] = 0x666U;
    COMP1->CSR = COMP_CSR_EN | COMP_CSR_INMSEL_2 | COMP_CSR_INMSEL_1;
    ADC1->CR = ADC_CR_DEEPPWD;
    ADC2->CR = ADC_CR_DEEPPWD;
    g_nx_stm32g474_system.rcc = RCC;
    g_nx_stm32g474_system.started = true;
    g_nx_stm32g474_system.core_hz = 170000000U;
}
extern "C" void nx_pa_advance_cycles(uint64_t count) {
    for (uint64_t i = 0U; i != count; ++i) {
        ++g_pa.cycles;
        if ((ADC1->CR & ADC_CR_ADCAL) != 0U &&
            g_pa.cycles >= g_pa.calibration_done) {
            ADC1->CR &= ~(uint32_t)ADC_CR_ADCAL;
        }
        if ((ADC1->CR & ADC_CR_ADEN) != 0U &&
            g_pa.cycles - g_pa.enabled_at >= adc_per_cycle) {
            ADC1->ISR |= ADC_ISR_ADRDY;
        }
        if ((ADC1->CR & ADC_CR_ADSTP) != 0U && !g_pa.hold_adc_stop) {
            ADC1->CR &= ~(uint32_t)(ADC_CR_ADSTART | ADC_CR_ADSTP);
            g_pa.converting = false;
        }
        if ((ADC1->CR & ADC_CR_ADDIS) != 0U && !g_pa.hold_adc_disable &&
            (ADC1->CR & ADC_CR_ADSTART) == 0U) {
            ADC1->CR &= ~(uint32_t)(ADC_CR_ADEN | ADC_CR_ADDIS);
        }
        if ((TIM1->CR1 & TIM_CR1_CEN) != 0U &&
            ++g_pa.counter_divider > g_pa.active_psc) {
            g_pa.counter_divider = 0U;
            if (TIM1->CNT >= g_pa.active_arr) {
                TIM1->CNT = 0U;
                update_event();
            } else {
                ++TIM1->CNT;
            }
            update_trigger();
        }
        if (g_pa.converting && !g_pa.hold_conversion &&
            g_pa.cycles >= g_pa.conversion_done) {
            complete_adc();
        }
    }
}
extern "C" void nx_pa_break(bool asserted) {
    if (asserted) {
        COMP1->CSR |= COMP_CSR_VALUE;
        if ((COMP1->CSR & COMP_CSR_EN) != 0U &&
            (TIM1->AF1 & TIM1_AF1_BKCMP1E) != 0U &&
            (TIM1->BDTR & TIM_BDTR_BKE) != 0U) {
            TIM1->BDTR &= ~(uint32_t)TIM_BDTR_MOE;
            TIM1->SR |= TIM_SR_BIF;
            if ((TIM1->DIER & TIM_DIER_BIE) != 0U) {
                g_pa.irq_pending[TIM1_BRK_TIM15_IRQn] = true;
            }
        }
    } else {
        COMP1->CSR &= ~(uint32_t)COMP_CSR_VALUE;
    }
}
extern "C" uint32_t nx_pa_read(volatile uint32_t* reg) {
    if ((inside(reg, ADC1, sizeof(*ADC1)) || inside(reg, ADC2, sizeof(*ADC2)) ||
         inside(reg, ADC12_COMMON, sizeof(*ADC12_COMMON))) &&
        (RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN) == 0U) {
        ++g_pa.unsupported_reads;
        return 0U;
    }
    if (inside(reg, TIM1, sizeof(*TIM1)) &&
        (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) == 0U) {
        ++g_pa.unsupported_reads;
        return 0U;
    }
    if (inside(reg, COMP1, sizeof(*COMP1)) &&
        (RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) == 0U) {
        ++g_pa.unsupported_reads;
        return 0U;
    }
    const uint32_t value = *reg;
    if (g_pa.after_read != nullptr) {
        g_pa.after_read(reg);
    }
    return value;
}
extern "C" void nx_pa_write(volatile uint32_t* reg, uint32_t value) {
    ++g_pa.writes;
    const uint32_t previous = *reg;
    if (reg == g_pa.refused) {
        value = (value & ~g_pa.held_bits) | (previous & g_pa.held_bits);
    }
    if (reg == &ADC1->ISR) {
        ADC1->ISR &= ~value;
        return;
    }
    if (reg == &TIM1->SR) {
        TIM1->SR &= value;
        return;
    }
    if (reg == &DMA1->IFCR) {
        g_pa.global_clear_while_active |=
            (value & DMA_IFCR_CGIF1) != 0U &&
            (DMA1_Channel1->CCR & DMA_CCR_EN) != 0U;
        DMA1->ISR &= ~(value & 14U);
        if ((value & DMA_IFCR_CGIF1) != 0U) {
            DMA1->ISR &= ~15U;
        }
        if ((DMA1->ISR & 14U) == 0U) {
            DMA1->ISR &= ~(uint32_t)DMA_ISR_GIF1;
        }
        return;
    }
    if (reg == &DMA1_Channel1->CCR && g_pa.hold_dma_disable &&
        (value & DMA_CCR_EN) == 0U) {
        value |= previous & DMA_CCR_EN;
    }
    if (g_pa.hold_addresses && value == 0U &&
        (reg == &DMA1_Channel1->CMAR || reg == &DMA1_Channel1->CPAR)) {
        value = previous;
    }
    if (reg == &TIM1->CR1 && g_pa.hold_counter_stop &&
        (value & TIM_CR1_CEN) == 0U) {
        value |= previous & TIM_CR1_CEN;
    }
    if (reg == &COMP1->CSR) {
        value =
            (value & ~(uint32_t)COMP_CSR_VALUE) | (previous & COMP_CSR_VALUE);
    }
    *reg = value;
    if (reg == &DMA1_Channel1->CNDTR) {
        g_pa.dma_initial = value;
    }
    if (reg == &ADC1->CR) {
        if ((value & ADC_CR_ADCAL) != 0U && (previous & ADC_CR_ADCAL) == 0U) {
            g_pa.calibration_done = g_pa.cycles + 116U * adc_per_cycle;
        }
        if ((value & ADC_CR_ADEN) != 0U && (previous & ADC_CR_ADEN) == 0U) {
            ++g_pa.enables;
            g_pa.enabled_at = g_pa.cycles;
            g_pa.limited_complete = false;
        }
        if ((value & ADC_CR_ADSTART) != 0U &&
            (previous & ADC_CR_ADSTART) == 0U &&
            (ADC1->CFGR & ADC_CFGR_EXTEN) == 0U) {
            g_pa.software_conversion = true;
            g_pa.converting = true;
            g_pa.conversion_done = g_pa.cycles + conversion_cycles;
        }
    }
    if (reg == &TIM1->EGR && (value & TIM_EGR_UG) != 0U) {
        TIM1->CNT = 0U;
        g_pa.counter_divider = 0U;
        update_event();
        if ((TIM1->CR2 & TIM_CR2_MMS2) == 0U) {
            edge();
        }
    }
    if (reg == &TIM1->CR1 || reg == &TIM1->CR2 || reg == &TIM1->EGR) {
        if (reg == &TIM1->CR1 && (previous & TIM_CR1_CEN) == 0U &&
            (value & TIM_CR1_CEN) != 0U) {
            event(NX_PA_COUNTER_STARTED);
        }
        update_trigger();
    }
}
extern "C" void nx_pa_poll(void*) {
    if (g_pa.poll != nullptr) {
        g_pa.poll();
    }
    nx_pa_advance_cycles(kernel_per_us);
}
extern "C" nx_result_t nx_pa_irq_control(int irq, bool enabled,
                                         uint8_t priority) {
    if (irq < 0 || irq >= 102 || priority >= 16U) {
        return NX_ERROR_INVALID;
    }
    g_pa.irq_enabled[irq] = enabled;
    if (enabled) {
        g_pa.priority[irq] = priority;
    } else {
        g_pa.irq_pending[irq] = false;
        if (g_pa.irq_active[irq]) {
            return NX_ERROR_BUSY;
        }
    }
    return NX_SUCCESS;
}
extern "C" void nx_pa_priority_write(int irq, uint32_t priority) {
    if (irq >= 0 && irq < 102 && priority < 16U) {
        g_pa.priority[irq] = (uint8_t)priority;
    }
}
#ifndef NX_G474_PWM_ADC_EXTERNAL_CORE
extern "C" bool nx_arch_is_privileged(void) {
    return g_pa.privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_pa.in_isr;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return g_pa.in_isr ? 16U : 0U;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_pa.mask != 0U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {g_pa.mask};
    g_pa.mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_pa.mask = previous.value;
}
extern "C" void nx_arch_dsb(void) {
    /* Synchronous MMIO model: compiler ordering, no thread publication. */
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" void nx_arch_isb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return g_pa.cycles / kernel_per_us;
}

#endif
