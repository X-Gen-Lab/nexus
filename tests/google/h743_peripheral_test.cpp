/**
 * \file            h743_peripheral_test.cpp
 * \brief           Real H743 PWM, watchdog, EXTI and SPI V3 contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_peripheral_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_provider.h"
#include "terminal_watchdog.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
class SpiFaultBoundary {
  public:
    MOCK_METHOD(void, Observe, ());
};
SpiFaultBoundary* spi_fault;
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    EXPECT_EQ(NX_FAULT_PLATFORM, domain);
    EXPECT_EQ(NX_ERROR_IO, reason);
    EXPECT_NE(nullptr, file);
    EXPECT_NE(0U, line);
    spi_fault->Observe();
}

class H743Pwm : public testing::Test {
  protected:
    TIM_TypeDef timer{};
    nx_stm32h743_gpio_state_t idle{};
    nx_stm32h743_pwm_state_t state{};
    nx_pwm_port_t face{&nx_stm32h743_pwm_ops, &state};
    void SetUp() override {
        nx_h743_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.APB1LENR = RCC_APB1LENR_TIM3EN;
        g_h743_model.pwm = &timer;
        idle = {&g_h743_model.gpio_a, 1U << 6U, true, false, false};
        state.registers = &timer;
        state.idle = &idle;
        state.reset = &g_h743_model.rcc.APB1LRSTR;
        state.reset_mask = RCC_APB1LRSTR_TIM3RST;
        state.period_ticks = 1000U;
        state.duty_ticks = 250U;
        state.tick_hz = 1000000U;
        state.poll_limit = 4U;
        state.pin = 6U;
    }
};
TEST_F(H743Pwm, ColdBaseAndBothDutyEndpointsUseTheSelectedClock) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pwm_initialize(&state));
    EXPECT_EQ(63U, timer.PSC);
    EXPECT_EQ(999U, timer.ARR);
    EXPECT_EQ(0U, timer.CCER);
    ASSERT_EQ(NX_SUCCESS, nx_pwm_port_start(&face));
    EXPECT_EQ(TIM_CCER_CC1E, timer.CCER);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 0U));
    EXPECT_EQ(0U, timer.CCR1);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_set(&face, 1000U, 1000U));
    EXPECT_GT(timer.CCR1, timer.ARR);
    EXPECT_EQ(NX_SUCCESS, nx_pwm_port_stop(&face));
    EXPECT_EQ(1U, (g_h743_model.gpio_a.MODER >> 12U) & 3U);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pwm_close(&state));
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Pwm, Hsi32ProfileUsesItsOwnPrescaler) {
    g_nx_stm32h743_system.core_hz = 32000000U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pwm_initialize(&state));
    EXPECT_EQ(31U, timer.PSC);
}
TEST_F(H743Pwm, ForeignTimerRejectsWithoutGpioOrRegisterWrites) {
    timer.SMCR = TIM_SMCR_SMS_0;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_pwm_initialize(&state));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Pwm, FailedUpdateRetainsAndCloseCanRecover) {
    g_h743_model.hold_pwm_update = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_stm32h743_pwm_initialize(&state));
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pwm_close(&state));
    EXPECT_FALSE(idle.retained);
}
TEST_F(H743Pwm, FailedResetKeepsResponsibilityForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pwm_initialize(&state));
    g_h743_model.hold_pwm_reset = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_pwm_close(&state));
    EXPECT_TRUE(state.retained);
    g_h743_model.hold_pwm_reset = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_pwm_close(&state));
}

class H743Watchdog : public testing::Test {
  protected:
    IWDG_TypeDef registers{};
    DBGMCU_TypeDef debug{};
    nx_stm32h743_watchdog_state_t state{};
    nx_watchdog_port_t face{&nx_stm32h743_watchdog_ops, &state};
    void SetUp() override {
        nx_h743_model_reset();
        registers.WINR = registers.RLR = 4095U;
        g_h743_model.flash.OPTSR_CUR = FLASH_OPTSR_IWDG1_SW;
        g_h743_model.watchdog = &registers;
        state.registers = &registers;
        state.rcc = &g_h743_model.rcc;
        state.flash = &g_h743_model.flash;
        state.debug = &debug;
        state.poll_limit = 4U;
    }
};
TEST_F(H743Watchdog, StartAndFeedExposeIrreversibleUnknownTimingEnvelope) {
    g_h743_model.rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_SUCCESS,
              nx_watchdog_port_enable(&face, 1000000U, true, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_TRUE(actual.debug_freeze);
    EXPECT_EQ(0U, actual.minimum_timeout_us);
    EXPECT_EQ(0U, actual.maximum_timeout_us);
    EXPECT_EQ(3999U, registers.RLR);
    EXPECT_EQ(NX_SUCCESS, nx_watchdog_port_feed(&face));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_watchdog_port_state(&face, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_watchdog_stop(&state));
}
TEST_F(H743Watchdog, LsiFailureHasNoIrreversibleActivation) {
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000U, false, &actual));
    EXPECT_FALSE(actual.enabled);
    EXPECT_EQ(0U, registers.KR);
    EXPECT_EQ(0U, g_h743_model.rcc.CSR & RCC_CSR_LSION);
}
TEST_F(H743Watchdog, PostActivationFailureKeepsTheWatchdogEnabled) {
    g_h743_model.rcc.CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    g_h743_model.hold_watchdog_update = true;
    nx_watchdog_state_t actual{};
    EXPECT_EQ(NX_ERROR_IO,
              nx_watchdog_port_enable(&face, 1000000U, false, &actual));
    EXPECT_TRUE(actual.enabled);
    EXPECT_TRUE(actual.irreversible);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_watchdog_stop(&state));
}
TEST_F(H743Watchdog, ResetCausesAreLatchedBeforeExternalAcknowledgement) {
    g_nx_stm32h743_system.reset_cause =
        RCC_RSR_PORRSTF | RCC_RSR_IWDG1RSTF | RCC_RSR_BORRSTF;
    g_h743_model.rcc.RSR = 0U;
    EXPECT_EQ(NX_RESET_POWER_ON | NX_RESET_IWDG | NX_RESET_BROWNOUT,
              nx_reset_cause());
    EXPECT_EQ(0U, g_h743_model.writes);
}

class H743Exti : public testing::Test {
  protected:
    nx_exti_event_t events[2]{};
    nx_stm32h743_exti_state_t state{};
    nx_exti_port_t face{&nx_stm32h743_exti_ops, &state};
    void SetUp() override {
        nx_h743_model_reset();
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.AHB4ENR = RCC_AHB4ENR_GPIOAEN;
        g_h743_model.rcc.APB4ENR = RCC_APB4ENR_SYSCFGEN;
        state.registers = &g_h743_model.exti;
        state.mux = &g_h743_model.mux;
        state.gpio = &g_h743_model.gpio_a;
        state.storage = events;
        state.capacity = 2U;
        state.edge = NX_EXTI_BOTH;
        state.priority = 5U;
        state.irq_policy = {NX_IRQ_KERNEL_NONE, 150U, 4U, 0U};
    }
};
TEST_F(H743Exti, SelectedLineAcknowledgesOnlyItsOwnPendingBit) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_exti_initialize(&state));
    g_h743_model.exti.PR1 = 3U;
    g_h743_model.gpio_a.IDR = 1U;
    nx_stm32h743_exti_irq(&state);
    EXPECT_EQ(2U, g_h743_model.exti.PR1);
    nx_exti_event_t out{};
    size_t count = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_read(&face, &out, 1U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(NX_EXTI_RISING, out.edge);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, out.flags);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
}
TEST_F(H743Exti, ForeignPendingLineIsRejectedBeforeAnyEffect) {
    g_h743_model.exti.PR1 = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32h743_exti_initialize(&state));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743Exti, RefusedVectorWithdrawalRetainsStorageUntilRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_exti_initialize(&state));
    g_h743_model.hold_irq_off = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_h743_model.hold_irq_off = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&face));
    EXPECT_FALSE(state.retained);
}

class H743Spi : public testing::Test {
  protected:
    SPI_TypeDef registers{};
    nx_stm32h743_spi_state_t state{};
    nx_stm32h743_gpio_state_t cs{};
    nx_stm32h743_spi_endpoint_state_t endpoint{};
    nx_spi_port_t face{&nx_stm32h743_spi_ops, &state};
    nx_spi_endpoint_t device{&nx_stm32h743_spi_endpoint_ops, &endpoint};
    testing::NiceMock<SpiFaultBoundary> faults;
    void SetUp() override {
        nx_h743_model_reset();
        spi_fault = &faults;
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.APB2ENR = RCC_APB2ENR_SPI4EN;
        g_h743_model.spi = &registers;
        registers.SR = SPI_SR_TXP | SPI_SR_TXC;
        registers.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
        state.registers = &registers;
        state.reset = &g_h743_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI4RST;
        state.clock_hz = 64000000U;
        state.poll_limit = 4U;
        cs = {&g_h743_model.gpio_b, 1U, true, false, false};
        endpoint = {&state, &cs, 1U, 1000000U, 0U};
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_gpio_initialize(&cs, 1U));
    }
};
TEST_F(H743Spi, ColdAdmissionUsesV3CfgAndNoDmaOrInterrupt) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    EXPECT_EQ(7U | (7U << SPI_CFG1_CRCSIZE_Pos), registers.CFG1);
    EXPECT_EQ(0U, registers.IER);
    EXPECT_EQ(0U, registers.CR1);
}
TEST_F(H743Spi, V3TsizeAndByteAccessCompleteThenWithdrawBeforeCs) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
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
}
TEST_F(H743Spi, SlowClockTailSurvivesEarlyEotAndTxcBeforeResetAndCsRelease) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    endpoint.frequency_hz = 250000U;
    state.poll_limit = 16U;
    uint8_t byte = 0x25U;
    size_t transferred = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, &byte, nullptr, 1U,
                                                   10000U, &transferred));
    EXPECT_FALSE(g_h743_model.spi_tail_truncated);
    EXPECT_GE(g_h743_model.timer.CNT, g_h743_model.spi_tail_until);
}
TEST_F(H743Spi, CallerExpiryCannotCutShortTheFinalClockDrain) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    endpoint.frequency_hz = 250000U;
    state.poll_limit = 16U;
    uint8_t byte = 0x25U;
    size_t transferred = 0U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_spi_endpoint_transfer(&device, &byte, nullptr, 1U, 3U,
                                       &transferred));
    EXPECT_EQ(1U, transferred);
    EXPECT_FALSE(g_h743_model.spi_tail_truncated);
    EXPECT_GE(g_h743_model.timer.CNT, g_h743_model.spi_tail_until);
    EXPECT_NE(0U, cs.registers->ODR & cs.mask);
    EXPECT_FALSE(state.active);
}
TEST_F(H743Spi, StalledByteResetsControllerAndRequiresExplicitRecovery) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    g_h743_model.hold_spi_progress = true;
    uint8_t tx = 12U;
    size_t transferred = 99U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U, 100U,
                                       &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_TRUE(state.fault);
    EXPECT_NE(0U, cs.registers->ODR & cs.mask);
    g_h743_model.hold_spi_progress = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&face));
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U,
                                                   100U, &transferred));
}
TEST_F(H743Spi, ForeignKernelSelectorRejectsWithoutChangingTheDomain) {
    g_h743_model.rcc.D2CCIP1R = RCC_D2CCIP1R_SPI45SEL_0;
    uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_initialize(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Spi, AliasedOrUnboundedRequestsRejectBeforeChipSelect) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    size_t transferred = 17U;
    uint8_t tx = 12U;
    uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_spi_endpoint_transfer(&device, &tx, nullptr, 1U,
                                       NX_DEADLINE_NEVER, &transferred));
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_transfer(
                                    &device, reinterpret_cast<uint8_t*>(&state),
                                    nullptr, 1U, 100U, &transferred));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743Spi, RefusedResetKeepsPortResponsibilityForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    registers.CR1 = SPI_CR1_SPE;
    g_h743_model.hold_spi_reset = true;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_h743_model.hold_spi_reset = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&face));
    EXPECT_FALSE(state.retained);
}
TEST_F(H743Spi, RefusedDrainCannotReturnOrReleaseTheCallerAndChipSelect) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    g_h743_model.hold_spi_progress = true;
    g_h743_model.hold_spi_reset = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(faults, Observe()).WillOnce([&] {
            nexus_test::RecordTerminalObservation(
                nx_arch_irq_is_masked() && state.active && state.retained &&
                state.fault && state.held_cs == &cs &&
                (registers.CR1 & SPI_CR1_SPE) != 0U &&
                (cs.registers->ODR & cs.mask) == 0U);
        });
        uint8_t byte = 0x55U;
        size_t transferred = 0U;
        (void)nx_spi_endpoint_transfer(&device, &byte, nullptr, 1U, 10000U,
                                       &transferred);
    }));
}
