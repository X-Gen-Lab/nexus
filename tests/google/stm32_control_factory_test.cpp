/**
 * \file            stm32_control_factory_test.cpp
 * \brief           Actual generated ADC and PWM cold ownership failures
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32f407_control_cold.h"
#include "stm32f407_provider.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_gd32_model_nvic;
SCB_Type g_gd32_model_scb;
SysTick_Type g_gd32_model_systick;
uint32_t g_gd32_model_mask;
bool g_gd32_model_isr;
#ifdef NX_CONTROL_EXTI
void EXTI9_5_IRQHandler(void);
#endif
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
bool privileged;
volatile uint32_t* refused;
uint32_t held_bits;
unsigned starts;
unsigned stops;
unsigned unpowered_adc_writes;
int refused_priority;
int refused_enable;
bool pend_first_line;
nx_time_us_t now;
class ControlFactory : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_gd32_model_nvic = {};
        g_gd32_model_scb = {};
        g_gd32_model_systick = {};
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        privileged = true;
        refused = nullptr;
        held_bits = starts = stops = 0U;
        unpowered_adc_writes = 0U;
        refused_priority = refused_enable = -1;
        pend_first_line = false;
        now = 100U;
    }
    void TearDown() override {
        privileged = true;
        g_gd32_model_mask = 0U;
        g_gd32_model_isr = false;
        refused = nullptr;
        refused_priority = refused_enable = -1;
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
    volatile uint32_t* Gate() {
#ifdef NX_CONTROL_PWM
        return &RCC->APB1ENR;
#else
        return &RCC->APB2ENR;
#endif
    }
    uint32_t Mask() {
#ifdef NX_CONTROL_PWM
        return RCC_APB1ENR_TIM3EN;
#elif defined(NX_CONTROL_EXTI)
        return RCC_APB2ENR_SYSCFGEN;
#else
        return RCC_APB2ENR_ADC1EN;
#endif
    }
};
} /* namespace */

TEST_F(ControlFactory, ExactStaticFactoryStartsStopsAndRestarts) {
#ifdef NX_CONTROL_PWM
    const nx_pwm_port_t* port = nx_factory_pwm(NX_PWM_ID_PWM0);
#elif defined(NX_CONTROL_EXTI)
    const nx_exti_port_t* port = nx_factory_exti(NX_EXTI_ID_EDGE5);
#else
    const nx_adc_port_t* port = nx_factory_adc(NX_ADC_ID_ADC0);
#endif
    ASSERT_NE(nullptr, port);
    EXPECT_EQ(0U, *Gate());
    EXPECT_EQ(0U, starts);
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOCEN;
    Start();
    EXPECT_NE(0U, *Gate() & Mask());
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, *Gate() & Mask());
    EXPECT_EQ(RCC_AHB1ENR_GPIOCEN, RCC->AHB1ENR);
    Start();
}

TEST_F(ControlFactory, RejectedForeignControllerCannotBeStoppedOrReleased) {
    *Gate() |= Mask();
#ifdef NX_CONTROL_PWM
    nx_stm32_gpio_state_t inactive = {};
    inactive.registers = GPIOA;
    inactive.mask = 1U << 6U;
    inactive.output = inactive.initialized = true;
    nx_stm32_pwm_state_t state = {};
    state.registers = TIM3;
    state.inactive_gpio = &inactive;
    state.inactive_mask = 1U << 6U;
    state.initialized = true;
    const nx_stm32_pwm_config_t config = {1000U, 0U, 1000000U, 83U, false};
#elif defined(NX_CONTROL_EXTI)
    nx_exti_event_t events[2] = {};
    nx_stm32_exti_state_t state = {};
    state.registers = EXTI;
    state.gpio = GPIOA;
    state.storage = events;
    state.capacity = 2U;
    state.line = 5U;
    state.edge = NX_EXTI_BOTH;
    state.initialized = true;
    EXTI->IMR = EXTI->RTSR = EXTI->FTSR = 1U << 5U;
    const nx_stm32_exti_config_t config = {GPIOA, events, 2U, NX_EXTI_BOTH,
                                           0U,    5U,     5U};
#else
    const uint8_t channels[] = {0U};
    const uint8_t sample_times[] = {2U};
    nx_stm32_adc_state_t state = {};
    state.registers = ADC1;
    state.common = ADC;
    state.channels = channels;
    state.sample_times = sample_times;
    state.channel_count = 1U;
    state.reference_mv = 3300U;
    state.initialized = true;
    const nx_stm32_adc_config_t config = {channels, sample_times, 1U, 3300U};
#endif
    const auto original = state;
    const auto* bytes = static_cast<const uint8_t*>(mmio);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    const NVIC_Type before_nvic = *NVIC;
#ifdef NX_CONTROL_PWM
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pwm_prepare(&state, &inactive, &config));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pwm_close(&state, &inactive));
#elif defined(NX_CONTROL_EXTI)
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_exti_prepare(&state, &config));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_exti_close(&state));
#else
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_prepare(&state, &config));
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_adc_close(&state));
#endif
    EXPECT_EQ(0, std::memcmp(&state, &original, sizeof(original)));
    EXPECT_EQ(0, std::memcmp(bytes, before.data(), before.size()));
    EXPECT_EQ(0, std::memcmp(NVIC, &before_nvic, sizeof(before_nvic)));
    *Gate() &= ~Mask();
}

TEST_F(ControlFactory, RefusedPeripheralGateCannotPublishReady) {
    refused = Gate();
    held_bits = Mask();
    const auto result = nx_platform_start();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, *Gate() & Mask());
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
    EXPECT_EQ(0U, unpowered_adc_writes);
    refused = nullptr;
    Start();
}

TEST_F(ControlFactory, RefusedGateReleaseRetainsPlatformForRetry) {
    Start();
    refused = Gate();
    held_bits = Mask();
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_NE(0U, *Gate() & Mask());
#ifdef NX_CONTROL_EXTI
    EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
#else
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
#endif
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, *Gate() & Mask());
    Start();
}

#ifndef NX_CONTROL_EXTI
TEST_F(ControlFactory, LostOwnedClockRetainsStateUntilExternalRecovery) {
    Start();
    *Gate() &= ~Mask();
    EXPECT_EQ(NX_ERROR_IO, nx_platform_stop());
    EXPECT_EQ(NX_ERROR_STATE, nx_platform_start().primary);
    EXPECT_NE(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
    *Gate() |= Mask();
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, *Gate() & Mask());
    Start();
}
#endif

#ifdef NX_CONTROL_EXTI
TEST_F(ControlFactory, NewSharedLineHintSurvivesBothPublications) {
    pend_first_line = true;
    Start();
    const unsigned irq = (unsigned)EXTI9_5_IRQn;
    EXPECT_NE(0U, NVIC->ISPR[irq / 32U] & (1U << (irq % 32U)));
    EXPECT_NE(0U, NVIC->ISER[irq / 32U] & (1U << (irq % 32U)));
    EXPECT_EQ((1U << 5U) | (1U << 6U), EXTI->IMR & 0x60U);
    EXPECT_EQ(NX_SUCCESS, nx_platform_stop());
    EXPECT_EQ(0U, NVIC->ISPR[irq / 32U] & (1U << (irq % 32U)));
    EXPECT_EQ(0U, NVIC->ISER[irq / 32U] & (1U << (irq % 32U)));
}

TEST_F(ControlFactory, PublisherPriorityOrEnableRefusalRetainsNoFalseReady) {
    for (unsigned refusal = 0U; refusal < 2U; ++refusal) {
        refused_priority = refusal == 0U ? (int)EXTI9_5_IRQn : -1;
        refused_enable = refusal == 1U ? (int)EXTI9_5_IRQn : -1;
        const auto result = nx_platform_start();
        EXPECT_EQ(NX_ERROR_IO, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, EXTI->IMR & 0x60U);
        EXPECT_EQ(0U, RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN);
        refused_priority = refused_enable = -1;
    }
    Start();
}

TEST_F(ControlFactory, ActualSharedVectorPublishesToItsExactPublicFace) {
    Start();
    GPIOA->IDR = 1U << 5U;
    EXTI->PR = 1U << 5U;
    EXTI9_5_IRQHandler();
    nx_exti_event_t event{};
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_read(nx_factory_exti(NX_EXTI_ID_EDGE5),
                                            &event, 1U, &count));
    ASSERT_EQ(1U, count);
    EXPECT_EQ(5U, event.line);
    EXPECT_EQ(NX_EXTI_RISING, event.edge);
    EXPECT_EQ(0U, EXTI->PR & (1U << 5U));
    EXPECT_EQ(NX_ERROR_EMPTY,
              nx_exti_port_read(nx_factory_exti(NX_EXTI_ID_EDGE6), &event, 1U,
                                &count));
}
#endif

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{g_gd32_model_mask};
    g_gd32_model_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    g_gd32_model_mask = previous.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++now;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    if (reg == &ADC1->CR2 && (RCC->APB2ENR & RCC_APB2ENR_ADC1EN) == 0U)
        ++unpowered_adc_writes;
    if (reg == refused)
        value = (value & ~held_bits) | (*reg & held_bits);
    *reg = value;
    if (pend_first_line && reg == &EXTI->IMR && (value & (1U << 5U)) != 0U) {
        const unsigned irq = (unsigned)EXTI9_5_IRQn;
        NVIC->ISPR[irq / 32U] |= 1U << (irq % 32U);
    }
}
extern "C" void nx_control_model_clear(volatile uint32_t* reg, uint32_t mask) {
    *reg &= ~mask;
}
extern "C" void nx_control_model_priority(IRQn_Type irq, uint32_t priority) {
    if ((int)irq != refused_priority)
        NVIC->IPR[(unsigned)irq] = (uint8_t)priority;
}
extern "C" void nx_control_model_enable(IRQn_Type irq) {
    if ((int)irq != refused_enable)
        NVIC->ISER[(unsigned)irq / 32U] |= 1U << ((unsigned)irq % 32U);
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
