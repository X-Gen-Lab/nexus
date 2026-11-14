/**
 * \file            stm32_adc_cold_fixture.h
 * \brief           Shared ADC cold hardware model and retained fault injection
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32_ADC_COLD_FIXTURE_H
#define NX_STM32_ADC_COLD_FIXTURE_H
extern "C" {
#if defined(NX_ADC_FACTORY_CONTRACT)
#include "nexus_bindings.h"
#include "nexus_factory.h"
#endif
#include "stm32f407_control_cold.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
volatile uint32_t* refused;
bool refuse_restore;
bool refuse_powerdown;
bool privileged;
ADC_TypeDef* gated_foreign;
uint32_t gated_clock;
uint32_t foreign_control;
uint32_t refuse_probe_enable;
uint32_t refuse_probe_release;
unsigned starts;
unsigned stops;
nx_time_us_t now;
#if defined(NX_ADC_INTERNAL_CONTRACT)
bool fractional_clock;
uint64_t clock_ns;
uint64_t enabled_ns;
uint64_t conversion_ns;
bool injected_complete;
unsigned injected_launches;
uint32_t injected_sequence;
#endif
constexpr uint32_t original_ahb1 = RCC_AHB1ENR_GPIODEN;

class AdcColdModel : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        refused = nullptr;
        refuse_restore = false;
        refuse_powerdown = false;
        privileged = true;
        gated_foreign = nullptr;
        gated_clock = foreign_control = 0U;
        refuse_probe_enable = refuse_probe_release = 0U;
        starts = stops = 0U;
        now = 100U;
#if defined(NX_ADC_INTERNAL_CONTRACT)
        fractional_clock = false;
        clock_ns = enabled_ns = conversion_ns = 0U;
        injected_complete = true;
        injected_launches = 0U;
        injected_sequence = 0U;
#endif
        RCC->AHB1ENR = original_ahb1;
    }
    void TearDown() override {
        refused = nullptr;
        refuse_restore = false;
        refuse_powerdown = false;
        privileged = true;
        gated_foreign = nullptr;
        refuse_probe_enable = refuse_probe_release = 0U;
        if (mmio != MAP_FAILED) {
#if defined(NX_ADC_FACTORY_CONTRACT)
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
#endif
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
#if defined(NX_ADC_FACTORY_CONTRACT)
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(NX_SUCCESS, result.primary);
        ASSERT_EQ(NX_SUCCESS, result.cleanup);
        ASSERT_EQ(0U, result.remaining_effects);
    }
    void Restored() {
        EXPECT_EQ(original_ahb1, RCC->AHB1ENR);
        EXPECT_EQ(0U, RCC->APB2ENR);
        EXPECT_EQ(0U, GPIOA->MODER);
        EXPECT_EQ(0U, GPIOB->MODER);
        EXPECT_EQ(0U, GPIOC->MODER);
    }
#endif
};
} /* namespace */

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
#if defined(NX_ADC_INTERNAL_CONTRACT)
    if (fractional_clock && enabled_ns == 0U &&
        (ADC1->CR2 & ADC_CR2_ADON) != 0U) {
        /* Enable near a tick boundary; the public clock reports whole us. */
        clock_ns = ((clock_ns + 999U) / 1000U) * 1000U + 950U;
        enabled_ns = clock_ns;
    }
#endif
    if (refuse_restore && (GPIOB->MODER & 3U) == 0U)
        GPIOB->MODER |= 3U;
    if (refuse_powerdown && ADC1->CR2 == 0U)
        ADC1->CR2 = ADC_CR2_ADON;
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
#if defined(NX_ADC_INTERNAL_CONTRACT)
    if (fractional_clock) {
        clock_ns += 10U;
        now = clock_ns / 1000U;
        return now;
    }
#endif
    return ++now;
}
extern "C" void nx_stm32_adc_cold_model_poll(unsigned kind, void* raw) {
    EXPECT_EQ(5U, kind);
    auto* state = static_cast<nx_stm32_adc_state_t*>(raw);
    auto* adc = state->registers;
#if defined(NX_ADC_INTERNAL_CONTRACT)
    if ((adc->CR2 & ADC_CR2_JSWSTART) != 0U) {
        ++injected_launches;
        injected_sequence = adc->JSQR;
        if (fractional_clock && conversion_ns == 0U)
            conversion_ns = clock_ns;
        adc->CR2 &= ~static_cast<uint32_t>(ADC_CR2_JSWSTART);
        if (injected_complete) {
            const unsigned bits = 12U - 2U * state->resolution;
            const uint32_t mask = (1U << bits) - 1U;
            adc->JDR1 = 321U & mask;
            adc->JDR2 = 432U & mask;
            adc->JDR3 = 543U & mask;
            adc->JDR4 = 654U & mask;
            adc->SR |= ADC_SR_JEOC;
        }
    }
#endif
    if ((adc->CR2 & ADC_CR2_SWSTART) != 0U) {
#if defined(NX_ADC_INTERNAL_CONTRACT)
        if (fractional_clock && conversion_ns == 0U)
            conversion_ns = clock_ns;
#endif
        const unsigned bits = 12U - 2U * state->resolution;
        adc->DR = (100U + 100U * (adc->SQR3 & 31U)) & ((1U << bits) - 1U);
        adc->CR2 &= ~static_cast<uint32_t>(ADC_CR2_SWSTART);
        adc->SR |= ADC_SR_EOC;
    }
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    if (reg == &RCC->APB2ENR && gated_foreign != nullptr &&
        (*reg & gated_clock) != 0U)
        foreign_control = gated_foreign->CR2;
    if (reg == &RCC->APB2ENR) {
        value &= ~refuse_probe_enable;
        value |= *reg & refuse_probe_release;
    }
    if (reg != refused)
        *reg = value;
    if (reg == &RCC->APB2ENR && gated_foreign != nullptr)
        gated_foreign->CR2 = (*reg & gated_clock) != 0U ? foreign_control : 0U;
}
extern "C" nx_result_t nx_stm32_validate_identity(uint32_t) {
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t*) {
    return NX_SUCCESS;
}
extern "C" nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t*,
                                                         uint32_t) {
    ++starts;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++stops;
    return 0;
}

#endif /* NX_STM32_ADC_COLD_FIXTURE_H */
