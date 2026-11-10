/**
 * \file            stm32_adc_internal_factory_test.cpp
 * \brief           Existing pinless ADC selection and pre-System admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_clock.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
namespace {
unsigned starts;
unsigned stops;
unsigned gpio_writes;
unsigned clock_writes;
class InternalAdcFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    std::array<std::array<unsigned char, sizeof(GPIO_TypeDef)>, 9U> gpio_before;

    void NoGpioEffects() {
        EXPECT_EQ(gpio_writes, 0U);
        EXPECT_EQ(clock_writes, 0U);
        EXPECT_EQ(RCC->AHB1ENR, 0U);
        for (size_t bank = 0U; bank < gpio_before.size(); ++bank) {
            const auto* registers = reinterpret_cast<const void*>(
                GPIOA_BASE + bank * UINT32_C(0x400));
            EXPECT_EQ(std::memcmp(gpio_before[bank].data(), registers,
                                  gpio_before[bank].size()),
                      0);
        }
    }
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mmio, MAP_FAILED);
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        starts = stops = gpio_writes = clock_writes = 0U;
        for (size_t bank = 0U; bank < gpio_before.size(); ++bank) {
            auto* registers =
                reinterpret_cast<void*>(GPIOA_BASE + bank * UINT32_C(0x400));
            std::memset(registers, 0xA5, gpio_before[bank].size());
            std::memcpy(gpio_before[bank].data(), registers,
                        gpio_before[bank].size());
        }
    }
    void TearDown() override {
        if (mmio != MAP_FAILED) {
            EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
            NoGpioEffects();
            EXPECT_EQ(munmap(mmio, 0x80000U), 0);
        }
    }
};
TEST_F(InternalAdcFactory, GetterIsPureAndHasNoGpio) {
    ASSERT_NE(nx_factory_adc(NX_ADC_ID_INTERNAL), nullptr);
    EXPECT_EQ(starts, 0U);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
}
TEST_F(InternalAdcFactory, HealthyPlatformOwnsAndClosesTheInternalPath) {
    const auto result = nx_platform_start();
    ASSERT_EQ(result.primary, NX_SUCCESS);
    ASSERT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(starts, 1U);
    EXPECT_EQ(RCC->AHB1ENR, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_ADC1EN, 0U);
    EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(stops, 1U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
}
TEST_F(InternalAdcFactory, ForeignAdcRejectsBeforeSystemClockEffects) {
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    RCC->APB2ENR = RCC_APB2ENR_ADC1EN;
    ADC1->CR2 = ADC_CR2_ADON;
    std::array<unsigned char, sizeof(RCC_TypeDef)> clock_before;
    std::array<unsigned char, sizeof(ADC_TypeDef)> adc_before;
    std::memcpy(clock_before.data(), RCC, clock_before.size());
    std::memcpy(adc_before.data(), ADC1, adc_before.size());
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(starts, 0U);
    EXPECT_EQ(stops, 0U);
    EXPECT_EQ(std::memcmp(clock_before.data(), RCC, clock_before.size()), 0);
    EXPECT_EQ(std::memcmp(adc_before.data(), ADC1, adc_before.size()), 0);
    EXPECT_EQ(ADC1->CR2, (uint32_t)ADC_CR2_ADON);
    EXPECT_EQ(RCC->APB2ENR, (uint32_t)RCC_APB2ENR_ADC1EN);
}
} /* namespace */

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
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_f4_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* target,
                                             uint32_t value) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(target);
    if (address >= GPIOA_BASE && address < GPIOI_BASE + sizeof(GPIO_TypeDef))
        ++gpio_writes;
    if (target == &RCC->AHB1ENR)
        ++clock_writes;
    *target = value;
}
extern "C" void nx_stm32_adc_cold_model_poll(unsigned kind, void* port) {
    (void)kind;
    (void)port;
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
    g_nx_stm32_system.started = true;
    g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
    ++g_nx_stm32_system.time_epoch;
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
    RCC->CFGR = NEXUS_CLOCK_CFGR | RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL;
    return {0, 0, 0U};
}
extern "C" int nx_stm32_system_stop(nx_stm32_system_t*, uint32_t) {
    ++stops;
    g_nx_stm32_system.started = false;
    return 0;
}
