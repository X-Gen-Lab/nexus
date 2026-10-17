/**
 * \file            g474_adc_group_factory_ports.cpp
 * \brief           External MMIO bridge, never replacement provider methods
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_adc_group_factory_model.h"
extern "C" {
bool g_ag_factory_refuse_priority;
}
namespace {
bool inside(volatile uint32_t* reg, const void* object, size_t bytes) {
    return (uintptr_t)reg >= (uintptr_t)object &&
           (uintptr_t)reg < (uintptr_t)object + bytes;
}
bool acquisition(volatile uint32_t* reg) {
    return inside(reg, ADC1, sizeof(*ADC1)) ||
           inside(reg, ADC2, sizeof(*ADC2)) ||
           inside(reg, ADC12_COMMON, sizeof(*ADC12_COMMON)) ||
           inside(reg, DMA1, sizeof(*DMA1)) ||
           inside(reg, DMA1_Channel1, sizeof(*DMA1_Channel1)) ||
           inside(reg, DMAMUX1_Channel0, sizeof(*DMAMUX1_Channel0)) ||
           inside(reg, TIM6, sizeof(*TIM6)) ||
           inside(reg, DBGMCU, sizeof(*DBGMCU));
}
} /* namespace */
extern "C" uint32_t nx_ag_factory_read(volatile uint32_t* reg) {
    return acquisition(reg) ? nx_ag_read(reg) : nx_g474_model_read(reg);
}
extern "C" void nx_ag_factory_write(volatile uint32_t* reg, uint32_t value) {
    if (acquisition(reg)) {
        nx_ag_write(reg, value);
    } else {
        nx_g474_model_write(reg, value);
    }
}
extern "C" void nx_ag_factory_advance(uint64_t microseconds) {
    nx_ag_advance(microseconds * 170U);
    if ((TIM2->CR1 & TIM_CR1_CEN) != 0U) {
        TIM2->CNT += (uint32_t)microseconds;
    }
    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0U &&
        (DMA1_Channel1->CCR & DMA_CCR_TCIE) != 0U) {
        g_g474_model.irq_pending[DMA1_Channel1_IRQn] = true;
    }
    if ((DMA1->ISR & DMA_ISR_TEIF1) != 0U &&
        (DMA1_Channel1->CCR & DMA_CCR_TEIE) != 0U) {
        g_g474_model.irq_pending[DMA1_Channel1_IRQn] = true;
    }
    if ((ADC1->ISR & ADC1->IER & (ADC_ISR_OVR | ADC_ISR_JEOS | ADC_ISR_AWD1)) !=
        0U) {
        g_g474_model.irq_pending[ADC1_2_IRQn] = true;
    }
}
extern "C" void nx_ag_factory_poll(void* context) {
    nx_g474_model_system_poll(context);
    nx_ag_factory_advance(1U);
}
extern "C" void nx_ag_factory_adc_poll(void* context) {
    (void)context;
    nx_ag_factory_advance(1U);
}
extern "C" void nx_ag_factory_priority_write(int irq, uint32_t priority) {
    if (!g_ag_factory_refuse_priority && irq >= 0 && irq < 102 &&
        priority < 16U) {
        g_g474_model.priority[irq] = (uint8_t)priority;
    }
}
