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
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
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
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
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
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
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

#ifdef NX_CONTROL_EXTI
TEST_F(ControlFactory, EveryGPIOEXTILineUsesItsActualNVICVector) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    for (uint8_t line = 0U; line < 16U; ++line) {
        nx_exti_event_t events[2]{};
        nx_stm32_exti_state_t state{};
        const nx_stm32_exti_config_t config{GPIOF, events, 2U, NX_EXTI_FALLING,
                                            5U,    line,   5U, 0U};
        const IRQn_Type irq =
            line < 5U ? static_cast<IRQn_Type>(EXTI0_IRQn + line)
                      : (line < 10U ? EXTI9_5_IRQn : EXTI15_10_IRQn);
        ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_prepare(&state, &config))
            << "EXTI line " << static_cast<unsigned>(line);
        EXPECT_EQ(5U, NVIC_GetPriority(irq));
        EXPECT_NE(0U, NVIC_GetEnableIRQ(irq));
        EXPECT_EQ(5U, (SYSCFG->EXTICR[line / 4U] >> ((line % 4U) * 4U)) & 15U);
        EXPECT_EQ(NX_SUCCESS, nx_stm32_exti_close(&state));
        EXPECT_EQ(0U, NVIC_GetEnableIRQ(irq));
    }
}

TEST_F(ControlFactory, TouchLineElevenAndPeerRetainSharedVectorUntilLastClose) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    nx_exti_event_t first_events[2]{};
    nx_exti_event_t second_events[2]{};
    nx_stm32_exti_state_t first{};
    nx_stm32_exti_state_t second{};
    const nx_stm32_exti_config_t first_config{
        GPIOF, first_events, 2U, NX_EXTI_FALLING, 5U, 11U, 5U, 0U};
    const nx_stm32_exti_config_t second_config{
        GPIOG, second_events, 2U, NX_EXTI_BOTH, 6U, 12U, 5U, 0U};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_prepare(&first, &first_config));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_prepare(&second, &second_config));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_exti_close(&first));
    EXPECT_NE(0U, NVIC_GetEnableIRQ(EXTI15_10_IRQn));
    EXPECT_NE(0U, EXTI->IMR & (1U << 12U));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_exti_close(&second));
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(EXTI15_10_IRQn));
}

TEST_F(ControlFactory, EXTIRejectsMismatchedGPIOIdentityBeforeAnyEffect) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    nx_exti_event_t events[2]{};
    nx_stm32_exti_state_t state{};
    const nx_stm32_exti_config_t config{GPIOF, events, 2U, NX_EXTI_FALLING,
                                        0U,    5U,     5U, 0U};
    const auto* bytes = static_cast<const uint8_t*>(mmio);
    const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_exti_prepare(&state, &config));
    EXPECT_FALSE(state.cold_owned);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(0, std::memcmp(before.data(), bytes, before.size()));
}

TEST_F(ControlFactory, EXTIInputPullAndInvalidChoiceRespectThePinBoundary) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    nx_exti_event_t events[2]{};
    nx_stm32_exti_state_t state{};
    nx_stm32_exti_config_t config{GPIOF, events, 2U, NX_EXTI_FALLING,
                                  5U,    11U,    5U, 3U};
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_exti_prepare(&state, &config));
    EXPECT_FALSE(state.cold_owned);
    const uint32_t before = UINT32_C(0x76543210) & ~(3U << 22U);
    GPIOF->PUPDR = before;
    config.pull = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_prepare(&state, &config));
    EXPECT_EQ(before | (1U << 22U), GPIOF->PUPDR);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_exti_close(&state));
}

TEST_F(ControlFactory, EXTIDoesNotRewriteAnEnabledPeersDifferentPriority) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    const unsigned irq = static_cast<unsigned>(EXTI15_10_IRQn);
    NVIC->IPR[irq] = 7U;
    NVIC->ISER[irq / 32U] = 1U << (irq % 32U);
    EXTI->IMR = 1U << 12U;
    nx_exti_event_t events[2]{};
    nx_stm32_exti_state_t state{};
    const nx_stm32_exti_config_t config{GPIOF, events, 2U, NX_EXTI_FALLING,
                                        5U,    11U,    5U, 0U};
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_exti_prepare(&state, &config));
    EXPECT_FALSE(state.cold_owned);
    EXPECT_EQ(7U, NVIC_GetPriority(EXTI15_10_IRQn));
    EXPECT_EQ(1U << 12U, EXTI->IMR);
    EXPECT_EQ(0U, GPIOF->MODER);
}

TEST_F(ControlFactory, TouchVectorEnableFailureRetainsTheCleanupOwner) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    refused_enable = static_cast<int>(EXTI15_10_IRQn);
    nx_exti_event_t events[2]{};
    nx_stm32_exti_state_t state{};
    const nx_stm32_exti_config_t config{GPIOF, events, 2U, NX_EXTI_FALLING,
                                        5U,    11U,    5U, 0U};
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_exti_prepare(&state, &config));
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.cold_owned);
    EXPECT_EQ(events, state.storage);
    refused_enable = -1;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_exti_close(&state));
    EXPECT_FALSE(state.cold_owned);
}

TEST_F(ControlFactory, ActiveTouchPublisherPreventsColdAdmissionAndFinalClose) {
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    const unsigned irq = static_cast<unsigned>(EXTI15_10_IRQn);
    const uint32_t bit = 1U << (irq % 32U);
    NVIC->IABR[irq / 32U] = bit;
    nx_exti_event_t events[2]{};
    nx_stm32_exti_state_t state{};
    const nx_stm32_exti_config_t config{GPIOF, events, 2U, NX_EXTI_FALLING,
                                        5U,    11U,    5U, 0U};
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_exti_prepare(&state, &config));
    EXPECT_FALSE(state.cold_owned);
    NVIC->IABR[irq / 32U] = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_prepare(&state, &config));
    NVIC->IABR[irq / 32U] = bit;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_exti_close(&state));
    EXPECT_TRUE(state.initialized);
    EXPECT_TRUE(state.cold_owned);
    EXPECT_EQ(events, state.storage);
    NVIC->IABR[irq / 32U] = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_exti_close(&state));
}
#endif

#ifdef NX_CONTROL_PWM
TEST_F(ControlFactory, FixedPwmUsesEveryReviewedChannelAndSelectedTimer) {
    const struct {
        TIM_TypeDef* timer;
        GPIO_TypeDef* gpio;
        uint32_t gate;
        uint8_t first_pin;
        uint8_t channels;
        uint8_t af;
        bool apb2;
    } timers[]{
        {TIM3, GPIOA, RCC_APB1ENR_TIM3EN, 6U, 4U, 2U, false},
        {TIM4, GPIOB, RCC_APB1ENR_TIM4EN, 6U, 4U, 2U, false},
        {TIM5, GPIOA, RCC_APB1ENR_TIM5EN, 0U, 4U, 2U, false},
        {TIM9, GPIOA, RCC_APB2ENR_TIM9EN, 2U, 2U, 3U, true},
        {TIM10, GPIOB, RCC_APB2ENR_TIM10EN, 8U, 1U, 3U, true},
        {TIM11, GPIOB, RCC_APB2ENR_TIM11EN, 9U, 1U, 3U, true},
        {TIM12, GPIOB, RCC_APB1ENR_TIM12EN, 14U, 2U, 9U, false},
        {TIM13, GPIOA, RCC_APB1ENR_TIM13EN, 6U, 1U, 9U, false},
        {TIM14, GPIOA, RCC_APB1ENR_TIM14EN, 7U, 1U, 9U, false},
    };
    for (const auto& selected : timers) {
        for (uint8_t channel = 1U; channel <= selected.channels; ++channel) {
            SCOPED_TRACE(reinterpret_cast<uintptr_t>(selected.timer));
            SCOPED_TRACE(static_cast<unsigned>(channel));
            const bool tim3_upper = selected.timer == TIM3 && channel > 2U;
            GPIO_TypeDef* bank = tim3_upper ? GPIOB : selected.gpio;
            const uint8_t pin = static_cast<uint8_t>(
                tim3_upper ? channel - 3U : selected.first_pin + channel - 1U);
            const uint32_t period = selected.timer == TIM5 ? 65537U : 1000U;
            nx_stm32_pwm_state_t state{};
            nx_stm32_gpio_state_t inactive{};
            const nx_stm32_pwm_config_t config{
                period,   1U,
                1000000U, static_cast<uint16_t>(selected.apb2 ? 167U : 83U),
                false,    selected.timer,
                bank,     channel,
                pin};
            RCC->APB1ENR = RCC_APB1ENR_TIM6EN;
            RCC->APB2ENR = RCC_APB2ENR_TIM8EN;
            const auto result =
                nx_stm32_pwm_prepare(&state, &inactive, &config);
            EXPECT_EQ(NX_SUCCESS, result);
            if (result != NX_SUCCESS)
                continue;
            EXPECT_EQ(selected.timer, state.registers);
            EXPECT_EQ(bank, inactive.registers);
            EXPECT_EQ(1U << pin, state.inactive_mask);
            EXPECT_EQ(channel, state.channel);
            EXPECT_EQ(RCC_APB1ENR_TIM6EN | (selected.apb2 ? 0U : selected.gate),
                      RCC->APB1ENR);
            EXPECT_EQ(RCC_APB2ENR_TIM8EN | (selected.apb2 ? selected.gate : 0U),
                      RCC->APB2ENR);
            EXPECT_EQ(period - 1U, selected.timer->ARR);
            const nx_pwm_port_t face{&nx_stm32_pwm_ops, &state};
            ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
            EXPECT_NE(0U, selected.timer->CCER & (1U << ((channel - 1U) * 4U)));
            EXPECT_EQ(2U, (bank->MODER >> (pin * 2U)) & 3U);
            EXPECT_EQ(selected.af,
                      (bank->AFR[pin / 8U] >> ((pin % 8U) * 4U)) & 15U);
            EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
            EXPECT_EQ(RCC_APB1ENR_TIM6EN, RCC->APB1ENR);
            EXPECT_EQ(RCC_APB2ENR_TIM8EN, RCC->APB2ENR);
        }
    }
}

TEST_F(ControlFactory, Apb2PwmRefusedGateRetainsOnlyItsCleanupResponsibility) {
    nx_stm32_pwm_state_t state{};
    nx_stm32_gpio_state_t inactive{};
    const nx_stm32_pwm_config_t config{1000U, 1U,    1000000U, 167U, false,
                                       TIM10, GPIOB, 1U,       8U};
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    refused = &RCC->APB2ENR;
    held_bits = RCC_APB2ENR_TIM10EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_pwm_prepare(&state, &inactive, &config));
    EXPECT_EQ(TIM10, state.registers);
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
    EXPECT_EQ(RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pwm_prepare(&state, &inactive, &config));
    refused = &RCC->APB2ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_pwm_close(&state, &inactive));
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_EQ(RCC_APB2ENR_SYSCFGEN | RCC_APB2ENR_TIM10EN, RCC->APB2ENR);
    refused = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
    EXPECT_EQ(0U, state.cold_effects);
    EXPECT_EQ(RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR);
}

TEST_F(ControlFactory, SmallPwmTimersRejectAbsentChannelsAndWrongDividers) {
    const struct {
        TIM_TypeDef* timer;
        GPIO_TypeDef* gpio;
        uint8_t channel;
        uint8_t pin;
        uint16_t psc;
    } absent[]{
        {TIM9, GPIOA, 3U, 4U, 167U},  {TIM10, GPIOB, 2U, 8U, 167U},
        {TIM11, GPIOB, 2U, 9U, 167U}, {TIM12, GPIOB, 3U, 16U, 83U},
        {TIM13, GPIOA, 2U, 6U, 83U},  {TIM14, GPIOA, 2U, 7U, 83U},
    };
    for (const auto& selected : absent) {
        nx_stm32_pwm_state_t state{};
        nx_stm32_gpio_state_t inactive{};
        const nx_stm32_pwm_config_t config{1000U,         1U,
                                           1000000U,      selected.psc,
                                           false,         selected.timer,
                                           selected.gpio, selected.channel,
                                           selected.pin};
        const auto* bytes = static_cast<const uint8_t*>(mmio);
        const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_pwm_prepare(&state, &inactive, &config));
        EXPECT_EQ(0, std::memcmp(bytes, before.data(), before.size()));
        EXPECT_EQ(0U, state.cold_effects);
    }
    nx_stm32_pwm_state_t state{};
    nx_stm32_gpio_state_t inactive{};
    const nx_stm32_pwm_config_t wrong_apb{1000U, 1U,    1000000U, 83U, false,
                                          TIM9,  GPIOA, 1U,       2U};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_pwm_prepare(&state, &inactive, &wrong_apb));
    EXPECT_EQ(0U, RCC->APB1ENR | RCC->APB2ENR);
}

TEST_F(ControlFactory, InvalidPwmRouteRejectsBeforeClockOrPadChanges) {
    for (unsigned invalid = 0U; invalid < 4U; ++invalid) {
        SCOPED_TRACE(invalid);
        nx_stm32_pwm_state_t state{};
        nx_stm32_gpio_state_t inactive{};
        nx_stm32_pwm_config_t config{1000U, 0U,    1000000U, 83U, false,
                                     TIM3,  GPIOA, 1U,       6U};
        if (invalid == 0U)
            config.registers = TIM4;
        if (invalid == 1U)
            config.gpio = GPIOB;
        if (invalid == 2U)
            config.channel = 0U;
        if (invalid == 3U)
            config.pin = 5U;
        const auto* bytes = static_cast<const uint8_t*>(mmio);
        const std::vector<uint8_t> before(bytes, bytes + 0x80000U);
        const auto result = nx_stm32_pwm_prepare(&state, &inactive, &config);
        EXPECT_EQ(NX_ERROR_INVALID, result);
        EXPECT_EQ(0, std::memcmp(bytes, before.data(), before.size()));
        if (result == NX_SUCCESS) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
        }
    }
}

TEST_F(ControlFactory, Tim5PreservesFullWidthPeriodAndFullDuty) {
    nx_stm32_pwm_state_t state{};
    nx_stm32_gpio_state_t inactive{};
    const nx_stm32_pwm_config_t config{UINT32_MAX, 0U,    1000000U, 83U, false,
                                       TIM5,       GPIOA, 4U,       3U};
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pwm_prepare(&state, &inactive, &config));
    const nx_pwm_port_t face{&nx_stm32_pwm_ops, &state};
    EXPECT_EQ(UINT32_MAX - 1U, TIM5->ARR);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, UINT32_MAX, UINT32_MAX));
    EXPECT_EQ(UINT32_MAX, TIM5->CCR4);
    nx_pwm_state_t observed{};
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_state(&face, &observed));
    EXPECT_EQ(UINT32_MAX, observed.period_ticks);
    EXPECT_EQ(UINT32_MAX, observed.duty_ticks);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
}

TEST_F(ControlFactory, ForeignInactiveGpioRejectsBeforeTimerAcquisition) {
    nx_stm32_pwm_state_t state{};
    nx_stm32_gpio_state_t inactive{GPIOB, 1U, true, true};
    const nx_stm32_pwm_config_t config{1000U, 1U,    1000000U, 83U, false,
                                       TIM5,  GPIOA, 4U,       3U};
    const auto original = inactive;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pwm_prepare(&state, &inactive, &config));
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->AHB1ENR);
    EXPECT_EQ(nullptr, state.registers);
    EXPECT_EQ(0, std::memcmp(&original, &inactive, sizeof(original)));
    if (state.cold_effects != 0U) {
        EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
    }
}

TEST_F(ControlFactory, Tim5ClockFailureRetainsOnlyItsOwnCleanup) {
    nx_stm32_pwm_state_t state{};
    nx_stm32_gpio_state_t inactive{};
    const nx_stm32_pwm_config_t config{1000U, 1U,    1000000U, 83U, false,
                                       TIM5,  GPIOA, 4U,       3U};
    RCC->APB1ENR = RCC_APB1ENR_TIM3EN;
    TIM3->CR1 = TIM_CR1_CEN;
    TIM3->CCER = TIM_CCER_CC1E;
    refused = &RCC->APB1ENR;
    held_bits = RCC_APB1ENR_TIM5EN;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_pwm_prepare(&state, &inactive, &config));
    EXPECT_EQ(TIM5, state.registers);
    EXPECT_NE(0U, state.cold_effects);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
    EXPECT_EQ(0U, state.cold_effects);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pwm_prepare(&state, &inactive, &config));
    refused = &RCC->APB1ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_pwm_close(&state, &inactive));
    EXPECT_NE(0U, state.cold_effects);
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pwm_close(&state, &inactive));
    EXPECT_EQ(RCC_APB1ENR_TIM3EN, RCC->APB1ENR);
    EXPECT_EQ(TIM_CR1_CEN, TIM3->CR1);
    EXPECT_EQ(TIM_CCER_CC1E, TIM3->CCER);
}
#endif

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
    const nx_stm32_pwm_config_t config = {1000U, 0U,    1000000U, 83U, false,
                                          TIM3,  GPIOA, 1U,       6U};
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
                                           0U,    5U,     5U, 0U};
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
    const nx_stm32_adc_config_t config = {channels, sample_times, 1U,
                                          3300U,    0U,           ADC1};
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
