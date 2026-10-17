/**
 * \file            h743_high_kernel_test.cpp
 * \brief           H743 joint mux lease and existing provider kernel paths
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_high_fixture.h"
#include "nexus/core/fault.h"
#include <array>
#include <cstdlib>
#include <gmock/gmock.h>
extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    ADD_FAILURE() << "Unexpected terminal provider fault in finite test";
    std::abort();
}
class H743HighMux : public H743HighReference {};
TEST_F(H743HighMux, DomainOracleDoesNotUseTheSoftwareCoreFrequencyNumber) {
    ASSERT_EQ(NX_SUCCESS, StartClock().primary);
    g_nx_stm32h743_system.core_hz = 64000000U;
    const auto c = nx_h743_high_model_clocks();
    EXPECT_EQ(384000000U, c.cpu_hz);
    EXPECT_EQ(192000000U, c.hclk_hz);
    EXPECT_EQ(96000000U, c.pclk_hz[0]);
    EXPECT_EQ(192000000U, c.tim2_hz);
    g_nx_stm32h743_system.core_hz = 384000000U;
}
TEST_F(H743HighMux, ConservativeSourceEndpointsKeepKernelAndBusBoundsDistinct) {
    ASSERT_EQ(NX_SUCCESS, StartClock().primary);
    g_h743_model.rcc.D2CCIP2R = KernelD2();
    g_h743_high_behavior.hsi_input_hz = 65000000U;
    g_h743_high_behavior.pll_scale_ppm = 1020000U;
    auto c = nx_h743_high_model_clocks();
    EXPECT_EQ(397800000U, c.cpu_hz);
    EXPECT_EQ(198900000U, c.hclk_hz);
    EXPECT_EQ(99450000U, c.spi4_hz);
    EXPECT_EQ(65000000U, c.usart3_hz);
    EXPECT_EQ(65000000U, c.i2c1_hz);
    g_h743_high_behavior.hsi_input_hz = 62000000U;
    g_h743_high_behavior.pll_scale_ppm = 980000U;
    c = nx_h743_high_model_clocks();
    EXPECT_EQ(364560000U, c.cpu_hz);
    EXPECT_EQ(182280000U, c.hclk_hz);
    EXPECT_EQ(91140000U, c.spi4_hz);
    EXPECT_EQ(62000000U, c.usart3_hz);
    EXPECT_EQ(62000000U, c.i2c1_hz);
    g_h743_model.rcc.D2CCIP2R = 0U;
}
TEST_F(H743HighMux, TimerInputOracleDistinguishesCpu383FromTimer191Prescalers) {
    ASSERT_EQ(NX_SUCCESS, StartClock().primary);
    g_h743_model.rcc.APB1LENR = RCC_APB1LENR_TIM2EN;
    g_h743_model.timer.CR1 = TIM_CR1_CEN;
    g_h743_model.timer.CR2 = TIM_CR2_MMS_0;
    g_h743_model.timer.PSC = 383U;
    NX_H743_WRITE(&g_h743_model.timer.EGR, TIM_EGR_UG);
    nx_h743_high_model_advance_hclk(192000U);
    EXPECT_EQ(500U, g_h743_model.timer.CNT);
    g_h743_model.timer.CNT = 0U;
    g_h743_model.timer.PSC = 191U;
    NX_H743_WRITE(&g_h743_model.timer.EGR, TIM_EGR_UG);
    g_h743_high_behavior.timer_input_remainder = 0U;
    nx_h743_high_model_advance_hclk(192000U);
    EXPECT_EQ(1000U, g_h743_model.timer.CNT);
    /* Withdraw only this external model-oracle fixture. The separate System
     * target must establish/withdraw those registers through production. */
    g_h743_model.timer.CR1 = 0U;
    g_h743_model.timer.CR2 = 0U;
    g_h743_model.timer.SR = 0U;
    g_h743_model.rcc.APB1LENR = 0U;
}
TEST_F(H743HighMux,
       OneColdOwnerSelectsIndependentKernelSourcesBeforeConsumers) {
    ASSERT_EQ(NX_SUCCESS, StartClock().primary);
    EXPECT_EQ(KernelD2(), g_h743_model.rcc.D2CCIP2R);
    EXPECT_EQ(RCC_D3CCIPR_ADCSEL_1, g_h743_model.rcc.D3CCIPR);
    EXPECT_EQ(0U, g_h743_model.rcc.D1CCIPR & RCC_D1CCIPR_CKPERSEL);
    const auto c = nx_h743_high_model_clocks();
    EXPECT_EQ(384000000U, c.cpu_hz);
    EXPECT_EQ(192000000U, c.hclk_hz);
    EXPECT_EQ(64000000U, c.usart3_hz);
    EXPECT_EQ(64000000U, c.i2c1_hz);
    EXPECT_EQ(96000000U, c.spi4_hz);
    EXPECT_EQ(64000000U, c.adc12_input_hz);
    EXPECT_EQ(0U, c.adc_hz);
    EXPECT_EQ(0U, g_h743_high_behavior.mux_writes_with_consumers);
    EXPECT_EQ(4U, sizeof(owner));
}
TEST_F(H743HighMux, PartialMuxWriteFailureRollsBackEveryEarlierOwnedStage) {
    g_h743_model.ignore_write = &g_h743_model.rcc.D2CCIP2R;
    const auto result = StartClock();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.rcc.D3CCIPR & RCC_D3CCIPR_ADCSEL);
    EXPECT_EQ(0U, g_h743_model.rcc.D2CCIP2R);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    g_h743_model.ignore_write = nullptr;
}
TEST_F(H743HighMux, RefusedMuxRestoreRetainsJointOwnerAndCanBeRetried) {
    ASSERT_EQ(NX_SUCCESS, StartClock().primary);
    g_h743_model.ignore_write = &g_h743_model.rcc.D2CCIP2R;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_pll384_stop(&owner, 8U));
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(KernelD2(), g_h743_model.rcc.D2CCIP2R);
    g_h743_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
    EXPECT_EQ(0U, g_h743_model.rcc.D2CCIP2R);
    EXPECT_EQ(0U, g_h743_model.rcc.D3CCIPR & RCC_D3CCIPR_ADCSEL);
}
TEST_F(H743HighMux, ForeignOwnedMuxValueIsNotClobberedDuringWithdrawal) {
    ASSERT_EQ(NX_SUCCESS, StartClock().primary);
    g_h743_model.rcc.D2CCIP2R =
        (KernelD2() & ~(uint32_t)RCC_D2CCIP2R_USART28SEL) |
        RCC_D2CCIP2R_USART28SEL_0;
    const uint32_t foreign = g_h743_model.rcc.D2CCIP2R;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_pll384_stop(&owner, 8U));
    EXPECT_NE(0U, owner.remaining_effects);
    EXPECT_EQ(foreign, g_h743_model.rcc.D2CCIP2R);
    g_h743_model.rcc.D2CCIP2R = KernelD2();
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
}
TEST_F(H743HighMux, EachOwnedSelectorMayAlreadyHaveReturnedToItsBaseline) {
    for (uint32_t mask : {uint32_t(RCC_D2CCIP2R_USART28SEL),
                          uint32_t(RCC_D2CCIP2R_I2C123SEL)}) {
        ASSERT_EQ(NX_SUCCESS, StartClock().primary);
        g_h743_model.rcc.D2CCIP2R &= ~mask;
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
        EXPECT_EQ(0U, owner.remaining_effects);
        EXPECT_EQ(0U, g_h743_model.rcc.D2CCIP2R);
        if (owner.remaining_effects != 0U) {
            /* Retire an intended RED's real lease after recording failure. */
            g_h743_model.rcc.D2CCIP2R = KernelD2();
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
        }
    }
}
TEST_F(H743HighMux, PartialSelectorAcquireFailureRollsBackTheAcceptedNeighbor) {
    g_h743_high_behavior.refused_kernel_field = RCC_D2CCIP2R_I2C123SEL;
    const auto result = StartClock();
    EXPECT_EQ(NX_ERROR_IO, result.primary);
    EXPECT_EQ(NX_SUCCESS, result.cleanup);
    EXPECT_EQ(0U, result.remaining_effects);
    EXPECT_EQ(0U, owner.remaining_effects);
    EXPECT_EQ(0U, g_h743_model.rcc.D2CCIP2R);
    EXPECT_EQ(FLASH_ACR_LATENCY_7WS, g_h743_model.flash.ACR);
    g_h743_high_behavior.refused_kernel_field = 0U;
    if (owner.remaining_effects != 0U) {
        g_h743_model.rcc.D2CCIP2R = KernelD2();
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
    }
}

class H743HighUart : public H743HighReference {
  protected:
    USART_TypeDef registers{};
    nx_stm32h743_uart_state_t state{};
    std::array<uint8_t, 8U> ring{};
    void SetUp() override {
        H743HighReference::SetUp();
        ASSERT_EQ(NX_SUCCESS, StartClock().primary);
        PublishIsolatedProviderPrerequisites();
        g_h743_model.rcc.APB1LENR |= RCC_APB1LENR_USART3EN;
        g_h743_model.uart = &registers;
        state.registers = &registers;
        state.rx_storage = ring.data();
        state.rx_capacity = ring.size();
        state.baud = 115200U;
        state.irq = USART3_IRQn;
        state.profile = NX_UART_RX_BYTES;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U};
    }
    void TearDown() override {
        if (state.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_uart_ops.stop(&state));
        }
        g_h743_model.rcc.APB1LENR &= ~(uint32_t)RCC_APB1LENR_USART3EN;
        WithdrawIsolatedKernelFixture();
        H743HighReference::TearDown();
    }
};
TEST_F(H743HighUart, HsiKernelUsesItsOwnBrrInsteadOfCpuOrPclk) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_uart_initialize(&state, 64000000U, 8U));
    EXPECT_EQ(556U, registers.BRR);
    EXPECT_EQ(64000000U, nx_h743_high_model_clocks().usart3_hz);
    EXPECT_EQ(96000000U, nx_h743_high_model_clocks().pclk_hz[0]);
    EXPECT_TRUE(state.initialized);
}
TEST_F(H743HighUart, AnApbFrequencyArgumentCannotOverrideTheSelectedHsiSource) {
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32h743_uart_initialize(&state, 96000000U, 8U));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743HighUart, WrongKernelMuxRejectsBeforeUeOrRingMutation) {
    g_h743_model.rcc.D2CCIP2R &= ~(uint32_t)RCC_D2CCIP2R_USART28SEL;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_stm32h743_uart_initialize(&state, 64000000U, 8U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, registers.CR1);
    g_h743_model.rcc.D2CCIP2R = KernelD2();
}

class H743HighI2c : public H743HighReference {
  protected:
    void SetUp() override {
        H743HighReference::SetUp();
        ASSERT_EQ(NX_SUCCESS, StartClock().primary);
        PublishIsolatedProviderPrerequisites();
        g_h743_model.rcc.APB1LENR |= RCC_APB1LENR_I2C1EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    }
    void TearDown() override {
        g_h743_model.rcc.APB1LENR &= ~(uint32_t)RCC_APB1LENR_I2C1EN;
        g_h743_model.rcc.AHB4ENR &= ~(uint32_t)RCC_AHB4ENR_GPIOBEN;
        WithdrawIsolatedKernelFixture();
        H743HighReference::TearDown();
    }
};
TEST_F(H743HighI2c,
       HsiKernelAndSeparatePclkAdmitOnlyTheSelectedOpenDrainRoute) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_i2c_pins_prepare());
    EXPECT_EQ(64000000U, nx_h743_high_model_clocks().i2c1_hz);
    EXPECT_EQ(96000000U, nx_h743_high_model_clocks().pclk_hz[0]);
    EXPECT_EQ(3U << 8U, g_h743_model.gpio_b.OTYPER & (3U << 8U));
    EXPECT_EQ(0x44U, g_h743_model.gpio_b.AFR[1] & 0xffU);
}
TEST_F(H743HighI2c, PclkFallbackCannotMasqueradeAsTheHsiTimingInput) {
    g_h743_model.rcc.D2CCIP2R &= ~(uint32_t)RCC_D2CCIP2R_I2C123SEL;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_i2c_pins_prepare());
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.rcc.D2CCIP2R = KernelD2();
}

class H743HighSpi : public H743HighReference {
  protected:
    nx_stm32h743_gpio_state_t cs{};
    nx_stm32h743_spi_state_t state{};
    nx_stm32h743_spi_endpoint_state_t endpoint{};
    nx_spi_endpoint_t device{&nx_stm32h743_spi_endpoint_ops, &endpoint};
    void SetUp() override {
        H743HighReference::SetUp();
        ASSERT_EQ(NX_SUCCESS, StartClock().primary);
        PublishIsolatedProviderPrerequisites();
        g_h743_model.rcc.APB2ENR |= RCC_APB2ENR_SPI4EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
        g_h743_model.spi = &g_h743_model.spi_registers;
        cs = {&g_h743_model.gpio_e, 1U << 4U, true, false, false};
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_gpio_initialize(&cs, cs.mask));
        state.registers = g_h743_model.spi;
        state.reset = &g_h743_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI4RST;
        state.clock_hz = 96000000U;
        state.poll_limit = 32U;
        endpoint = {&state, &cs, cs.mask, 1500000U, 0U};
    }
    void TearDown() override {
        if (state.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_ops.stop(&state));
        }
        if (cs.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_gpio_stop(&cs, cs.mask));
        }
        g_h743_model.rcc.APB2ENR &= ~(uint32_t)RCC_APB2ENR_SPI4EN;
        g_h743_model.rcc.AHB4ENR &= ~(uint32_t)RCC_AHB4ENR_GPIOEEN;
        WithdrawIsolatedKernelFixture();
        H743HighReference::TearDown();
    }
};
TEST_F(H743HighSpi, ApbKernelUsesNinetySixMhzInsteadOfCpu384) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    EXPECT_EQ(96000000U, nx_h743_high_model_clocks().spi4_hz);
    EXPECT_EQ(96000000U, state.clock_hz);
}
TEST_F(H743HighSpi, MaximumSourceBoundSelectsDiv128RatherThanNominalDiv64) {
    g_h743_high_behavior.hsi_input_hz = 65000000U;
    g_h743_high_behavior.pll_scale_ppm = 1020000U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    uint8_t tx = 0x6aU;
    size_t transferred = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U,
                                                   10000U, &transferred));
    EXPECT_EQ(128U, g_h743_high_behavior.spi_started_divisor);
    /* Nominal96MHz/64 exactly1.5MHz hides the99.45MHz upper bound.
     * Div128 keeps the whole local source envelope under caller maximum. */
    EXPECT_LE(99450000U, endpoint.frequency_hz * 128U);
    EXPECT_GT(99450000U, endpoint.frequency_hz * 64U);
    const auto c = nx_h743_high_model_clocks();
    EXPECT_EQ(99450000U, c.spi4_hz);
    const uint32_t divisor = g_h743_high_behavior.spi_started_divisor;
    EXPECT_LE(c.spi4_hz, endpoint.frequency_hz * divisor);
    EXPECT_EQ(1U, transferred);
    EXPECT_FALSE(g_h743_model.spi_tail_truncated);
    EXPECT_NE(0U, cs.registers->ODR & cs.mask);
}
TEST_F(H743HighSpi, ForeignKernelSelectorRejectsBeforeControllerReset) {
    g_h743_model.rcc.D2CCIP1R = RCC_D2CCIP1R_SPI45SEL_0;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_initialize(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.rcc.D2CCIP1R = 0U;
}

class H743HighPwm : public H743HighReference {
  protected:
    nx_stm32h743_gpio_state_t idle{};
    nx_stm32h743_pwm_state_t state{};
    void SetUp() override {
        H743HighReference::SetUp();
        ASSERT_EQ(NX_SUCCESS, StartClock().primary);
        PublishIsolatedProviderPrerequisites();
        g_h743_model.rcc.APB1LENR |= RCC_APB1LENR_TIM3EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
        g_h743_model.pwm = &g_h743_model.pwm_registers;
        idle = {&g_h743_model.gpio_a, 1U << 6U, true, false, false};
        state.registers = g_h743_model.pwm;
        state.idle = &idle;
        state.reset = &g_h743_model.rcc.APB1LRSTR;
        state.reset_mask = RCC_APB1LRSTR_TIM3RST;
        state.period_ticks = 1000U;
        state.duty_ticks = 250U;
        state.tick_hz = 1000000U;
        state.poll_limit = 8U;
        state.pin = 6U;
    }
    void TearDown() override {
        if (state.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pwm_close(&state));
        }
        g_h743_model.rcc.APB1LENR &= ~(uint32_t)RCC_APB1LENR_TIM3EN;
        g_h743_model.rcc.AHB4ENR &= ~(uint32_t)RCC_AHB4ENR_GPIOAEN;
        WithdrawIsolatedKernelFixture();
        H743HighReference::TearDown();
    }
};
TEST_F(H743HighPwm, TimerMultiplierProducesPsc191RatherThanCpu383OrPclk95) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pwm_initialize(&state));
    EXPECT_EQ(192000000U, nx_h743_high_model_clocks().tim3_hz);
    EXPECT_EQ(191U, state.registers->PSC);
    EXPECT_EQ(999U, state.registers->ARR);
}
TEST_F(H743HighPwm, AForeignTimpreCannotSilentlyChangeThePwmTickDomain) {
    g_h743_model.rcc.CFGR |= RCC_CFGR_TIMPRE;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_pwm_initialize(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.rcc.CFGR &= ~(uint32_t)RCC_CFGR_TIMPRE;
}

class H743HighAdc : public H743HighReference {
  protected:
    nx_stm32h743_adc_state_t state{};
    void SetUp() override {
        H743HighReference::SetUp();
        auto& adc = g_h743_model.adc_registers;
        adc.CR = g_h743_model.adc_peer.CR = ADC_CR_DEEPPWD;
        adc.CFGR = ADC_CFGR_JQDIS;
        state.registers = &adc;
        state.peer = &g_h743_model.adc_peer;
        state.common = &g_h743_model.adc_common;
        state.gpio = &g_h743_model.gpio_a;
        state.reference_mv = state.supply_mv = 3300U;
        state.poll_limit = 64U;
        g_h743_model.adc = &adc;
    }
    void Boot(uint32_t revision = 0x2003U) {
        ASSERT_EQ(NX_SUCCESS, StartClock(revision).primary);
        PublishIsolatedProviderPrerequisites();
        g_h743_model.rcc.AHB1ENR |= RCC_AHB1ENR_ADC12EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    }
    void TearDown() override {
        if (state.retained) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_adc_stop(&state));
        }
        g_h743_model.rcc.AHB1ENR &= ~(uint32_t)RCC_AHB1ENR_ADC12EN;
        g_h743_model.rcc.AHB4ENR &= ~(uint32_t)RCC_AHB4ENR_GPIOAEN;
        WithdrawIsolatedKernelFixture();
        H743HighReference::TearDown();
    }
};
TEST_F(H743HighAdc, UnreviewedCutsCannotAcquireClockOrAdcPrerequisites) {
    for (uint32_t revision : {0x1003U, 0x2001U}) {
        const uint32_t writes = g_h743_model.writes;
        const auto result = StartClock(revision);
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, result.primary);
        EXPECT_EQ(NX_SUCCESS, result.cleanup);
        EXPECT_EQ(0U, result.remaining_effects);
        EXPECT_EQ(0U, owner.remaining_effects);
        EXPECT_EQ(writes, g_h743_model.writes);
        EXPECT_EQ(0U, g_h743_model.rcc.AHB1ENR & RCC_AHB1ENR_ADC12EN);
        EXPECT_EQ(ADC_CR_DEEPPWD, state.registers->CR);
        EXPECT_FALSE(state.retained);
        if (owner.remaining_effects != 0U) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pll384_stop(&owner, 8U));
        }
    }
}
TEST_F(H743HighAdc, ReviewedVSelectsAsyncDiv4AndItsOwnBoostEncoding) {
    Boot();
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_adc_initialize(&state));
    EXPECT_EQ(ADC_CR_BOOST_1, state.registers->CR & ADC_CR_BOOST);
    EXPECT_EQ(ADC_CCR_PRESC_1,
              state.common->CCR & (ADC_CCR_CKMODE | ADC_CCR_PRESC));
    EXPECT_EQ(16000000U, nx_h743_high_model_clocks().adc_hz);
    EXPECT_EQ(64000000U, nx_h743_high_model_clocks().adc12_input_hz);
    EXPECT_EQ(0U, g_h743_high_behavior.adc_enables_outside_profile);
}
TEST_F(H743HighAdc, ClockOracleExposesAnUnchangedHclkDiv4PathAsFortyEightMhz) {
    Boot();
    state.common->CCR = (ADC_CCR_CKMODE_1 | ADC_CCR_CKMODE_0);
    NX_H743_WRITE(&state.registers->CR, ADC_CR_ADVREGEN | ADC_CR_ADEN);
    EXPECT_EQ(48000000U, nx_h743_high_model_clocks().adc_hz);
    EXPECT_EQ(1U, g_h743_high_behavior.adc_enables_outside_profile);
    /* External sensitivity:48MHz violates the selected HSI64/4 profile.
     * This does not claim48MHz always exceeds the V-cut silicon limit. */
    NX_H743_WRITE(&state.registers->CR, ADC_CR_DEEPPWD);
    state.common->CCR = 0U;
}
TEST_F(H743HighAdc,
       ForeignPerClockSelectorRejectsBeforeRegulatorOrCalibration) {
    Boot();
    g_h743_model.rcc.D1CCIPR = RCC_D1CCIPR_CKPERSEL_0;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_adc_initialize(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_FALSE(state.retained);
    g_h743_model.rcc.D1CCIPR = 0U;
}

class H743HighFlash : public H743HighReference {
  protected:
    alignas(uint32_t) std::array<uint8_t, 2097152U> memory{};
    nx_stm32h743_flash_state_t state{};
    nx_flash_port_t face{&nx_stm32h743_flash_ops, &state};
    void SetUp() override {
        H743HighReference::SetUp();
        ASSERT_EQ(NX_SUCCESS, StartClock().primary);
        PublishIsolatedProviderPrerequisites();
        memory.fill(0xffU);
        state.registers = &g_h743_model.flash;
        state.memory = memory.data();
        ASSERT_EQ(0U, reinterpret_cast<uintptr_t>(state.memory) %
                          alignof(uint32_t));
        state.supply_mv = 3300U;
        state.poll_limit = 8U;
        g_h743_model.physical_flash = &state;
        state.registers->CR1 = state.registers->CR2 = FLASH_CR_LOCK;
    }
    void TearDown() override {
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_stop(&state));
        WithdrawIsolatedKernelFixture();
        H743HighReference::TearDown();
    }
};
TEST_F(H743HighFlash, ExactVos1Hclk192TwoWsWr2AllowsAnActualFlashword) {
    std::array<uint8_t, 32U> input{};
    input.fill(0x55U);
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 131072U, input.data(),
                                                input.size(), 10000U));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    EXPECT_EQ(0x55U, memory[131072U]);
    EXPECT_EQ(0U, g_h743_high_behavior.unsafe_flash_unlocks);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1,
              state.registers->ACR);
}
TEST_F(H743HighFlash,
       ExactProfileAllowsPhysicalBankTwoEraseWithoutRetuningAcr) {
    memory[1048576U] = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 1048576U, 131072U, 10000U));
    EXPECT_EQ(0xffU, memory[1048576U]);
    EXPECT_EQ(FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1,
              state.registers->ACR);
    EXPECT_EQ(0U, g_h743_high_behavior.unsafe_flash_unlocks);
}
TEST_F(H743HighFlash, ReadLatencyCannotExcuseAnUnsafeProgrammingDelay) {
    state.registers->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_0;
    std::array<uint8_t, 32U> input{};
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    state.registers->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1;
}
TEST_F(H743HighFlash, CorrectWrDelayCannotExcuseAnUnsafeCpuReadLatency) {
    state.registers->ACR = FLASH_ACR_LATENCY_1WS | FLASH_ACR_WRHIGHFREQ_1;
    std::array<uint8_t, 32U> input{};
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    state.registers->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_WRHIGHFREQ_1;
}
TEST_F(H743HighFlash, UnreadyActualVosRejectsBeforeUnlockOrDataStore) {
    const uint32_t csr = g_h743_model.power.CSR1;
    g_h743_model.power.CSR1 &= ~(uint32_t)PWR_CSR1_ACTVOSRDY;
    std::array<uint8_t, 32U> input{};
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    g_h743_model.power.CSR1 = csr;
}
TEST_F(H743HighFlash, Cpu384DoesNotAuthorizeForeignHclkOrFlashGeometry) {
    const uint32_t d1 = g_h743_model.rcc.D1CFGR;
    g_h743_model.rcc.D1CFGR &= ~(uint32_t)RCC_D1CFGR_HPRE;
    std::array<uint8_t, 32U> input{};
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.rcc.D1CFGR = d1;
}
TEST_F(H743HighFlash, CacheOnFlashRemainsExplicitlyUnsupportedBeforeAnyPulse) {
    g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
    std::array<uint8_t, 32U> input{};
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, 131072U, input.data(), input.size(),
                                    10000U));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    g_h743_model.core->CCR &= ~(uint32_t)SCB_CCR_DC_Msk;
}
