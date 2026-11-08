/**
 * \file            stm32_adc_cold_factory_test.cpp
 * \brief           Actual multi-bank ADC factory and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
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
unsigned starts;
unsigned stops;
nx_time_us_t now;
constexpr uint32_t original_ahb1 = RCC_AHB1ENR_GPIODEN;

class AdcColdFactory : public testing::Test {
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
        starts = stops = 0U;
        now = 100U;
        RCC->AHB1ENR = original_ahb1;
    }
    void TearDown() override {
        refused = nullptr;
        refuse_restore = false;
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
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
};
} /* namespace */

TEST_F(AdcColdFactory, LookupIsPureAndStopRestoresAllThreeBanks) {
    const auto* port = nx_factory_adc(static_cast<nx_adc_id_t>(0));
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(0U, starts);
    Restored();
    Start();
    nx_adc_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_info(port, &info));
    EXPECT_EQ(16U, info.channel_count);
    EXPECT_EQ(NX_TEST_ADC_BITS, info.resolution_bits);
    EXPECT_EQ((12U - NX_TEST_ADC_BITS) / 2U,
              (ADC1->CR1 & ADC_CR1_RES) >> ADC_CR1_RES_Pos);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, starts);
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedAdcShutdownKeepsEveryPadAndTheSystemAlive) {
    Start();
    ADC1->CR2 = ADC_CR2_ADON;
    refused = &ADC1->CR2;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0xfU, GPIOB->MODER);
    EXPECT_EQ(0xfffU, GPIOC->MODER);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

TEST_F(AdcColdFactory, RefusedBankRestoreRetainsClocksAndRetriesRemainingPads) {
    Start();
    refuse_restore = true;
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_ADC1EN);
    EXPECT_EQ(0U, stops);
    EXPECT_EQ(3U, GPIOB->MODER & 3U);
    EXPECT_EQ(0xffffU, GPIOA->MODER);
    EXPECT_EQ(0U, GPIOC->MODER);
    EXPECT_EQ(7U, RCC->AHB1ENR & 7U);
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    refuse_restore = false;
    ASSERT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(1U, stops);
    Restored();
}

extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
    if (refuse_restore && (GPIOB->MODER & 3U) == 0U)
        GPIOB->MODER |= 3U;
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
    return ++now;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    if (reg != refused)
        *reg = value;
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
