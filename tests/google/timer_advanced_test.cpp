/**
 * \file            timer_advanced_test.cpp
 * \brief           Production timer capture and break/deadtime contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/arch/arch.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f407xx.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#define CAP_INIT    nx_stm32_timer_capture_initialize
#define CAP_RELEASE nx_stm32_timer_capture_release
#define CAP_IRQ     nx_stm32_timer_capture_irq
#define CAP_OPS     nx_stm32_timer_capture_ops
#define ADV_INIT    nx_stm32_advanced_pwm_initialize
#define ADV_RELEASE nx_stm32_advanced_pwm_release
#define ADV_IRQ     nx_stm32_advanced_pwm_irq
#define ADV_OPS     nx_stm32_advanced_pwm_ops
#define CAP_HZ      84000000U
#define ADV_HZ      168000000U
#define CAP_CR1     TIM4->CR1
#define CAP_CR2     TIM4->CR2
#define CAP_SMCR    TIM4->SMCR
#define CAP_PSC     TIM4->PSC
#define CAP_ARR     TIM4->ARR
#define CAP_CCR     TIM4->CCR1
#define CAP_EGR     TIM4->EGR
#define CAP_CCMR    TIM4->CCMR1
#define CAP_CCER    TIM4->CCER
#define CAP_DIER    TIM4->DIER
#define CAP_SR      TIM4->SR
#define ADV_CR1     TIM1->CR1
#define ADV_SMCR    TIM1->SMCR
#define ADV_RCR     TIM1->RCR
#define ADV_PSC     TIM1->PSC
#define ADV_ARR     TIM1->ARR
#define ADV_CCR     TIM1->CCR1
#define ADV_CCMR    TIM1->CCMR1
#define ADV_CCER    TIM1->CCER
#define ADV_DIER    TIM1->DIER
#define ADV_BDTR    TIM1->BDTR
#define ADV_SR      TIM1->SR
#define BREAK_INPUT GPIOB->IDR

static bool s_isr;
static bool s_privileged;
static uint32_t s_mask;
static nx_time_us_t s_now;
static volatile uint32_t* s_refused;
static uint32_t s_hold;
static bool s_break_on_enable;
static bool s_break_before_enable;
static unsigned s_moe_enable_writes;
static unsigned s_writes;
static bool s_edge_before_capture_read;
static bool s_edge_after_capture_read;
static ::testing::MockFunction<nx_time_us_t()>* s_observation;
static volatile uint32_t* s_capture_read = &CAP_CCR;
static uint32_t s_capture_flag = 2U;

/** \brief External CPU context, not a replacement timer implementation. */
extern "C" bool nx_arch_in_isr(void) {
    return s_isr;
}
/** \brief Inject privilege at the actual task admission boundary. */
extern "C" bool nx_arch_is_privileged(void) {
    return s_privileged;
}
/** \brief Preserve an incoming local mask across bounded metadata sections. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t old{s_mask};
    s_mask = 1U;
    return old;
}
/** \brief Restore the exact incoming model mask, without enabling progress. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t old) {
    s_mask = old.value;
}
/** \brief Model bus ordering only; physical latency remains unmeasured. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief Separate monotonic IRQ observation from hardware capture ticks. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return s_observation != nullptr ? s_observation->Call() : s_now;
}
/** \brief W0C flags and asynchronous break obey actual timer register facts. */
extern "C" void nx_timer_model_write(volatile uint32_t* address,
                                     uint32_t value) {
    ++s_writes;
    if (address == &ADV_BDTR && (value & 32768U) != 0U) {
        ++s_moe_enable_writes;
    }
    if (address == s_refused) {
        value = (value & ~s_hold) | (*address & s_hold);
    }
    if (address == &CAP_SR || address == &TIM3->SR || address == &TIM5->SR ||
        address == &ADV_SR || address == &TIM8->SR) {
        *address &= value;
    } else {
        *address = value;
    }
    for (TIM_TypeDef* timer : {TIM3, TIM4, TIM5}) {
        if (address == &timer->EGR && (value & 1U) != 0U &&
            (timer->CR1 & 2U) == 0U) {
            timer->SR |= 1U;
        }
    }
    if (address == &ADV_BDTR && (value & 32768U) != 0U && s_break_on_enable) {
        BREAK_INPUT |= 4096U;
        ADV_SR |= 128U;
        ADV_BDTR &= ~32768U;
    }
    if (address == &ADV_CR1 && (value & 1U) != 0U && s_break_before_enable) {
        ADV_SR |= 128U;
        ADV_BDTR &= ~32768U;
    }
    if (address == &TIM8->BDTR && (value & 32768U) != 0U && s_break_on_enable) {
        GPIOA->IDR |= 64U;
        TIM8->SR |= 128U;
        TIM8->BDTR &= ~32768U;
    }
}

/** \brief Actual input capture read-clear with asynchronous edge injection. */
extern "C" uint32_t nx_timer_model_read(volatile uint32_t* address) {
    if (address == s_capture_read && s_edge_before_capture_read) {
        *address = 211U;
        CAP_SR |= s_capture_flag | (s_capture_flag << 8U);
        s_edge_before_capture_read = false;
    }
    uint32_t value = *address;
    for (TIM_TypeDef* timer : {TIM3, TIM4, TIM5}) {
        volatile uint32_t* ccr[4] = {&timer->CCR1, &timer->CCR2, &timer->CCR3,
                                     &timer->CCR4};
        for (unsigned channel = 0U; channel < 4U; ++channel) {
            if (address == ccr[channel]) {
                timer->SR &= ~(2U << channel);
            }
        }
    }
    if (address == s_capture_read) {
        if (s_edge_after_capture_read) {
            *address = 211U;
            CAP_SR |= s_capture_flag;
            s_edge_after_capture_read = false;
        }
    }
    return value;
}

class TimerHardware : public ::testing::Test {
  protected:
    nx_timer_capture_controller_t capture{};
    nx_advanced_pwm_controller_t advanced{};
    nx_timer_capture_config_t cap_config{CAP_HZ, 1000000U, NX_TIMER_EDGE_RISING,
                                         0U,     true,     1U};
    nx_advanced_pwm_config_t adv_config{&BREAK_INPUT, 4096U, ADV_HZ, 1000000U,
                                        1000U,        128U,  true,   false,
                                        false,        true,  1U};
    nx_timer_capture_t cap{&CAP_OPS, &capture};
    nx_advanced_pwm_t pwm{&ADV_OPS, &advanced};
    nx_timer_capture_event_t ring[2]{};
    void SetUp() override {
        void* p = mmap(reinterpret_cast<void*>(0x40000000U), 0x25000U,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_EQ(p, reinterpret_cast<void*>(0x40000000U));
        std::memset(p, 0, 0x25000U);
        s_isr = false;
        s_privileged = true;
        s_mask = 0U;
        s_now = 700U;
        s_refused = nullptr;
        s_hold = 0U;
        s_break_on_enable = false;
        s_break_before_enable = false;
        s_moe_enable_writes = 0U;
        s_writes = 0U;
        s_edge_before_capture_read = false;
        s_edge_after_capture_read = false;
        s_observation = nullptr;
        s_capture_read = &CAP_CCR;
        s_capture_flag = 2U;
        RCC->APB1ENR = RCC_APB1ENR_TIM4EN;
        RCC->APB2ENR = RCC_APB2ENR_TIM1EN;
        RCC->CFGR = RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
        GPIOA->MODER = 2U << 16U;
        GPIOA->AFR[1] = 1U;
        GPIOB->MODER = (2U << 12U) | (10U << 24U);
        GPIOB->AFR[0] = 2U << 24U;
        GPIOB->AFR[1] = 17U << 16U;
    }
    void edge(uint32_t ticks, uint32_t flags = 2U) {
        CAP_CCR = ticks;
        CAP_SR |= flags;
        s_isr = true;
        CAP_IRQ(&capture);
        s_isr = false;
    }
    void Tim8Pads(void) {
        RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
        GPIOC->MODER |= 2U << 12U;
        GPIOC->AFR[0] |= 3U << 24U;
        GPIOA->MODER |= 10U << 12U;
        GPIOA->AFR[0] |= 51U << 24U;
    }
};

/** \brief SDK channel registers remain separate scalar MMIO objects. */
static volatile uint32_t* CaptureRegister(TIM_TypeDef* timer,
                                          unsigned channel) {
    switch (channel) {
        case 1U:
            return &timer->CCR1;
        case 2U:
            return &timer->CCR2;
        case 3U:
            return &timer->CCR3;
        default:
            return &timer->CCR4;
    }
}

TEST_F(TimerHardware, CaptureAllFourChannelsUseExactModesFlagsAndRegisters) {
    GPIOB->MODER |= 0xAAU << 12U;
    GPIOB->AFR[0] |= 0x22U << 24U;
    GPIOB->AFR[1] |= 0x22U;
    for (uint8_t channel = 1U; channel <= 4U; ++channel) {
        SCOPED_TRACE(channel);
        std::memset(TIM4, 0, sizeof(*TIM4));
        capture = {};
        auto config = cap_config;
        config.channel = channel;
        config.edge = NX_TIMER_EDGE_BOTH;
        config.filter = 7U;
        ASSERT_EQ(NX_SUCCESS, CAP_INIT(&capture, &config));
        const uint32_t mode = 0x71U << (((channel - 1U) % 2U) * 8U);
        EXPECT_EQ(channel <= 2U ? mode : 0U, TIM4->CCMR1);
        EXPECT_EQ(channel > 2U ? mode : 0U, TIM4->CCMR2);
        ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&cap, ring, 2U));
        const uint32_t flag = 1U << channel;
        EXPECT_EQ(flag, CAP_DIER);
        EXPECT_EQ(11U << ((channel - 1U) * 4U), CAP_CCER);
        *CaptureRegister(TIM4, channel) = 100U + channel;
        CAP_SR |= flag | 1U;
        CAP_IRQ(&capture);
        nx_timer_capture_event_t event{};
        ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&cap, &event));
        EXPECT_EQ(100U + channel, event.captured_ticks);
        EXPECT_EQ(0U, CAP_SR & flag);
        EXPECT_NE(0U, CAP_SR & 1U);
        ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&cap));
        ASSERT_EQ(NX_SUCCESS, CAP_RELEASE(&capture));
    }
}

TEST_F(TimerHardware, SelectedCaptureOverrunPreservesOtherChannelFacts) {
    GPIOB->MODER |= 2U << 16U;
    GPIOB->AFR[1] |= 2U;
    cap_config.channel = 3U;
    ASSERT_EQ(NX_SUCCESS, CAP_INIT(&capture, &cap_config));
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&cap, ring, 2U));
    TIM4->CCR3 = 321U;
    CAP_SR |= (1U << 3U) | (1U << 11U) | 2U | 512U;
    CAP_IRQ(&capture);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&cap, &event));
    EXPECT_EQ(321U, event.captured_ticks);
    EXPECT_NE(0U, event.flags & NX_TIMER_CAPTURE_OVERCAPTURE);
    EXPECT_EQ(2U | 512U, CAP_SR & (2U | 512U));
    EXPECT_EQ(0U, CAP_SR & ((1U << 3U) | (1U << 11U)));
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&cap));
}

TEST_F(TimerHardware, ChannelFourReadPreservesAnEdgeArrivingAfterCcrRead) {
    GPIOB->MODER |= 2U << 18U;
    GPIOB->AFR[1] |= 2U << 4U;
    cap_config.channel = 4U;
    ASSERT_EQ(NX_SUCCESS, CAP_INIT(&capture, &cap_config));
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&cap, ring, 2U));
    s_capture_read = &TIM4->CCR4;
    s_capture_flag = 16U;
    s_edge_after_capture_read = true;
    TIM4->CCR4 = 111U;
    CAP_SR |= 16U;
    CAP_IRQ(&capture);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&cap, &event));
    EXPECT_EQ(111U, event.captured_ticks);
    EXPECT_NE(0U, CAP_SR & 16U);
    CAP_IRQ(&capture);
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&cap, &event));
    EXPECT_EQ(211U, event.captured_ticks);
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&cap));
}

TEST_F(TimerHardware, CaptureChannelAdmissionRejectsZeroAndFiveWithoutEffects) {
    for (uint8_t channel : {uint8_t{0U}, uint8_t{5U}}) {
        SCOPED_TRACE(channel);
        std::memset(TIM4, 0, sizeof(*TIM4));
        capture = {};
        cap_config.channel = channel;
        s_writes = 0U;
        EXPECT_EQ(NX_ERROR_INVALID, CAP_INIT(&capture, &cap_config));
        EXPECT_EQ(0U, s_writes);
        EXPECT_EQ(nullptr, capture.config);
    }
}

TEST_F(TimerHardware, FourthChannelStopRefusalKeepsRingAndDrainsLateFacts) {
    GPIOB->MODER |= 2U << 18U;
    GPIOB->AFR[1] |= 2U << 4U;
    cap_config.channel = 4U;
    ASSERT_EQ(NX_SUCCESS, CAP_INIT(&capture, &cap_config));
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&cap, ring, 2U));
    s_refused = &CAP_DIER;
    s_hold = 16U;
    ASSERT_EQ(NX_ERROR_BUSY, nx_timer_capture_stop(&cap));
    EXPECT_EQ(ring, capture.events);
    EXPECT_TRUE(capture.draining);
    TIM4->CCR4 = 777U;
    CAP_SR |= 16U | (16U << 8U) | 2U;
    CAP_IRQ(&capture);
    EXPECT_EQ(0U, capture.count);
    EXPECT_EQ(0U, CAP_SR & (16U | (16U << 8U)));
    EXPECT_NE(0U, CAP_SR & 2U);
    s_refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&cap));
    EXPECT_EQ(nullptr, capture.events);
}

TEST_F(TimerHardware, Tim3AndTim5KeepIndependentChannelsAndNativeWidths) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM5EN;
    GPIOB->MODER |= 2U;
    GPIOB->AFR[0] |= 2U;
    GPIOA->MODER |= 2U << 6U;
    GPIOA->AFR[0] |= 2U << 12U;
    nx_timer_capture_controller_t third{}, fifth{};
    auto three = cap_config;
    auto five = cap_config;
    three.channel = 3U;
    five.channel = 4U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_tim3_timer_capture_initialize(&third, &three));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_tim5_timer_capture_initialize(&fifth, &five));
    const nx_timer_capture_t a{&nx_stm32_tim3_timer_capture_ops, &third};
    const nx_timer_capture_t b{&nx_stm32_tim5_timer_capture_ops, &fifth};
    nx_timer_capture_event_t left[1]{}, right[1]{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&a, left, 1U));
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_start(&b, right, 1U));
    TIM3->CCR3 = 0xABCDU;
    TIM5->CCR4 = 0xFEDCBA98U;
    TIM3->SR |= 8U;
    TIM5->SR |= 16U;
    nx_stm32_tim3_timer_capture_irq(&third);
    nx_stm32_tim5_timer_capture_irq(&fifth);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&a, &event));
    EXPECT_EQ(0xABCDU, event.captured_ticks);
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_read(&b, &event));
    EXPECT_EQ(0xFEDCBA98U, event.captured_ticks);
    nx_timer_capture_state_t state{};
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_state(&b, &state));
    EXPECT_EQ(32U, state.counter_bits);
    EXPECT_EQ(UINT32_MAX, TIM5->ARR);
    EXPECT_EQ(UINT16_MAX, TIM3->ARR);
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&a));
    EXPECT_NE(0U, TIM5->CR1 & 1U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_tim3_timer_capture_release(&third));
    ASSERT_EQ(NX_SUCCESS, nx_timer_capture_stop(&b));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_tim5_timer_capture_release(&fifth));
}

TEST_F(TimerHardware, AdvancedPairsTwoAndThreeSelectExactIdleDutyAndSources) {
    Tim8Pads();
    GPIOA->MODER |= (2U << 18U) | (2U << 20U);
    GPIOA->AFR[1] |= 0x11U << 4U;
    GPIOB->MODER |= 0xAU << 28U;
    GPIOB->AFR[1] |= 0x11U << 24U;
    GPIOB->MODER |= 0xAU;
    GPIOB->AFR[0] |= 0x33U;
    GPIOC->MODER |= (2U << 14U) | (2U << 16U);
    GPIOC->AFR[0] |= 3U << 28U;
    GPIOC->AFR[1] |= 3U;
    for (TIM_TypeDef* timer : {TIM1, TIM8}) {
        for (uint8_t channel : {uint8_t{2U}, uint8_t{3U}}) {
            SCOPED_TRACE(channel);
            std::memset(timer, 0, sizeof(*timer));
            nx_advanced_pwm_controller_t local{};
            auto config = adv_config;
            config.channel = channel;
            config.idle_high = true;
            config.idle_complement_high = true;
            if (timer == TIM8) {
                config.break_input = &GPIOA->IDR;
                config.break_mask = 64U;
            }
            const nx_advanced_pwm_ops_t* ops =
                timer == TIM1 ? &ADV_OPS : &nx_stm32_tim8_advanced_pwm_ops;
            const auto initialize = timer == TIM1
                                        ? ADV_INIT
                                        : nx_stm32_tim8_advanced_pwm_initialize;
            const auto release = timer == TIM1
                                     ? ADV_RELEASE
                                     : nx_stm32_tim8_advanced_pwm_release;
            const nx_advanced_pwm_t face{ops, &local};
            ASSERT_EQ(NX_SUCCESS, initialize(&local, &config));
            const uint32_t mode = 104U << (((channel - 1U) % 2U) * 8U);
            EXPECT_EQ(channel == 2U ? mode : 0U, timer->CCMR1);
            EXPECT_EQ(channel == 3U ? mode : 0U, timer->CCMR2);
            EXPECT_EQ(768U << ((channel - 1U) * 2U), timer->CR2);
            ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&face, 345U));
            EXPECT_EQ(345U, *CaptureRegister(timer, channel));
            EXPECT_EQ(0U, timer->CCR1);
            ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&face));
            EXPECT_EQ(5U << ((channel - 1U) * 4U), timer->CCER);
            ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_stop(&face));
            EXPECT_EQ(0U, timer->CCER);
            EXPECT_EQ(0U, timer->BDTR & 32768U);
            ASSERT_EQ(NX_SUCCESS, release(&local));
        }
    }
}

TEST_F(TimerHardware, AdvancedPairAdmissionRejectsZeroAndFourWithoutEffects) {
    for (uint8_t channel : {uint8_t{0U}, uint8_t{4U}}) {
        std::memset(TIM1, 0, sizeof(*TIM1));
        advanced = {};
        adv_config.channel = channel;
        s_writes = 0U;
        EXPECT_EQ(NX_ERROR_INVALID, ADV_INIT(&advanced, &adv_config));
        EXPECT_EQ(0U, s_writes);
        EXPECT_EQ(nullptr, advanced.config);
    }
}

TEST_F(TimerHardware, ThirdPairBreakCannotReenableBeforeFullSourceWithdrawal) {
    Tim8Pads();
    GPIOC->MODER |= 2U << 16U;
    GPIOC->AFR[1] |= 3U;
    GPIOB->MODER |= 2U << 2U;
    GPIOB->AFR[0] |= 3U << 4U;
    auto config = adv_config;
    config.channel = 3U;
    config.break_input = &GPIOA->IDR;
    config.break_mask = 64U;
    nx_advanced_pwm_controller_t local{};
    const nx_advanced_pwm_t face{&nx_stm32_tim8_advanced_pwm_ops, &local};
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_tim8_advanced_pwm_initialize(&local, &config));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&face));
    TIM8->SR |= 128U;
    nx_stm32_tim8_advanced_pwm_irq(&local);
    EXPECT_TRUE(local.fault);
    EXPECT_EQ(0U, TIM8->BDTR & 32768U);
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&face));
    EXPECT_EQ(NX_ERROR_BUSY, nx_advanced_pwm_clear_fault(&face));
    s_refused = &TIM8->CCER;
    s_hold = 5U << 8U;
    ASSERT_EQ(NX_ERROR_BUSY, nx_advanced_pwm_stop(&face));
    EXPECT_EQ(NX_ERROR_BUSY, nx_advanced_pwm_clear_fault(&face));
    s_refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_stop(&face));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_clear_fault(&face));
    EXPECT_EQ(0U, TIM8->BDTR & 32768U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_tim8_advanced_pwm_release(&local));
}

TEST_F(TimerHardware, Tim8UsesItsOwnPadsClockAndBreakEncoding) {
    Tim8Pads();
    nx_advanced_pwm_controller_t second{};
    auto config = adv_config;
    config.break_input = &GPIOA->IDR;
    config.break_mask = 64U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_tim8_advanced_pwm_initialize(&second, &config));
    EXPECT_EQ(999U, TIM8->ARR);
    EXPECT_EQ(167U, TIM8->PSC);
    EXPECT_EQ(128U | 1024U | 2048U | 4096U | 8192U, TIM8->BDTR);
    EXPECT_EQ(0U, TIM1->CR1);
    const nx_advanced_pwm_t face{&nx_stm32_tim8_advanced_pwm_ops, &second};
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&face, 300U));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&face));
    EXPECT_NE(0U, TIM8->BDTR & 32768U);
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_stop(&face));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_tim8_advanced_pwm_release(&second));
    EXPECT_EQ(nullptr, second.config);
}

TEST_F(TimerHardware, TwoAdvancedControllersKeepDutyAndBreakIndependent) {
    Tim8Pads();
    ASSERT_EQ(NX_SUCCESS, ADV_INIT(&advanced, &adv_config));
    nx_advanced_pwm_controller_t second{};
    auto config = adv_config;
    config.break_input = &GPIOA->IDR;
    config.break_mask = 64U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_tim8_advanced_pwm_initialize(&second, &config));
    const nx_advanced_pwm_t face{&nx_stm32_tim8_advanced_pwm_ops, &second};
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&pwm, 100U));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_set_duty(&face, 200U));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&pwm));
    ASSERT_EQ(NX_SUCCESS, nx_advanced_pwm_start(&face));
    GPIOA->IDR |= 64U;
    TIM8->SR |= 128U;
    nx_stm32_tim8_advanced_pwm_irq(&second);
    EXPECT_TRUE(second.fault);
    EXPECT_FALSE(advanced.fault);
    EXPECT_EQ(0U, TIM8->BDTR & 32768U);
    EXPECT_NE(0U, TIM1->BDTR & 32768U);
    EXPECT_EQ(100U, TIM1->CCR1);
    EXPECT_EQ(200U, TIM8->CCR1);
}

TEST_F(TimerHardware, Tim8RejectsTim1BreakAndMissingTim8GateBeforeWrites) {
    Tim8Pads();
    nx_advanced_pwm_controller_t second{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_tim8_advanced_pwm_initialize(&second, &adv_config));
    auto config = adv_config;
    config.break_input = &GPIOA->IDR;
    config.break_mask = 64U;
    RCC->APB2ENR &= ~(uint32_t)RCC_APB2ENR_TIM8EN;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32_tim8_advanced_pwm_initialize(&second, &config));
    EXPECT_EQ(0U, s_writes);
    EXPECT_EQ(nullptr, second.config);
}

TEST_F(TimerHardware, Tim8BreakDuringMoeAdmissionNeverAutomaticallyRestarts) {
    Tim8Pads();
    nx_advanced_pwm_controller_t second{};
    auto config = adv_config;
    config.break_input = &GPIOA->IDR;
    config.break_mask = 64U;
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32_tim8_advanced_pwm_initialize(&second, &config));
    const nx_advanced_pwm_t face{&nx_stm32_tim8_advanced_pwm_ops, &second};
    s_break_on_enable = true;
    ASSERT_EQ(NX_ERROR_IO, nx_advanced_pwm_start(&face));
    EXPECT_TRUE(second.fault);
    EXPECT_EQ(0U, TIM8->BDTR & 32768U);
    s_break_on_enable = false;
    GPIOA->IDR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_advanced_pwm_start(&face));
    EXPECT_EQ(0U, TIM8->BDTR & 32768U);
}

TEST(TimerContracts, InterfacesKeepOptionalModesSeparate) {
    EXPECT_EQ(nx_timer_capture_start(nullptr, nullptr, 0U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_advanced_pwm_start(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(sizeof(nx_timer_capture_ops_t), sizeof(void*) * 4U);
    EXPECT_EQ(sizeof(nx_advanced_pwm_ops_t), sizeof(void*) * 5U);
}

TEST_F(TimerHardware, CaptureAuthorityRejectsBeforeMmio) {
    cap_config.exclusive = false;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_PERMISSION);
    EXPECT_EQ(s_writes, 0U);
    cap_config.exclusive = true;
    s_isr = true;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, CaptureRejectsUnrepresentableClockAndFilter) {
    cap_config.tick_hz = 999999U;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_INVALID);
    cap_config.tick_hz = 1000000U;
    cap_config.filter = 16U;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, CaptureCannotReplaceEnabledController) {
    CAP_CCER = 16U;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_BUSY);
    EXPECT_EQ(CAP_CCER, 16U);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, CaptureReportsRawModuloTicksAndSeparateObservation) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    edge(65534U);
    s_now = 900U;
    edge(2U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 65534U);
    EXPECT_EQ(event.observed_at, 700U);
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 2U);
    EXPECT_EQ(event.observed_at, 900U);
    EXPECT_EQ(nx_timer_capture_read(&cap, &event), NX_ERROR_EMPTY);
}

TEST_F(TimerHardware, CaptureOverflowIsObservableWithoutOverwritingConsumer) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    edge(1U);
    edge(2U);
    edge(3U, 2U | 512U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 1U);
    edge(4U);
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 2U);
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 4U);
    EXPECT_EQ(event.flags,
              NX_TIMER_CAPTURE_OVERCAPTURE | NX_TIMER_CAPTURE_RING_LOSS);
    nx_timer_capture_state_t state{};
    ASSERT_EQ(nx_timer_capture_state(&cap, &state), NX_SUCCESS);
    EXPECT_EQ(state.loss_incidents, 2U);
    EXPECT_EQ(state.counter_bits, 16U);
}

TEST_F(TimerHardware, CaptureStopRetainsQueuedRingUntilTaskDrain) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    edge(19U);
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_ERROR_BUSY);
    EXPECT_EQ(capture.events, ring);
    edge(20U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 19U);
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_SUCCESS);
    EXPECT_EQ(capture.events, nullptr);
    edge(21U);
    EXPECT_EQ(ring[0].captured_ticks, 19U);
}

TEST_F(TimerHardware, CaptureFailedDetachRetainsRingAndIncomingMask) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    s_refused = &CAP_DIER;
    s_hold = 2U;
    s_mask = 1U;
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_ERROR_BUSY);
    EXPECT_EQ(capture.events, ring);
    EXPECT_EQ(s_mask, 1U);
    s_refused = nullptr;
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_SUCCESS);
    EXPECT_EQ(s_mask, 1U);
}

TEST_F(TimerHardware, CaptureRejectsRingControlOutputAndOverflowAliases) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    EXPECT_EQ(
        nx_timer_capture_start(
            &cap, reinterpret_cast<nx_timer_capture_event_t*>(&capture), 1U),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_timer_capture_start(&cap, ring, SIZE_MAX), NX_ERROR_INVALID);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    edge(17U);
    EXPECT_EQ(nx_timer_capture_read(&cap, ring), NX_ERROR_INVALID);
    EXPECT_EQ(capture.count, 1U);
}

TEST_F(TimerHardware, CaptureW0cPreservesUnrelatedObservedFacts) {
    cap_config.edge = NX_TIMER_EDGE_BOTH;
    cap_config.filter = 7U;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    EXPECT_EQ(CAP_CCER & 10U, 10U);
    EXPECT_EQ(CAP_CCMR, 113U);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    CAP_SR = 4U;
    edge(23U);
    EXPECT_EQ(CAP_SR, 4U);
}

TEST_F(TimerHardware, CaptureErrorOnlyDoesNotInventAnEdge) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    edge(123U, 512U);
    nx_timer_capture_event_t event{};
    EXPECT_EQ(nx_timer_capture_read(&cap, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(capture.loss_incidents, 1U);
}

TEST_F(TimerHardware, AdvancedAuthorityRejectsBeforeMmio) {
    adv_config.exclusive = false;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_PERMISSION);
    EXPECT_EQ(s_writes, 0U);
    adv_config.exclusive = true;
    s_privileged = false;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, AdvancedDeadtimeRepresentabilityFollowsActualEncoding) {
    for (uint32_t code = 0U; code <= 255U; ++code) {
        ADV_BDTR = ADV_CR1 = ADV_CCER = ADV_DIER = 0U;
        advanced = {};
        adv_config.deadtime_ticks = code < 128U   ? code
                                    : code < 192U ? (64U + (code & 63U)) * 2U
                                    : code < 224U ? (32U + (code & 31U)) * 8U
                                                  : (32U + (code & 31U)) * 16U;
        ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
        EXPECT_EQ(ADV_BDTR & 255U, code);
        EXPECT_EQ(ADV_BDTR & (768U | 16384U | 32768U), 0U);
    }
}

TEST_F(TimerHardware, AdvancedDoesNotRoundUnrepresentableDeadtime) {
    adv_config.deadtime_ticks = 129U;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_INVALID);
    adv_config.deadtime_ticks = 1009U;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, AdvancedRejectsIrreversibleExistingLock) {
    ADV_BDTR = 256U;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_BUSY);
    EXPECT_EQ(ADV_BDTR, 256U);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, AdvancedDutyCannotModifyFixedBase) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    EXPECT_EQ(nx_advanced_pwm_set_duty(&pwm, 1001U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_advanced_pwm_set_duty(&pwm, 1000U), NX_SUCCESS);
    EXPECT_EQ(ADV_CCR, 1000U);
    ADV_ARR = 8U;
    EXPECT_EQ(nx_advanced_pwm_set_duty(&pwm, 3U), NX_ERROR_STATE);
    EXPECT_EQ(ADV_CCR, 1000U);
}

TEST_F(TimerHardware, AdvancedDeadtimeClockIsIndependentOfPsc) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    nx_advanced_pwm_state_t state{};
    ASSERT_EQ(nx_advanced_pwm_state(&pwm, &state), NX_SUCCESS);
    EXPECT_EQ(state.tick_hz, 1000000U);
    EXPECT_EQ(state.deadtime_hz, ADV_HZ);
    EXPECT_EQ(state.deadtime_ticks, 128U);
    EXPECT_EQ(ADV_PSC, ADV_HZ / 1000000U - 1U);
}

TEST_F(TimerHardware, AdvancedHardwareBreakLatchesAndNeverAutorestarts) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ASSERT_EQ(nx_advanced_pwm_start(&pwm), NX_SUCCESS);
    BREAK_INPUT |= 4096U;
    ADV_SR |= 128U;
    ADV_BDTR &= ~32768U;
    s_isr = true;
    ADV_IRQ(&advanced);
    s_isr = false;
    EXPECT_TRUE(advanced.fault);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_STATE);
    ASSERT_EQ(nx_advanced_pwm_stop(&pwm), NX_SUCCESS);
    EXPECT_EQ(nx_advanced_pwm_clear_fault(&pwm), NX_ERROR_BUSY);
    BREAK_INPUT &= ~4096U;
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_STATE);
    EXPECT_EQ(nx_advanced_pwm_clear_fault(&pwm), NX_SUCCESS);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_SUCCESS);
}

TEST_F(TimerHardware, AdvancedLateBreakBeforeMoeReadbackFailsStart) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    s_break_on_enable = true;
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_IO);
    EXPECT_TRUE(advanced.fault);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
}

TEST_F(TimerHardware, AdvancedFailedMoeDetachKeepsOwnerResponsibility) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ASSERT_EQ(nx_advanced_pwm_start(&pwm), NX_SUCCESS);
    s_refused = &ADV_BDTR;
    s_hold = 32768U;
    EXPECT_EQ(nx_advanced_pwm_stop(&pwm), NX_ERROR_BUSY);
    EXPECT_EQ(ADV_BDTR & 32768U, 32768U);
    s_refused = nullptr;
    EXPECT_EQ(nx_advanced_pwm_stop(&pwm), NX_SUCCESS);
}

TEST_F(TimerHardware, AdvancedRejectsOutputThatOverwritesControl) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    EXPECT_EQ(nx_advanced_pwm_state(
                  &pwm, reinterpret_cast<nx_advanced_pwm_state_t*>(&advanced)),
              NX_ERROR_INVALID);
    EXPECT_TRUE(advanced.initialized);
}

TEST_F(TimerHardware, AdvancedClearsOnlyObservedBreakFlag) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ADV_SR = 128U | 4U;
    s_isr = true;
    ADV_IRQ(&advanced);
    s_isr = false;
    EXPECT_EQ(ADV_SR, 4U);
    EXPECT_TRUE(advanced.fault);
}

TEST_F(TimerHardware, AdvancedRejectsWrongPhysicalBreakBitBeforeWrites) {
    adv_config.break_mask = 1U;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_INVALID);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, AdvancedCannotEnableAlteredDeadtimeOrBreakPolarity) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ADV_BDTR ^= 1U;
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_STATE);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
}

TEST_F(TimerHardware, AdvancedCannotEnableAlteredDtsClockDivider) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ADV_CR1 |= 256U;
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_STATE);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
}

TEST_F(TimerHardware, AdvancedColdConfigurationCannotAliasMutableState) {
    union Storage {
        nx_advanced_pwm_config_t config;
        nx_advanced_pwm_controller_t state;
        Storage() : config{} {
        }
    } storage;
    storage.config = adv_config;
    auto before = storage.config;
    EXPECT_EQ(ADV_INIT(&storage.state, &storage.config), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&storage.config, &before, sizeof(before)), 0);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, CaptureProtectsActualInterfaceTableFromRingOverwrite) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    auto* alias = reinterpret_cast<nx_timer_capture_event_t*>(
        const_cast<nx_timer_capture_ops_t*>(&CAP_OPS));
    EXPECT_EQ(nx_timer_capture_start(&cap, alias, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(capture.events, nullptr);
}

TEST_F(TimerHardware, CaptureRejectsInterfaceFaceAndMethodsOutputAliases) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    EXPECT_EQ(nx_timer_capture_start(
                  &cap, reinterpret_cast<nx_timer_capture_event_t*>(&cap), 1U),
              NX_ERROR_INVALID);
    EXPECT_EQ(capture.events, nullptr);
}

TEST_F(TimerHardware, CaptureRejectsAlteredInputSelectionAtStart) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    CAP_CCMR = 0U;
    EXPECT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_ERROR_STATE);
    EXPECT_EQ(capture.events, nullptr);
}

TEST_F(TimerHardware, BothColdConstructorsRequireActualSelectedClockGates) {
    RCC->APB1ENR = RCC->APB2ENR = 0U;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_STATE);
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_STATE);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, CaptureFailedIrqAdmissionReturnsNoBorrow) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    s_refused = &CAP_DIER;
    s_hold = 2U;
    EXPECT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_ERROR_IO);
    EXPECT_EQ(capture.events, nullptr);
    edge(88U);
    EXPECT_EQ(ring[0].captured_ticks, 0U);
    s_refused = nullptr;
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_SUCCESS);
    EXPECT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
}

TEST_F(TimerHardware, CaptureRestartCannotReplaceAnExistingBorrow) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    nx_timer_capture_event_t other[2]{};
    EXPECT_EQ(nx_timer_capture_start(&cap, other, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(capture.events, ring);
}

TEST_F(TimerHardware, CaptureAndAdvancedTaskControlsRejectIsrMutation) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    unsigned before = s_writes;
    s_isr = true;
    EXPECT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_advanced_pwm_stop(&pwm), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_advanced_pwm_set_duty(&pwm, 9U), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_advanced_pwm_clear_fault(&pwm), NX_ERROR_CONTEXT);
    EXPECT_EQ(s_writes, before);
}

TEST_F(TimerHardware, AdvancedDoesNotClearAFaultUntilHardwareAcknowledge) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ADV_SR = 128U;
    advanced.fault = true;
    s_refused = &ADV_SR;
    s_hold = 128U;
    EXPECT_EQ(nx_advanced_pwm_clear_fault(&pwm), NX_ERROR_IO);
    EXPECT_TRUE(advanced.fault);
    s_refused = nullptr;
    EXPECT_EQ(nx_advanced_pwm_clear_fault(&pwm), NX_SUCCESS);
    EXPECT_FALSE(advanced.fault);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
}

TEST_F(TimerHardware, AdvancedPhysicalActiveLowBreakUsesActualPolarity) {
    adv_config.break_active_high = false;
    BREAK_INPUT = 4096U;
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    EXPECT_EQ(ADV_BDTR & 8192U, 0U);
    ASSERT_EQ(nx_advanced_pwm_start(&pwm), NX_SUCCESS);
    BREAK_INPUT = 0U;
    ADV_IRQ(&advanced);
    EXPECT_TRUE(advanced.fault);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
}

TEST_F(TimerHardware, AdvancedRefusedDutyNeverChangesStagedValue) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    s_refused = &ADV_CCR;
    s_hold = UINT32_MAX;
    EXPECT_EQ(nx_advanced_pwm_set_duty(&pwm, 35U), NX_ERROR_IO);
    EXPECT_EQ(advanced.duty_ticks, 0U);
    EXPECT_EQ(ADV_CCR, 0U);
}

TEST_F(TimerHardware, AdvancedRefusedChannelWithdrawPreventsCleanStop) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ASSERT_EQ(nx_advanced_pwm_start(&pwm), NX_SUCCESS);
    s_refused = &ADV_CCER;
    s_hold = 5U;
    EXPECT_EQ(nx_advanced_pwm_stop(&pwm), NX_ERROR_BUSY);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
    EXPECT_EQ(ADV_CCER & 5U, 5U);
    s_refused = nullptr;
    EXPECT_EQ(nx_advanced_pwm_stop(&pwm), NX_SUCCESS);
}

TEST_F(TimerHardware, CaptureUsesClockOnlyForActualPublishedValues) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 1U), NX_SUCCESS);
    ::testing::StrictMock<::testing::MockFunction<nx_time_us_t()>> observation;
    s_observation = &observation;
    EXPECT_CALL(observation, Call()).Times(1).WillOnce(::testing::Return(812U));
    edge(5U);
    edge(6U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 5U);
    EXPECT_EQ(event.observed_at, 812U);
    s_observation = nullptr;
}

TEST_F(TimerHardware, CaptureCannotInheritGatedClockOrXorInputSelection) {
    CAP_SMCR = 7U;
    CAP_CR2 = 128U;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    EXPECT_EQ(CAP_SMCR, 0U);
    EXPECT_EQ(CAP_CR2, 0U);
}

TEST_F(TimerHardware, AdvancedCannotInheritSlaveModeOrRepetitionCounter) {
    ADV_SMCR = 7U;
    ADV_RCR = 7U;
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    EXPECT_EQ(ADV_SMCR, 0U);
    EXPECT_EQ(ADV_RCR, 0U);
}

TEST_F(TimerHardware, CaptureColdReleaseRequiresDrainAndSupportsNewSession) {
    cap_config.edge = NX_TIMER_EDGE_FALLING;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    edge(31U);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_ERROR_BUSY);
    EXPECT_TRUE(capture.initialized);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_FALSE(capture.initialized);
    EXPECT_EQ(CAP_CCER, 0U);
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
}

TEST_F(TimerHardware, AdvancedColdReleaseQuiescesSourceBeforeNewSession) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    ASSERT_EQ(nx_advanced_pwm_start(&pwm), NX_SUCCESS);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    EXPECT_FALSE(advanced.initialized);
    ADV_IRQ(&advanced);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
}

TEST_F(TimerHardware, CaptureFailedStopAcknowledgesLateSourceWithoutRingWrite) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    s_refused = &CAP_DIER;
    s_hold = 2U;
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_ERROR_BUSY);
    edge(74U);
    EXPECT_EQ(CAP_SR & 2U, 0U);
    EXPECT_EQ(capture.count, 0U);
    EXPECT_EQ(capture.events, ring);
    EXPECT_EQ(ring[0].captured_ticks, 0U);
}

TEST_F(TimerHardware, AdvancedPendingBreakBeforeMoeIsNeverExplicitlyReenabled) {
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
    s_break_before_enable = true;
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_IO);
    EXPECT_TRUE(advanced.fault);
    EXPECT_EQ(s_moe_enable_writes, 0U);
}

TEST_F(TimerHardware, CaptureReadClearPreservesNewUnreadEdge) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    s_edge_after_capture_read = true;
    edge(109U);
    EXPECT_EQ(CAP_SR & 2U, 2U);
    s_isr = true;
    CAP_IRQ(&capture);
    s_isr = false;
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 109U);
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 211U);
    EXPECT_EQ(capture.loss_incidents, 0U);
}

TEST_F(TimerHardware, CaptureObservesOvercaptureAfterActualDataRead) {
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    s_edge_before_capture_read = true;
    edge(109U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 211U);
    EXPECT_EQ(event.flags & NX_TIMER_CAPTURE_OVERCAPTURE,
              NX_TIMER_CAPTURE_OVERCAPTURE);
    EXPECT_EQ(capture.loss_incidents, 1U);
    EXPECT_EQ(CAP_SR & 512U, 0U);
}

TEST_F(TimerHardware, FailedCaptureConstructorRetainsCleanupSession) {
    cap_config.edge = NX_TIMER_EDGE_FALLING;
    s_refused = &CAP_PSC;
    s_hold = UINT32_MAX;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_IO);
    EXPECT_FALSE(capture.initialized);
    EXPECT_EQ(capture.config, &cap_config);
    EXPECT_EQ(CAP_CCER, 2U);
    EXPECT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_ERROR_INVALID);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_EQ(capture.config, nullptr);
    EXPECT_EQ(CAP_CCER, 0U);
    s_refused = nullptr;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_SUCCESS);
}

TEST_F(TimerHardware, FailedAdvancedConstructorRetainsCleanupSession) {
    s_refused = &ADV_BDTR;
    s_hold = UINT32_MAX;
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_IO);
    EXPECT_FALSE(advanced.initialized);
    EXPECT_EQ(advanced.config, &adv_config);
    EXPECT_EQ(nx_advanced_pwm_start(&pwm), NX_ERROR_INVALID);
    EXPECT_EQ(nx_advanced_pwm_set_duty(&pwm, 8U), NX_ERROR_INVALID);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    EXPECT_EQ(advanced.config, nullptr);
    EXPECT_EQ(ADV_CR1 & 1U, 0U);
    s_refused = nullptr;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_SUCCESS);
}

TEST_F(TimerHardware, FailedCaptureCleanupRetainsIdentityUntilRealReadback) {
    s_refused = &CAP_PSC;
    s_hold = UINT32_MAX;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_IO);
    CAP_DIER = 2U;
    CAP_CR1 |= 1U;
    CAP_CCER |= 1U;
    s_refused = &CAP_DIER;
    s_hold = 2U;
    EXPECT_EQ(CAP_RELEASE(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(capture.config, &cap_config);
    unsigned before = s_writes;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, before);
    s_refused = nullptr;
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_EQ(capture.config, nullptr);
    EXPECT_EQ(CAP_DIER, 0U);
    EXPECT_EQ(CAP_CCER, 0U);
    EXPECT_EQ(CAP_CR1 & 1U, 0U);
}

TEST_F(TimerHardware, FailedAdvancedCleanupRetainsIdentityUntilRealReadback) {
    s_refused = &ADV_PSC;
    s_hold = UINT32_MAX;
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_IO);
    ADV_BDTR |= 32768U;
    ADV_DIER = 128U;
    ADV_CR1 |= 1U;
    ADV_CCER = 5U;
    s_refused = &ADV_BDTR;
    s_hold = 32768U;
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_ERROR_BUSY);
    EXPECT_EQ(advanced.config, &adv_config);
    unsigned before = s_writes;
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_BUSY);
    EXPECT_EQ(s_writes, before);
    s_refused = nullptr;
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    EXPECT_EQ(advanced.config, nullptr);
    EXPECT_EQ(ADV_DIER, 0U);
    EXPECT_EQ(ADV_CCER, 0U);
    EXPECT_EQ(ADV_CR1 & 1U, 0U);
    EXPECT_EQ(ADV_BDTR & 32768U, 0U);
}

TEST_F(TimerHardware, UntouchedReleaseNeverWithdrawsForeignActiveTimer) {
    CAP_CR1 = 1U;
    CAP_DIER = 2U;
    CAP_CCER = 1U;
    ADV_CR1 = 1U;
    ADV_DIER = 128U;
    ADV_CCER = 5U;
    ADV_BDTR = 32768U;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_BUSY);
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_BUSY);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    EXPECT_EQ(CAP_CR1, 1U);
    EXPECT_EQ(CAP_DIER, 2U);
    EXPECT_EQ(CAP_CCER, 1U);
    EXPECT_EQ(ADV_CR1, 1U);
    EXPECT_EQ(ADV_DIER, 128U);
    EXPECT_EQ(ADV_CCER, 5U);
    EXPECT_EQ(ADV_BDTR, 32768U);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, UntouchedAdmissionRejectionsReleaseWithoutEffects) {
    cap_config.tick_hz = 0U;
    adv_config.tick_hz = 0U;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_INVALID);
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_INVALID);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    cap_config.tick_hz = adv_config.tick_hz = 1000000U;
    s_isr = true;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_CONTEXT);
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_CONTEXT);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_ERROR_CONTEXT);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_ERROR_CONTEXT);
    s_isr = false;
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    cap_config.exclusive = adv_config.exclusive = false;
    EXPECT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_PERMISSION);
    EXPECT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_PERMISSION);
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
    EXPECT_EQ(s_writes, 0U);
}

TEST_F(TimerHardware, FailedSessionReleaseChecksAllOwnedSourceBits) {
    s_refused = &CAP_PSC;
    s_hold = UINT32_MAX;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_IO);
    s_refused = &ADV_PSC;
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_IO);
    CAP_DIER = 4U;
    s_refused = &CAP_DIER;
    s_hold = 4U;
    EXPECT_EQ(CAP_RELEASE(&capture), NX_ERROR_BUSY);
    EXPECT_EQ(capture.config, &cap_config);
    s_refused = nullptr;
    EXPECT_EQ(CAP_RELEASE(&capture), NX_SUCCESS);
    ADV_CCER = 16U;
    s_refused = &ADV_CCER;
    s_hold = 16U;
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_ERROR_BUSY);
    EXPECT_EQ(advanced.config, &adv_config);
    s_refused = nullptr;
    EXPECT_EQ(ADV_RELEASE(&advanced), NX_SUCCESS);
}

TEST_F(TimerHardware, FailedColdCaptureStopClosesSourceBeforePublisherJoin) {
    s_refused = &CAP_PSC;
    s_hold = UINT32_MAX;
    ASSERT_EQ(CAP_INIT(&capture, &cap_config), NX_ERROR_IO);
    s_refused = nullptr;
    CAP_DIER = 4U;
    CAP_CCER = 16U;
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_SUCCESS);
    EXPECT_EQ(CAP_DIER, 0U);
    EXPECT_EQ(CAP_CCER, 0U);
    EXPECT_EQ(capture.config, &cap_config);
}

TEST_F(TimerHardware, FailedColdAdvancedStopClosesSourceBeforePublisherJoin) {
    s_refused = &ADV_PSC;
    s_hold = UINT32_MAX;
    ASSERT_EQ(ADV_INIT(&advanced, &adv_config), NX_ERROR_IO);
    s_refused = nullptr;
    ADV_DIER = 4U;
    ADV_CCER = 16U;
    EXPECT_EQ(nx_advanced_pwm_stop(&pwm), NX_SUCCESS);
    EXPECT_EQ(ADV_DIER, 0U);
    EXPECT_EQ(ADV_CCER, 0U);
    EXPECT_EQ(advanced.config, &adv_config);
}
