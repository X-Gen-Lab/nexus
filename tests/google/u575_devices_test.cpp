/**
 * \file            u575_devices_test.cpp
 * \brief           U575 fixed timer ownership, duty and failure contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_cold.h"
#include "stm32u575_provider.h"
#include "u575_devices_model.h"
#include <cstring>
#include <gtest/gtest.h>
class U575Pwm : public testing::Test {
  protected:
    TIM_TypeDef timer{};
    nx_stm32u575_gpio_state_t pin{};
    nx_stm32u575_pwm_state_t state{};
    nx_pwm_port_t face{&nx_stm32u575_pwm_ops, &state};
    void SetUp() override {
        nx_u575_devices_reset();
        g_u575_pwm = &timer;
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_TIM3EN;
        pin.registers = NX_U575_GPIOA;
        pin.mask = 1U << 6U;
        pin.output = true;
        state.registers = &timer;
        state.idle = &pin;
        state.reset = &g_u575_model.rcc.APB1RSTR1;
        state.reset_mask = RCC_APB1RSTR1_TIM3RST;
        state.period_ticks = 1000U;
        state.duty_ticks = 250U;
        state.tick_hz = 1000000U;
        state.poll_limit = 4U;
        state.pin = 6U;
    }
    void TearDown() override {
        g_u575_pwm = nullptr;
    }
};
TEST_F(U575Pwm, FixedBaseAndSafeOutputPrecedeCounterAndAlternateMode) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_pwm_initialize(&state));
    EXPECT_EQ(3U, timer.PSC);
    EXPECT_EQ(999U, timer.ARR);
    EXPECT_EQ(0U, timer.CCER);
    EXPECT_EQ(1U, (pin.registers->MODER >> 12U) & 3U);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    EXPECT_EQ(2U, (pin.registers->MODER >> 12U) & 3U);
    EXPECT_EQ(2U, (pin.registers->AFR[0] >> 24U) & 15U);
    EXPECT_EQ(TIM_CCER_CC1E, timer.CCER);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_stop(&face));
    EXPECT_EQ(0U, timer.CCER);
    EXPECT_EQ(0U, pin.registers->ODR & pin.mask);
    EXPECT_EQ(1U, (pin.registers->MODER >> 12U) & 3U);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_pwm_close(&state));
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Pwm, ZeroAndFullDutyNeverChangeSharedBase) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_pwm_initialize(&state));
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 0U));
    EXPECT_EQ(0U, timer.CCR1);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 1000U));
    EXPECT_EQ(1000U, timer.CCR1);
    EXPECT_EQ(NX_ERROR_STATE, nx_pwm_port_set(&face, 500U, 1U));
    EXPECT_EQ(999U, timer.ARR);
}
TEST_F(U575Pwm, ForeignTimerRejectsBeforeHardwareEffects) {
    timer.SMCR = TIM_SMCR_SMS_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_pwm_initialize(&state));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Pwm, RefusedStopRetainsPinAndTimerUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_pwm_initialize(&state));
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    g_u575_model.ignore_write = &timer.CCER;
    EXPECT_EQ(NX_ERROR_IO, nx_pwm_port_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.fault);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_pwm_close(&state));
}
class U575I2c : public testing::Test {
  protected:
    nx_i2c_v2_registers_t registers{};
    nx_i2c_v2_config_t config{};
    nx_stm32u575_i2c_state_t state{};
    nx_stm32u575_i2c_clock_t clock{};
    void SetUp() override {
        nx_u575_devices_reset();
        g_u575_i2c = &registers;
        state.config = &config;
        config.timing = 0x10f42f2fU;
        config.ignore_master_berr = true;
        config.registers = &registers;
        config.reset = &g_u575_model.rcc.APB1RSTR1;
        config.reset_mask = RCC_APB1RSTR1_I2C1RST;
        config.poll_limit = 4U;
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
    }
    void TearDown() override {
        g_u575_i2c = nullptr;
    }
};
TEST_F(U575I2c, IndependentHsi16ClockAndFiniteReloadPreserveSystemTime) {
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_i2c_initialize(&state));
    EXPECT_EQ(0U, registers.cr1);
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    g_u575_model.rcc.CCIPR1 = RCC_CCIPR1_I2C1SEL_0;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_i2c_initialize(&state));
    EXPECT_EQ(0U, registers.cr1);
    g_u575_model.rcc.CCIPR1 = 0U;
    const auto system_clock = g_u575_model.rcc.CFGR1;
    const auto time_prescaler = g_u575_model.timer.PSC;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_start(&clock, &state));
    EXPECT_EQ(RCC_CR_HSION | RCC_CR_HSIRDY,
              g_u575_model.rcc.CR & (RCC_CR_HSION | RCC_CR_HSIRDY));
    EXPECT_EQ(RCC_CCIPR1_I2C1SEL_1,
              g_u575_model.rcc.CCIPR1 & RCC_CCIPR1_I2C1SEL);
    EXPECT_EQ(system_clock, g_u575_model.rcc.CFGR1);
    EXPECT_EQ(time_prescaler, g_u575_model.timer.PSC);
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_i2c_initialize(&state));
    EXPECT_TRUE(config.ignore_master_berr);
    EXPECT_FALSE(config.receive_reload_one);
    EXPECT_EQ(0x10f42f2fU, registers.timing);
    EXPECT_EQ(1U, registers.cr1);
    nx_i2c_port_t face{&nx_i2c_v2_ops, &state};
    nx_stm32u575_i2c_endpoint_state_t endpoint{&state, 0x50U};
    nx_i2c_endpoint_t child{&nx_i2c_v2_endpoint_ops, &endpoint};
    uint8_t tx = 0x12U;
    uint8_t rx[256]{};
    registers.rxdr = 0x5aU;
    nx_i2c_message_t messages[2]{{&tx, 1U, false}, {rx, sizeof(rx), true}};
    g_u575_i2c_berr = true;
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_i2c_endpoint_transaction(&child, messages, 2U, 100U, &count));
    EXPECT_EQ(257U, count);
    EXPECT_EQ(0x5aU, rx[255]);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_i2c_clock_stop(&clock));
    EXPECT_EQ(NX_SUCCESS, nx_i2c_port_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_stop(&clock));
    EXPECT_EQ(0U, g_u575_model.rcc.CR & (RCC_CR_HSION | RCC_CR_HSIRDY));
    EXPECT_EQ(0U, g_u575_model.rcc.CCIPR1);
    EXPECT_FALSE(clock.retained);
}
TEST_F(U575I2c, ExplicitTimingReachesActualRegistersAndSurvivesRecovery) {
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_start(&clock, &state));
    constexpr uint32_t timing = 0x20f41f1fU;
    config.timing = timing;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_i2c_initialize_timing(&state, timing));
    EXPECT_EQ(timing, registers.timing);
    EXPECT_EQ(timing, config.timing);
    nx_i2c_port_t face{&nx_i2c_v2_ops, &state};
    ASSERT_EQ(NX_SUCCESS, nx_i2c_port_recover(&face));
    EXPECT_EQ(timing, registers.timing);
    nx_stm32u575_i2c_endpoint_state_t endpoint{&state, 0x50U};
    nx_i2c_long_endpoint_t child{&nx_i2c_v2_long_endpoint_ops, &endpoint};
    uint8_t rx[511]{};
    registers.rxdr = 0x5aU;
    nx_i2c_message_t message{rx, sizeof(rx), true};
    size_t done = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_i2c_long_endpoint_transaction(&child, &message, 1U,
                                                           1000000U, &done));
    EXPECT_EQ(511U, done);
    EXPECT_EQ(0x5aU, rx[510]);
    EXPECT_EQ(NX_SUCCESS, nx_i2c_port_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_stop(&clock));
}
TEST_F(U575I2c, ReservedTimingBitsRejectBeforeAnyPeripheralWrite) {
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32u575_i2c_initialize_timing(&state, 0x0f000000U));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575I2c, ForeignSourceOrSelectorRejectWithoutClockEffects) {
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    for (uint32_t foreign : {static_cast<uint32_t>(RCC_CR_HSION),
                             static_cast<uint32_t>(RCC_CR_HSIKERON),
                             static_cast<uint32_t>(RCC_CR_HSIRDY)}) {
        g_u575_model.rcc.CR |= foreign;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_i2c_clock_start(&clock, &state));
        EXPECT_EQ(0U, g_u575_model.writes);
        EXPECT_FALSE(clock.retained);
        g_u575_model.rcc.CR &= ~foreign;
    }
    g_u575_model.rcc.CCIPR1 = RCC_CCIPR1_I2C1SEL_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_i2c_clock_start(&clock, &state));
    EXPECT_EQ(0U, g_u575_model.writes);
    g_u575_model.rcc.CCIPR1 = 0U;
    g_u575_model.rcc.ICSCR3 ^= RCC_ICSCR3_HSITRIM_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_i2c_clock_start(&clock, &state));
    EXPECT_EQ(0U, g_u575_model.writes);
}
TEST_F(U575I2c, FailedSourceStartRemainsReleasableWithoutChangingSystemClock) {
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    g_u575_hsi_hold_on = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32u575_i2c_clock_start(&clock, &state));
    EXPECT_TRUE(clock.retained);
    EXPECT_EQ(0U, registers.cr1);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_stop(&clock));
    EXPECT_FALSE(clock.retained);
    EXPECT_EQ(0U, g_u575_model.rcc.CFGR1);
    EXPECT_EQ(0U, g_u575_model.rcc.CR & RCC_CR_HSION);
}
TEST_F(U575I2c, FailedSelectorAndSourceWithdrawalRetainIndependentLease) {
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    g_u575_model.ignore_write = &g_u575_model.rcc.CCIPR1;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_i2c_clock_start(&clock, &state));
    EXPECT_TRUE(clock.retained);
    g_u575_model.ignore_write = nullptr;
    g_u575_hsi_hold_off = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32u575_i2c_clock_stop(&clock));
    EXPECT_TRUE(clock.retained);
    g_u575_hsi_hold_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_stop(&clock));
    EXPECT_FALSE(clock.retained);
}
TEST_F(U575I2c, FailedPeripheralSetupDrainsBeforeClockRelease) {
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_start(&clock, &state));
    g_u575_model.ignore_write = &registers.timing;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_i2c_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_i2c_clock_stop(&clock));
    nx_i2c_port_t face{&nx_i2c_v2_ops, &state};
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_i2c_port_stop(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_clock_stop(&clock));
}
TEST_F(U575I2c, FailedColdPrepareKeepsConfigurationUntilClockRelease) {
    g_u575_model.rcc.APB1ENR1 = RCC_APB1ENR1_I2C1EN;
    g_u575_hsi_hold_on = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_stm32u575_i2c_prepare(&state, &clock, &config));
    EXPECT_TRUE(clock.retained);
    EXPECT_FALSE(state.retained);
    auto replacement = config;
    replacement.timing = 0x20f41f1fU;
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_stm32u575_i2c_prepare(&state, &clock, &replacement));
    EXPECT_EQ(&config, state.config);
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_close(&state, &clock));
    EXPECT_FALSE(clock.retained);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_i2c_close(&state, &clock));
}
class U575Spi : public testing::Test {
  protected:
    SPI_TypeDef registers{};
    nx_stm32u575_spi_state_t state{};
    nx_stm32u575_gpio_state_t cs{};
    nx_stm32u575_spi_endpoint_state_t endpoint{};
    nx_spi_port_t face{&nx_stm32u575_spi_ops, &state};
    nx_spi_endpoint_t device{&nx_stm32u575_spi_endpoint_ops, &endpoint};
    void SetUp() override {
        nx_u575_devices_reset();
        g_u575_spi_hold = false;
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.APB2ENR = RCC_APB2ENR_SPI1EN;
        g_u575_spi = &registers;
        registers.SR = SPI_SR_TXP | SPI_SR_TXC;
        registers.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
        state.registers = &registers;
        state.reset = &g_u575_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI1RST;
        state.clock_hz = 4000000U;
        state.poll_limit = 4U;
        cs = {&g_u575_model.gpioa, 16U, true, false, false};
        endpoint = {&state, &cs, 16U, 1000000U, 0U};
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_gpio_initialize(&cs, 16U));
    }
    void TearDown() override {
        g_u575_spi = nullptr;
    }
};
TEST_F(U575Spi, ColdAdmissionUsesV3CfgAndNoDmaOrInterrupt) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_initialize(&state));
    EXPECT_EQ(7U | (7U << SPI_CFG1_CRCSIZE_Pos), registers.CFG1);
    EXPECT_EQ(0U, registers.IER);
    EXPECT_EQ(0U, registers.CR1);
}
TEST_F(U575Spi, V3TsizeAndByteAccessCompleteThenWithdrawBeforeCs) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_initialize(&state));
    uint8_t tx[2] = {0x12U, 0x34U};
    uint8_t rx[2]{};
    size_t transferred = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, tx, rx, 2U, 10000U,
                                                   &transferred));
    EXPECT_EQ(2U, transferred);
    EXPECT_EQ(0xedU, rx[0]);
    EXPECT_EQ(0xcbU, rx[1]);
    EXPECT_EQ(0U, registers.CR1 & SPI_CR1_SPE);
    EXPECT_NE(0U, cs.registers->ODR & cs.mask);
    EXPECT_GE(g_u575_spi_reset_gap, 2U);
}
TEST_F(U575Spi, StalledByteResetsControllerAndRequiresExplicitRecovery) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_initialize(&state));
    g_u575_spi_hold = true;
    uint8_t tx = 12U;
    size_t transferred = 99U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U, 100U,
                                       &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_TRUE(state.fault);
    EXPECT_NE(0U, cs.registers->ODR & cs.mask);
    g_u575_spi_hold = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&face));
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U,
                                                   100U, &transferred));
}
TEST_F(U575Spi, ForeignKernelSelectorRejectsWithoutChangingTheDomain) {
    g_u575_model.rcc.CCIPR1 = RCC_CCIPR1_SPI1SEL_0;
    uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_spi_initialize(&state));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Spi, AliasedOrUnboundedRequestsRejectBeforeChipSelect) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_initialize(&state));
    size_t transferred = 17U;
    uint8_t tx = 12U;
    uint32_t writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U,
                                       NX_DEADLINE_NEVER, &transferred));
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_transfer(
                                    &device, reinterpret_cast<uint8_t*>(&state),
                                    nullptr, 1U, 100U, &transferred));
    EXPECT_EQ(writes, g_u575_model.writes);
}
TEST_F(U575Spi, RefusedResetKeepsPortResponsibilityForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_initialize(&state));
    registers.CR1 = SPI_CR1_SPE;
    g_u575_model.ignore_write = state.reset;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&face));
    EXPECT_FALSE(state.retained);
}
class U575Exti : public testing::Test {
  protected:
    nx_exti_event_t storage[1]{};
    nx_stm32u575_exti_state_t state{};
    nx_exti_port_t face{&nx_stm32u575_exti_ops, &state};
    void SetUp() override {
        nx_u575_devices_reset();
        std::memset(&g_u575_exti, 0, sizeof(g_u575_exti));
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.AHB2ENR1 = RCC_AHB2ENR1_GPIOAEN;
        state.registers = EXTI;
        state.mux = EXTI;
        state.gpio = GPIOA;
        state.storage = storage;
        state.capacity = 1U;
        state.line = 1U;
        state.priority = 5U;
        state.edge = NX_EXTI_BOTH;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 126U, 4U, 0U};
    }
};
TEST_F(U575Exti, UsesEightBitMuxStrideAndSeparatePendingBanks) {
    EXTI->EXTICR[0] = 0x34560207U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_exti_initialize(&state));
    EXPECT_EQ(0x34560007U, EXTI->EXTICR[0]);
    EXTI->RPR1 = 3U;
    EXTI->FPR1 = 2U;
    GPIOA->IDR = 2U;
    nx_stm32u575_exti_irq(&state);
    EXPECT_EQ(1U, EXTI->RPR1);
    EXPECT_EQ(0U, EXTI->FPR1);
    nx_exti_event_t event{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, &event, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(NX_EXTI_RISING, event.edge);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, event.flags);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_EQ(0x34560207U, EXTI->EXTICR[0]);
}
TEST_F(U575Exti, OverflowIsReportedAfterOlderBufferedFact) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_exti_initialize(&state));
    EXTI->RPR1 = 2U;
    nx_stm32u575_exti_irq(&state);
    EXTI->FPR1 = 2U;
    nx_stm32u575_exti_irq(&state);
    nx_exti_event_t event[2]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&face, event, 2U, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, event[0].flags);
    EXPECT_EQ(NX_EXTI_EVENT_LOSS, event[1].flags);
}
TEST_F(U575Exti, RefusedWithdrawalRetainsRingAndSavedMux) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_exti_initialize(&state));
    g_u575_model.ignore_write = &EXTI->IMR1;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.closing);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(state.retained);
}
class U575Adc : public testing::Test {
  protected:
    ADC_TypeDef registers{};
    ADC_Common_TypeDef common{};
    nx_stm32u575_adc_state_t state{};
    nx_adc_port_t face{&nx_stm32u575_adc_ops, &state};
    void SetUp() override {
        nx_u575_devices_reset();
        g_u575_adc = &registers;
        g_u575_adc_common = &common;
        g_u575_adc_hold = false;
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.AHB3ENR = RCC_AHB3ENR_ADC4EN | RCC_AHB3ENR_PWREN;
        g_u575_model.power.SVMCR = PWR_SVMCR_ASV;
        state.registers = &registers;
        state.common = &common;
        state.reference_mv = 3300U;
        state.poll_limit = 8U;
    }
    void TearDown() override {
        g_u575_adc = nullptr;
        g_u575_adc_common = nullptr;
    }
};
TEST_F(U575Adc, ColdPrepareBindsResourcesAndRejectsLiveRebinding) {
    const nx_stm32u575_adc_config_t config{&registers, &common, 3300U, 8U};
    state = {};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_adc_prepare(&state, &config));
    ADC_TypeDef foreign{};
    auto other = config;
    other.registers = &foreign;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_adc_prepare(&state, &other));
    EXPECT_EQ(&registers, state.registers);
    EXPECT_EQ(0U, foreign.CR);
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_adc_close(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_adc_prepare(&state, &config));
}
TEST_F(U575Adc, ColdPrepareRejectsInvalidResourcesBeforeAnalogEffects) {
    const nx_stm32u575_adc_config_t invalid{&registers, &common, 0U, 8U};
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_adc_prepare(nullptr, &invalid));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_adc_prepare(&state, nullptr));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_adc_prepare(&state, &invalid));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Adc, ColdInitializationConfiguresOnlyPc0Analog) {
    GPIOC->MODER = 0xA5A5A5A0U;
    GPIOC->PUPDR = 0x5A5A5A53U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(0xA5A5A5A3U, GPIOC->MODER);
    EXPECT_EQ(0x5A5A5A50U, GPIOC->PUPDR);
}
TEST_F(U575Adc, RefusedAnalogPadRetainsCleanupBeforeStartingRegulator) {
    GPIOC->MODER = 0U;
    g_u575_model.ignore_write = &GPIOC->MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_adc_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.initialized);
    EXPECT_EQ(0U, registers.CR);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_adc_close(&state));
}
TEST_F(U575Adc, CalibratesBeforeReadyAndSamplesOnlyReviewedChannel) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(2U, registers.CHSELR);
    EXPECT_EQ(7U, registers.SMPR1);
    EXPECT_EQ(0U, registers.IER);
    EXPECT_EQ(0U, registers.CFGR1);
    uint16_t sample = 0U;
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_adc_port_sample(&face, &sample, 1U, 100U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0xabcU, sample);
    EXPECT_EQ(0U, registers.CR & (ADC_CR_ADSTART | ADC_CR_ADSTP));
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_adc_close(&state));
}
TEST_F(U575Adc, MissingAnalogSupplyRejectsWithoutGuessingVoltage) {
    g_u575_model.power.SVMCR = 0U;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Adc, ReferenceBelowExactAdc4MinimumRejectsBeforeWrites) {
    state.reference_mv = 999U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Adc, TimeoutAbortsAndCountsNoUnobservedSample) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_adc_initialize(&state));
    g_u575_adc_hold = true;
    uint16_t sample = 0x55U;
    size_t count = 7U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_adc_port_sample(&face, &sample, 1U, 100U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0x55U, sample);
    EXPECT_EQ(0U, registers.CR & (ADC_CR_ADSTART | ADC_CR_ADSTP));
}
TEST_F(U575Adc, ForeignDmaOrTriggerModeRejectsBeforeWrites) {
    registers.CFGR1 = ADC4_CFGR1_DMAEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(0U, g_u575_model.writes);
}
TEST_F(U575Adc, ForeignAdc12OwnerCannotJoinTheFixedAdc4ClockContract) {
    g_u575_model.rcc.AHB2ENR1 |= RCC_AHB2ENR1_ADC12EN;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(0U, g_u575_model.writes);
}
TEST_F(U575Adc, RefusedCalibrationCannotPublishAnUncalibratedConverter) {
    g_u575_adc_refuse_calibration = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_adc_initialize(&state));
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_adc_close(&state));
}
TEST_F(U575Adc, StaleRegulatorAndReadyFlagsMustClearBeforeAcquisition) {
    registers.ISR = ADC_ISR_LDORDY | ADC_ISR_ADRDY;
    g_u575_model.ignore_write = &registers.ISR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_adc_initialize(&state));
    EXPECT_EQ(0U, registers.CR);
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_adc_close(&state));
}
TEST_F(U575Adc, StaleSampleCompletionCannotAuthorizeAnOldSample) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_adc_initialize(&state));
    registers.ISR |= ADC_ISR_EOC | ADC_ISR_EOS;
    g_u575_model.ignore_write = &registers.ISR;
    uint16_t sample = 0x55U;
    size_t count = 7U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_adc_port_sample(&face, &sample, 1U, 100U, &count));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(0x55U, sample);
    EXPECT_EQ(0U, registers.CR & ADC_CR_ADSTART);
    EXPECT_FALSE(state.active);
}
alignas(16) static uint8_t physical_flash[2097152];
class U575Flash : public testing::Test {
  protected:
    nx_stm32u575_flash_state_t state{};
    nx_flash_port_t face{&nx_stm32u575_flash_ops, &state};
    void SetUp() override {
        nx_u575_devices_reset();
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_physical_flash = &g_u575_model.flash;
        g_u575_flash_memory = physical_flash;
        g_u575_flash_words = g_u575_flash_polls = g_u575_flash_erase = 0U;
        std::memset(physical_flash, 0xff, sizeof(physical_flash));
        g_u575_model.flash.NSCR = FLASH_NSCR_LOCK | FLASH_NSCR_OPTLOCK;
        state.registers = &g_u575_model.flash;
        state.memory = physical_flash;
    }
    void TearDown() override {
        g_u575_physical_flash = nullptr;
    }
};
TEST_F(U575Flash, GeometryCoversBothBanksWithoutProductReservations) {
    const auto* geometry = nx_flash_port_geometry(&face);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(2097152U, geometry->size);
    EXPECT_EQ(16U, geometry->program_unit);
    EXPECT_EQ(NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE, geometry->program_flags);
    ASSERT_EQ(256U, geometry->sector_count);
    EXPECT_EQ(8192U, geometry->sectors[255].size);
    EXPECT_EQ(2097152U - 8192U, geometry->sectors[255].offset);
}
TEST_F(U575Flash, InvalidAlignmentOrExpiredDeadlineNeverUnlocks) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    uint8_t data[16]{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 1U, data, sizeof(data), 100U));
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_flash_port_program(&face, 0U, data, sizeof(data), 0U));
    EXPECT_EQ(0U, state.registers->NSKEYR);
    EXPECT_EQ(0U, g_u575_flash_words);
}
TEST_F(U575Flash, ForeignFlashInterruptOwnerRejectsBeforeControllerEffects) {
    g_u575_model.irq_enabled[FLASH_IRQn] = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_flash_initialize(&state));
    EXPECT_EQ(0U, g_u575_model.writes);
    EXPECT_FALSE(state.retained);
    EXPECT_EQ(0U, state.registers->NSKEYR);
}
TEST_F(U575Flash, QuadWordUsesFourWordsAndSettlesPulseBeforeRelocking) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    g_u575_model.priority[FLASH_IRQn] = 9U;
    uint8_t data[16] = {1U, 2U, 3U, 4U};
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, 0U, data, sizeof(data), 100U));
    EXPECT_EQ(4U, g_u575_flash_words);
    EXPECT_GE(g_u575_flash_polls, 3U);
    EXPECT_EQ(0, std::memcmp(data, physical_flash, sizeof(data)));
    EXPECT_NE(0U, state.registers->NSCR & FLASH_NSCR_LOCK);
    EXPECT_EQ(0U, state.registers->NSSR & FLASH_NSSR_BSY);
    EXPECT_EQ(0U, state.registers->NSCR & FLASH_NSCR_EOPIE);
    EXPECT_FALSE(g_u575_model.irq_enabled[FLASH_IRQn]);
    EXPECT_FALSE(g_u575_model.irq_pending[FLASH_IRQn]);
    EXPECT_EQ(9U, g_u575_model.priority[FLASH_IRQn]);
    EXPECT_FALSE(state.active);
}
TEST_F(U575Flash, RefusedPendingClearRetainsLeaseAndBlocksReuseUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    g_u575_model.hold_irq_off = true;
    uint8_t data[16]{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_program(&face, 0U, data, sizeof(data), 100U));
    EXPECT_FALSE(state.initialized);
    EXPECT_TRUE(state.retained);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(0U, state.registers->NSCR & FLASH_NSCR_EOPIE);
    EXPECT_NE(0U, state.registers->NSCR & FLASH_NSCR_LOCK);
    EXPECT_TRUE(g_u575_model.irq_pending[FLASH_IRQn]);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_flash_initialize(&state));
    g_u575_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_flash_close(&state));
    EXPECT_FALSE(g_u575_model.irq_pending[FLASH_IRQn]);
    EXPECT_FALSE(state.retained);
}
TEST_F(U575Flash, AllErasedInputSkipsHiddenEccProgrammingPulses) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    uint8_t data[32];
    std::memset(data, 0xff, sizeof(data));
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, 0U, data, sizeof(data), 100U));
    EXPECT_EQ(0U, g_u575_flash_words);
    EXPECT_EQ(0U, g_u575_flash_polls);
}
TEST_F(U575Flash,
       VisiblyProgrammedUnitRejectsBeforeUnlockWithoutProvingEccHistory) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    physical_flash[0] = 0U;
    uint8_t data[16]{};
    EXPECT_EQ(NX_ERROR_STATE,
              nx_flash_port_program(&face, 0U, data, sizeof(data), 100U));
    EXPECT_EQ(0U, g_u575_flash_words);
    EXPECT_EQ(0U, state.registers->NSKEYR);
}
TEST_F(U575Flash, BankTwoPageZeroAndTimeoutStillDrainCurrentPulse) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    physical_flash[1048576U] = 0U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_flash_port_erase(&face, 1048576U, 8192U, 1U));
    EXPECT_NE(0U, g_u575_flash_erase & FLASH_NSCR_BKER);
    EXPECT_EQ(0U, g_u575_flash_erase & FLASH_NSCR_PNB);
    EXPECT_EQ(0xffU, physical_flash[1048576U]);
    EXPECT_GE(g_u575_flash_polls, 3U);
    EXPECT_EQ(0U, state.registers->NSSR & FLASH_NSSR_BSY);
}
TEST_F(U575Flash, RefusedEraseStartCannotReportSuccessFromIdleStatus) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    physical_flash[0] = 0U;
    g_u575_flash_ignore_start = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 8192U, 100U));
    EXPECT_EQ(0U, physical_flash[0]);
    EXPECT_NE(0U, state.registers->NSCR & FLASH_NSCR_LOCK);
    EXPECT_FALSE(state.active);
}
TEST_F(U575Flash, RefusedStaleCompletionClearRejectsBeforeUnlock) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    state.registers->NSSR = FLASH_NSSR_EOP;
    g_u575_flash_hold_eop_clear = true;
    g_u575_flash_ignore_start = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, 8192U, 100U));
    EXPECT_EQ(0U, state.registers->NSKEYR);
    EXPECT_EQ(0U, g_u575_flash_erase);
}
TEST_F(U575Flash, SettledPulseRequiresFreshCompletionEvenWhenReadbackMatches) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_flash_initialize(&state));
    g_u575_flash_omit_eop = true;
    uint8_t data[16]{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_program(&face, 0U, data, sizeof(data), 100U));
    EXPECT_EQ(0U, state.registers->NSSR & FLASH_NSSR_BSY);
    EXPECT_NE(0U, state.registers->NSCR & FLASH_NSCR_LOCK);
}

TEST_F(U575Spi, RejectedLiveChipSelectNeverAcquiresTheWire) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_spi_endpoint_state_t child{&cold, &cs, 16U, 1000000U, 0U};
    nx_stm32u575_spi_endpoint_config_t endpoint_config{
        &cs, &child, &g_u575_model.gpioa, 16U, 1000000U, 0U};
    nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 5U, 5U};
    nx_stm32u575_spi_config_t config{&endpoint_config, 1U, &pin, 1U};
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32u575_spi_prepare(&cold, &config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(cold.retained);
    EXPECT_FALSE(cold.initialized);
}

TEST_F(U575Spi, InvalidLaterEndpointRejectsBeforeAnyAcquisition) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_gpio_state_t first{};
    nx_stm32u575_spi_endpoint_state_t child{&cold, &first, 16U, 1000000U, 0U};
    nx_stm32u575_spi_endpoint_config_t endpoints[]{
        {&first, &child, &g_u575_model.gpioa, 16U, 1000000U, 0U},
        {nullptr, nullptr, &g_u575_model.gpioa, 32U, 1000000U, 0U}};
    nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 5U, 5U};
    nx_stm32u575_spi_config_t config{endpoints, 2U, &pin, 1U};
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_spi_prepare(&cold, &config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(cold.retained);
    EXPECT_FALSE(first.retained);
}

TEST_F(U575Spi, RejectedLiveChipSelectIsNeverReleasedByColdCleanup) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_spi_endpoint_state_t child{&cold, &cs, 16U, 1000000U, 0U};
    nx_stm32u575_spi_endpoint_config_t endpoint_config{
        &cs, &child, &g_u575_model.gpioa, 16U, 1000000U, 0U};
    nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 5U, 5U};
    nx_stm32u575_spi_config_t config{&endpoint_config, 1U, &pin, 1U};
    ASSERT_EQ(NX_ERROR_BUSY, nx_stm32u575_spi_prepare(&cold, &config));
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_spi_release(&cold, &config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_TRUE(cs.initialized);
    EXPECT_TRUE(cs.retained);
}

TEST_F(U575Spi, PartialColdCleanupSkipsEveryUnacquiredLaterEndpoint) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_gpio_state_t first{};
    nx_stm32u575_gpio_state_t later{};
    nx_stm32u575_spi_endpoint_state_t children[]{
        {&cold, &first, 32U, 1000000U, 0U}, {&cold, &later, 64U, 1000000U, 0U}};
    nx_stm32u575_spi_endpoint_config_t endpoints[]{
        {&first, &children[0], &g_u575_model.gpioa, 32U, 1000000U, 0U},
        {&later, &children[1], &g_u575_model.gpioa, 64U, 1000000U, 0U}};
    nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 7U, 5U};
    nx_stm32u575_spi_config_t config{endpoints, 2U, &pin, 1U};
    g_u575_model.ignore_write = &g_u575_model.gpioa.MODER;
    ASSERT_EQ(NX_ERROR_IO, nx_stm32u575_spi_prepare(&cold, &config));
    EXPECT_TRUE(first.retained);
    EXPECT_EQ(nullptr, later.registers);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_spi_release(&cold, &config));
    EXPECT_FALSE(first.retained);
    EXPECT_EQ(nullptr, later.registers);
}

TEST_F(U575Spi, ColdCleanupRetriesCsAfterTheWireHasAlreadyJoined) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_gpio_state_t own_cs{};
    nx_stm32u575_spi_endpoint_state_t child{&cold, &own_cs, 16U, 1000000U, 0U};
    nx_stm32u575_spi_endpoint_config_t endpoint_config{
        &own_cs, &child, &g_u575_model.gpioa, 16U, 1000000U, 0U};
    nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 5U, 5U};
    nx_stm32u575_spi_config_t config{&endpoint_config, 1U, &pin, 1U};
    ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_prepare(&cold, &config));
    g_u575_model.gpioa.ODR &= ~16U;
    g_u575_model.ignore_write = &g_u575_model.gpioa.BSRR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32u575_spi_release(&cold, &config));
    EXPECT_FALSE(cold.initialized);
    EXPECT_TRUE(own_cs.retained);
    g_u575_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_spi_release(&cold, &config));
    EXPECT_FALSE(own_cs.retained);
}
TEST_F(U575Spi, DuplicateCsStateRejectsTheCompleteSetBeforeAnyEffect) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_gpio_state_t shared{};
    nx_stm32u575_spi_endpoint_state_t children[2]{};
    const nx_stm32u575_spi_endpoint_config_t endpoints[]{
        {&shared, &children[0], &g_u575_model.gpioa, 32U, 1000000U, 0U},
        {&shared, &children[1], &g_u575_model.gpioa, 32U, 1000000U, 0U}};
    const nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 7U, 5U};
    const nx_stm32u575_spi_config_t config{endpoints, 2U, &pin, 1U};
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_spi_prepare(&cold, &config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(cold.retained);
    EXPECT_FALSE(shared.retained);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_spi_release(&cold, &config));
}
TEST_F(U575Spi, DuplicateEndpointStateRejectsBeforeAnyCsEffect) {
    nx_stm32u575_spi_state_t cold{};
    nx_stm32u575_gpio_state_t pins[2]{};
    nx_stm32u575_spi_endpoint_state_t child{};
    const nx_stm32u575_spi_endpoint_config_t endpoints[]{
        {&pins[0], &child, &g_u575_model.gpioa, 32U, 1000000U, 0U},
        {&pins[1], &child, &g_u575_model.gpioa, 64U, 1000000U, 0U}};
    const nx_stm32u575_alternate_config_t pin{&g_u575_model.gpioa, 7U, 5U};
    const nx_stm32u575_spi_config_t config{endpoints, 2U, &pin, 1U};
    const auto writes = g_u575_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32u575_spi_prepare(&cold, &config));
    EXPECT_EQ(writes, g_u575_model.writes);
    EXPECT_FALSE(cold.retained);
    EXPECT_FALSE(pins[0].retained);
    EXPECT_FALSE(pins[1].retained);
    EXPECT_EQ(NX_SUCCESS, nx_stm32u575_spi_release(&cold, &config));
}
