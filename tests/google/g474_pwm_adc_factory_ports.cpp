/**
 * \file            g474_pwm_adc_factory_ports.cpp
 * \brief           External event graph shared by actual generated assembly
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_pwm_adc_factory_model.h"
extern "C" {
bool g_pa_factory_refuse_priority;
bool g_pa_factory_refuse_comparator;
}
namespace {
bool inside(volatile uint32_t* address, const void* object, size_t bytes) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(object);
    return value >= begin && value < begin + bytes;
}
bool acquisition(volatile uint32_t* address) {
    return inside(address, TIM1, sizeof(*TIM1)) ||
           inside(address, TIM15, sizeof(*TIM15)) ||
           inside(address, COMP1, sizeof(*COMP1)) ||
           inside(address, ADC1, sizeof(*ADC1)) ||
           inside(address, ADC2, sizeof(*ADC2)) ||
           inside(address, ADC12_COMMON, sizeof(*ADC12_COMMON)) ||
           inside(address, DMA1, sizeof(*DMA1)) ||
           inside(address, DMA1_Channel1, sizeof(*DMA1_Channel1)) ||
           inside(address, DMAMUX1_Channel0, sizeof(*DMAMUX1_Channel0));
}
} /* namespace */
extern "C" uint32_t nx_pa_factory_read(volatile uint32_t* address) {
    return acquisition(address) ? nx_pa_read(address)
                                : nx_g474_model_read(address);
}
extern "C" void nx_pa_factory_write(volatile uint32_t* address,
                                    uint32_t value) {
    if (address == &COMP1->CSR && g_pa_factory_refuse_comparator &&
        (COMP1->CSR & COMP_CSR_EN) != 0U && (value & COMP_CSR_EN) == 0U) {
        ++g_pa.writes;
        return;
    }
    if (acquisition(address)) {
        nx_pa_write(address, value);
    } else {
        nx_g474_model_write(address, value);
    }
}
extern "C" void nx_pa_factory_advance(uint64_t microseconds) {
    nx_pa_advance_cycles(microseconds * 170U);
    if ((TIM2->CR1 & TIM_CR1_CEN) != 0U) {
        /* Tests advance bounded intervals below a single 32-bit wrap. */
        TIM2->CNT += (uint32_t)microseconds;
    }
    if (((DMA1_Channel1->CCR & DMA_CCR_TCIE) != 0U &&
         (DMA1->ISR & DMA_ISR_TCIF1) != 0U) ||
        ((DMA1_Channel1->CCR & DMA_CCR_TEIE) != 0U &&
         (DMA1->ISR & DMA_ISR_TEIF1) != 0U)) {
        g_g474_model.irq_pending[DMA1_Channel1_IRQn] = true;
    }
    if ((TIM1->DIER & TIM_DIER_BIE) != 0U && (TIM1->SR & TIM_SR_BIF) != 0U) {
        g_g474_model.irq_pending[TIM1_BRK_TIM15_IRQn] = true;
    }
}
extern "C" void nx_pa_factory_poll(void* context) {
    nx_g474_model_system_poll(context);
    nx_pa_factory_advance(1U);
}
extern "C" void nx_pa_factory_adc_poll(void* context) {
    (void)context;
    nx_pa_factory_advance(1U);
}
extern "C" void nx_pa_factory_priority_write(int irq, uint32_t priority) {
    if (!g_pa_factory_refuse_priority && irq >= 0 && irq < 102 &&
        priority < 16U) {
        g_g474_model.priority[irq] = (uint8_t)priority;
    }
}
