/**
 * \file            g474_timer_modes_test.cpp
 * \brief           Actual32-bit capture and16-bit encoder software boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_timer_modes_model.h"
#include "stm32g474_timer.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
TIM_TypeDef g_g474_mode_timers[2];
TIM_TypeDef* g_g474_mode_tim3;
TIM_TypeDef* g_g474_mode_tim5;
GPIO_TypeDef g_g474_mode_gpio;
RCC_TypeDef g_g474_mode_rcc;
bool g_g474_mode_irq_quiet;
nx_stm32g474_system_t g_nx_stm32g474_system;
}
namespace {
bool privileged;
bool in_isr;
uint32_t incoming_mask;
unsigned writes;
volatile uint32_t* refused;
uint32_t held_bits;
bool edge_after_read;
bool overcapture_after_read;
bool capture_software_trigger;
testing::MockFunction<nx_time_us_t()>* observation;
} /* namespace */

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return incoming_mask != 0U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous = {incoming_mask};
    incoming_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    incoming_mask = previous.value;
}
extern "C" void nx_arch_dsb(void) {
    /* Synchronous MMIO model: compiler ordering, no thread publication. */
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return observation == nullptr ? 700U : observation->Call();
}
extern "C" uint32_t nx_g474_mode_read(volatile uint32_t* address) {
    const uint32_t value = *address;
    if (address == &TIM5->CCR1) {
        TIM5->SR &= ~static_cast<uint32_t>(TIM_SR_CC1IF);
        if (edge_after_read) {
            TIM5->CCR1 = 0x87654321U;
            TIM5->SR |= TIM_SR_CC1IF;
            edge_after_read = false;
        }
        if (overcapture_after_read) {
            TIM5->SR |= TIM_SR_CC1OF;
            overcapture_after_read = false;
        }
    }
    return value;
}
extern "C" void nx_g474_mode_write(volatile uint32_t* address, uint32_t value) {
    ++writes;
    if (address == refused) {
        value = (value & ~held_bits) | (*address & held_bits);
    }
    if (address == &TIM3->CR1 && (TIM3->SMCR & TIM_SMCR_SMS) >= 1U &&
        (TIM3->SMCR & TIM_SMCR_SMS) <= 3U) {
        /* RM0440 DIR is read-only while the quadrature personality owns it. */
        value = (value & ~static_cast<uint32_t>(TIM_CR1_DIR)) |
                (*address & TIM_CR1_DIR);
    }
    if (address == &TIM3->SR || address == &TIM5->SR) {
        *address &= value;
        return;
    }
    *address = value;
    for (auto& timer : g_g474_mode_timers) {
        if (address == &timer.EGR && (value & TIM_EGR_UG) != 0U &&
            (timer.CR1 & TIM_CR1_UDIS) == 0U) {
            if (&timer == TIM5 && (timer.CR2 & TIM_CR2_MMS) == 0U) {
                capture_software_trigger = true;
            }
            timer.CNT = 0U;
            timer.SR |= TIM_SR_UIF;
        }
    }
}

class G474TimerModes : public testing::Test {
  protected:
    nx_timer_capture_controller_t capture{};
    nx_timer_capture_config_t cap_config{170000000U, 1000000U,
                                         NX_TIMER_EDGE_RISING, 0U, true};
    const nx_timer_capture_t cap{&nx_stm32g474_timer_capture_ops, &capture};
    nx_timer_capture_event_t ring[2]{};
    nx_stm32g474_encoder_state_t encoder{};
    nx_stm32g474_encoder_config_t enc_config{
        NX_ENCODER_MODE_X4_TI12, 0U, 0U, false, false, true};
    const nx_encoder_t enc{&nx_stm32g474_encoder_ops, &encoder};
    void SetUp() override {
        g_g474_mode_tim3 = &g_g474_mode_timers[0];
        g_g474_mode_tim5 = &g_g474_mode_timers[1];
        std::memset(g_g474_mode_timers, 0, sizeof(g_g474_mode_timers));
        std::memset(GPIOA, 0, sizeof(*GPIOA));
        std::memset(RCC, 0, sizeof(*RCC));
        std::memset(&g_nx_stm32g474_system, 0, sizeof(g_nx_stm32g474_system));
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000U;
        g_nx_stm32g474_system.rcc = RCC;
        RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN;
        RCC->APB1ENR1 = RCC_APB1ENR1_TIM3EN | RCC_APB1ENR1_TIM5EN;
        GPIOA->MODER = 2U | (2U << 12U) | (2U << 14U);
        GPIOA->AFR[0] = 2U | (2U << 24U) | (2U << 28U);
        privileged = true;
        in_isr = false;
        incoming_mask = 0U;
        writes = 0U;
        refused = nullptr;
        held_bits = 0U;
        edge_after_read = false;
        overcapture_after_read = false;
        capture_software_trigger = false;
        observation = nullptr;
        g_g474_mode_irq_quiet = true;
    }
    void TearDown() override {
        refused = nullptr;
        incoming_mask = 0U;
        privileged = true;
        in_isr = false;
        g_g474_mode_irq_quiet = true;
        if (capture.config != nullptr) {
            (void)nx_timer_capture_stop(&cap);
            nx_timer_capture_event_t event{};
            while (nx_timer_capture_read(&cap, &event) == NX_SUCCESS) {
            }
            EXPECT_EQ(nx_stm32g474_timer_capture_release(&capture), NX_SUCCESS);
        }
        if (encoder.config != nullptr) {
            EXPECT_EQ(nx_stm32g474_encoder_close(&encoder), NX_SUCCESS);
        }
    }
    void initialize_capture() {
        ASSERT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
                  NX_SUCCESS);
        ASSERT_EQ(nx_timer_capture_start(&cap, ring, 2U), NX_SUCCESS);
    }
    void capture_edge(uint32_t count) {
        TIM5->CCR1 = count;
        TIM5->SR |= TIM_SR_CC1IF;
        nx_stm32g474_timer_capture_irq(&capture);
    }
};

TEST_F(G474TimerModes, Tim5CapturesEveryHighBitAndReportsItsActualWidth) {
    initialize_capture();
    capture_edge(0xFEDCBA98U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 0xFEDCBA98U);
    EXPECT_EQ(event.observed_at, 700U);
    nx_timer_capture_state_t state{};
    ASSERT_EQ(nx_timer_capture_state(&cap, &state), NX_SUCCESS);
    EXPECT_EQ(state.counter_bits, 32U);
    EXPECT_EQ(state.tick_hz, 1000000U);
    EXPECT_EQ(TIM5->ARR, UINT32_MAX);
    EXPECT_EQ(TIM5->PSC, 169U);
}

TEST_F(G474TimerModes, RawFullWidthWrapNeverInventsAnAbsolutePosition) {
    initialize_capture();
    capture_edge(0xFFFFFFFEU);
    capture_edge(1U);
    nx_timer_capture_event_t first{}, second{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &first), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_read(&cap, &second), NX_SUCCESS);
    EXPECT_EQ(first.captured_ticks, 0xFFFFFFFEU);
    EXPECT_EQ(second.captured_ticks, 1U);
    EXPECT_EQ(second.captured_ticks - first.captured_ticks, 3U);
}

TEST_F(G474TimerModes, NewUnreadEdgeSurvivesThePreviousCaptureReadClear) {
    initialize_capture();
    edge_after_read = true;
    capture_edge(0x12345678U);
    EXPECT_NE(TIM5->SR & TIM_SR_CC1IF, 0U);
    nx_stm32g474_timer_capture_irq(&capture);
    nx_timer_capture_event_t first{}, second{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &first), NX_SUCCESS);
    ASSERT_EQ(nx_timer_capture_read(&cap, &second), NX_SUCCESS);
    EXPECT_EQ(first.captured_ticks, 0x12345678U);
    EXPECT_EQ(second.captured_ticks, 0x87654321U);
}

TEST_F(G474TimerModes, CaptureObservesOvercaptureAfterCcrAndPreservesOldRing) {
    initialize_capture();
    overcapture_after_read = true;
    capture_edge(100U);
    capture_edge(200U);
    capture_edge(300U);
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 100U);
    EXPECT_NE(event.flags & NX_TIMER_CAPTURE_OVERCAPTURE, 0U);
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(event.captured_ticks, 200U);
    nx_timer_capture_state_t state{};
    ASSERT_EQ(nx_timer_capture_state(&cap, &state), NX_SUCCESS);
    EXPECT_EQ(state.loss_incidents, 2U);
}

TEST_F(G474TimerModes, CaptureRejectsMissingClockResetAndForeignVector) {
    RCC->APB1ENR1 &= ~static_cast<uint32_t>(RCC_APB1ENR1_TIM5EN);
    EXPECT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_ERROR_STATE);
    EXPECT_EQ(writes, 0U);
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM5EN;
    RCC->APB1RSTR1 |= RCC_APB1RSTR1_TIM5RST;
    EXPECT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_ERROR_STATE);
    EXPECT_EQ(writes, 0U);
    RCC->APB1RSTR1 = 0U;
    g_g474_mode_irq_quiet = false;
    EXPECT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_ERROR_BUSY);
    EXPECT_EQ(writes, 0U);
    EXPECT_EQ(capture.config, nullptr);
}

TEST_F(G474TimerModes, CaptureCannotBorrowMmioAsEventRingOrOutput) {
    ASSERT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_SUCCESS);
    const unsigned before = writes;
    auto* registers = reinterpret_cast<nx_timer_capture_event_t*>(TIM5);
    EXPECT_EQ(nx_timer_capture_start(&cap, registers, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(writes, before);
    EXPECT_EQ(capture.events, nullptr);
    EXPECT_EQ(nx_timer_capture_state(
                  &cap, reinterpret_cast<nx_timer_capture_state_t*>(RCC)),
              NX_ERROR_INVALID);
    EXPECT_EQ(writes, before);
}

TEST_F(G474TimerModes, CaptureStopRetainsQueuedLoanAndRefusedIrqSource) {
    initialize_capture();
    capture_edge(8U);
    refused = &TIM5->DIER;
    held_bits = TIM_DIER_CC1IE;
    incoming_mask = 1U;
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_ERROR_BUSY);
    EXPECT_EQ(incoming_mask, 1U);
    EXPECT_EQ(capture.events, ring);
    refused = nullptr;
    incoming_mask = 0U;
    nx_timer_capture_event_t event{};
    ASSERT_EQ(nx_timer_capture_read(&cap, &event), NX_SUCCESS);
    EXPECT_EQ(nx_timer_capture_stop(&cap), NX_SUCCESS);
    g_g474_mode_irq_quiet = false;
    EXPECT_EQ(nx_stm32g474_timer_capture_release(&capture), NX_ERROR_BUSY);
    EXPECT_NE(capture.config, nullptr);
    g_g474_mode_irq_quiet = true;
    EXPECT_EQ(nx_stm32g474_timer_capture_release(&capture), NX_SUCCESS);
}

TEST_F(G474TimerModes, CaptureRequiresFreshReloadBeforeReportingReady) {
    TIM5->SR = TIM_SR_UIF;
    refused = &TIM5->EGR;
    held_bits = UINT32_MAX;
    EXPECT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_ERROR_IO);
    EXPECT_EQ(capture.config, &cap_config);
    EXPECT_FALSE(capture.initialized);
}

TEST_F(G474TimerModes, CaptureSoftwareReloadKeepsDownstreamTriggerQuiet) {
    ASSERT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_SUCCESS);
    EXPECT_FALSE(capture_software_trigger);
}

TEST_F(G474TimerModes, CaptureCannotAdoptARefusedDownCounterControl) {
    TIM5->CR1 = TIM_CR1_DIR;
    refused = &TIM5->CR1;
    held_bits = TIM_CR1_DIR;
    EXPECT_EQ(nx_stm32g474_timer_capture_initialize(&capture, &cap_config),
              NX_ERROR_IO);
    EXPECT_EQ(capture.config, &cap_config);
    EXPECT_FALSE(capture.initialized);
}

TEST_F(G474TimerModes, CaptureProvesClockBeforeAnyPeripheralRegisterRead) {
    EXPECT_EXIT(
        {
            void* inaccessible = mmap(nullptr, 4096U, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (inaccessible == MAP_FAILED) {
                std::_Exit(2);
            }
            g_g474_mode_tim5 = static_cast<TIM_TypeDef*>(inaccessible);
            RCC->APB1ENR1 &= ~static_cast<uint32_t>(RCC_APB1ENR1_TIM5EN);
            const auto result =
                nx_stm32g474_timer_capture_initialize(&capture, &cap_config);
            std::_Exit(result == NX_ERROR_STATE ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(G474TimerModes, EncoderProgramsRealQuadratureModesAndInputFilters) {
    for (unsigned mode = 1U; mode <= 3U; ++mode) {
        enc_config.mode = static_cast<nx_encoder_mode_t>(mode);
        enc_config.filter_a = 3U;
        enc_config.filter_b = 12U;
        enc_config.invert_a = true;
        ASSERT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
                  NX_SUCCESS);
        EXPECT_EQ(TIM3->SMCR & TIM_SMCR_SMS, mode);
        EXPECT_EQ((TIM3->CCMR1 >> 4U) & 15U, 3U);
        EXPECT_EQ((TIM3->CCMR1 >> 12U) & 15U, 12U);
        EXPECT_EQ(TIM3->CCMR1 & (TIM_CCMR1_CC1S | TIM_CCMR1_CC2S),
                  TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0);
        EXPECT_EQ(TIM3->PSC, 0U);
        EXPECT_EQ(TIM3->ARR, 65535U);
        EXPECT_EQ(TIM3->DIER, 0U);
        ASSERT_EQ(nx_encoder_start(&enc), NX_SUCCESS);
        EXPECT_EQ(TIM3->CCER, TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC1P);
        ASSERT_EQ(nx_stm32g474_encoder_close(&encoder), NX_SUCCESS);
    }
}

TEST_F(G474TimerModes, EncoderReadReportsOnlyRawCounterAndObservationTime) {
    ASSERT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(&enc), NX_SUCCESS);
    TIM3->CNT = 65535U;
    testing::StrictMock<testing::MockFunction<nx_time_us_t()>> clock;
    EXPECT_CALL(clock, Call()).WillOnce(testing::Return(701U));
    observation = &clock;
    nx_encoder_sample_t sample{};
    ASSERT_EQ(nx_encoder_read(&enc, &sample), NX_SUCCESS);
    observation = nullptr;
    EXPECT_EQ(sample.count, 65535U);
    EXPECT_EQ(sample.observed_at, 701U);
    nx_encoder_state_t state{};
    ASSERT_EQ(nx_encoder_state(&enc, &state), NX_SUCCESS);
    EXPECT_EQ(state.counter_bits, 16U);
    EXPECT_EQ(state.filter_clock_hz, 170000000U);
    EXPECT_EQ(state.mode, NX_ENCODER_MODE_X4_TI12);
    EXPECT_TRUE(state.running);
}

TEST_F(G474TimerModes, EncoderRejectsUnknownModeClock2AndBadInputRoute) {
    enc_config.mode = NX_ENCODER_MODE_UNKNOWN;
    EXPECT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_ERROR_INVALID);
    EXPECT_EQ(writes, 0U);
    enc_config.mode = NX_ENCODER_MODE_X4_TI12;
    TIM3->SMCR = TIM_SMCR_ECE;
    EXPECT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_ERROR_BUSY);
    EXPECT_EQ(writes, 0U);
    TIM3->SMCR = 0U;
    GPIOA->AFR[0] &= ~(15U << 24U);
    EXPECT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_ERROR_STATE);
    EXPECT_EQ(writes, 0U);
}

TEST_F(G474TimerModes, EncoderRefusedDetachRetainsSessionAndCounter) {
    ASSERT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(&enc), NX_SUCCESS);
    TIM3->CNT = 123U;
    refused = &TIM3->CCER;
    held_bits = TIM_CCER_CC1E | TIM_CCER_CC2E;
    EXPECT_EQ(nx_encoder_stop(&enc), NX_ERROR_BUSY);
    EXPECT_NE(encoder.config, nullptr);
    EXPECT_EQ(nx_stm32g474_encoder_close(&encoder), NX_ERROR_BUSY);
    EXPECT_EQ(TIM3->CNT, 123U);
    refused = nullptr;
    EXPECT_EQ(nx_encoder_stop(&enc), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, 123U);
    EXPECT_EQ(nx_stm32g474_encoder_close(&encoder), NX_SUCCESS);
}

TEST_F(G474TimerModes, EncoderColdReuseLeavesTheEndedDirectionPersonality) {
    ASSERT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(&enc), NX_SUCCESS);
    TIM3->CR1 |= TIM_CR1_DIR;
    TIM3->CNT = 65535U;
    EXPECT_EQ(nx_encoder_stop(&enc), NX_SUCCESS);
    EXPECT_EQ(TIM3->CNT, 65535U);
    EXPECT_EQ(nx_stm32g474_encoder_close(&encoder), NX_SUCCESS);
    EXPECT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_SUCCESS);
}

TEST_F(G474TimerModes, CaptureRetainedStopProvesClockBeforeRegisterAccess) {
    initialize_capture();
    EXPECT_EXIT(
        {
            void* inaccessible = mmap(nullptr, 4096U, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (inaccessible == MAP_FAILED) {
                std::_Exit(2);
            }
            g_g474_mode_tim5 = static_cast<TIM_TypeDef*>(inaccessible);
            RCC->APB1ENR1 &= ~static_cast<uint32_t>(RCC_APB1ENR1_TIM5EN);
            const auto result = nx_timer_capture_stop(&cap);
            const bool retained = capture.config == &cap_config &&
                                  capture.events == ring && capture.running;
            std::_Exit(result == NX_ERROR_STATE && retained ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(G474TimerModes, EncoderProvesClockBeforeColdAndLiveRegisterReads) {
    EXPECT_EXIT(
        {
            void* inaccessible = mmap(nullptr, 4096U, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (inaccessible == MAP_FAILED) {
                std::_Exit(2);
            }
            g_g474_mode_tim3 = static_cast<TIM_TypeDef*>(inaccessible);
            RCC->APB1ENR1 &= ~static_cast<uint32_t>(RCC_APB1ENR1_TIM3EN);
            const auto result =
                nx_stm32g474_encoder_initialize(&encoder, &enc_config);
            std::_Exit(result == NX_ERROR_STATE ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
    ASSERT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_SUCCESS);
    ASSERT_EQ(nx_encoder_start(&enc), NX_SUCCESS);
    EXPECT_EXIT(
        {
            void* inaccessible = mmap(nullptr, 4096U, PROT_NONE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (inaccessible == MAP_FAILED) {
                std::_Exit(2);
            }
            g_g474_mode_tim3 = static_cast<TIM_TypeDef*>(inaccessible);
            RCC->APB1ENR1 &= ~static_cast<uint32_t>(RCC_APB1ENR1_TIM3EN);
            nx_encoder_sample_t sample{};
            sample.observed_at = 10U;
            sample.count = 20U;
            const auto result = nx_encoder_read(&enc, &sample);
            std::_Exit(result == NX_ERROR_STATE && sample.count == 20U ? 0 : 3);
        },
        testing::ExitedWithCode(0), "");
}

TEST_F(G474TimerModes, EncoderFailedColdWritesRetainOnlyTheAcquiredSession) {
    refused = &TIM3->CCMR1;
    held_bits = UINT32_MAX;
    EXPECT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_ERROR_IO);
    EXPECT_EQ(encoder.config, &enc_config);
    EXPECT_FALSE(encoder.initialized);
    refused = nullptr;
    EXPECT_EQ(nx_stm32g474_encoder_close(&encoder), NX_SUCCESS);
    EXPECT_EQ(encoder.config, nullptr);
}

TEST_F(G474TimerModes, EncoderRejectsMmioAliasesAndUnavailableContext) {
    ASSERT_EQ(nx_stm32g474_encoder_initialize(&encoder, &enc_config),
              NX_SUCCESS);
    EXPECT_EQ(
        nx_encoder_read(&enc, reinterpret_cast<nx_encoder_sample_t*>(TIM3)),
        NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_encoder_state(&enc, reinterpret_cast<nx_encoder_state_t*>(&encoder)),
        NX_ERROR_INVALID);
    const unsigned before = writes;
    in_isr = true;
    EXPECT_EQ(nx_encoder_start(&enc), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_encoder_stop(&enc), NX_ERROR_CONTEXT);
    EXPECT_EQ(writes, before);
}

TEST(EncoderDispatch, InvalidAndMissingMethodsDoNotAccessProviderState) {
    EXPECT_EQ(nx_encoder_start(nullptr), NX_ERROR_INVALID);
    const nx_encoder_ops_t missing{};
    unsigned context = 0U;
    const nx_encoder_t port{&missing, &context};
    EXPECT_EQ(nx_encoder_start(&port), NX_ERROR_UNSUPPORTED);
    nx_encoder_sample_t sample{10U, 20U};
    EXPECT_EQ(nx_encoder_read(&port, &sample), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(sample.count, 20U);
    EXPECT_EQ(context, 0U);
}
