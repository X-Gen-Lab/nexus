/**
 * \file            h743_spi_wire_test.cpp
 * \brief           Bounded wire leaves, actual stop events and pin handoff
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_peripheral_model.h"
#include "stm32h743_spi_wire.h"
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

class H743SpiWire : public testing::Test {
  protected:
    SPI_TypeDef registers{};
    nx_stm32h743_spi_state_t state{};
    nx_stm32h743_gpio_state_t cs{};
    void SetUp() override {
        nx_h743_model_reset();
        g_h743_model.spi = &registers;
        registers.CFG1 = NX_STM32H743_SPI_BYTE_CONFIG;
        registers.SR = SPI_SR_TXP | SPI_SR_TXC;
        state.registers = &registers;
        state.reset = &g_h743_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI4RST;
        state.clock_hz = 64000000U;
        state.poll_limit = 16U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_system_start(&g_nx_stm32h743_system,
                                                        64000000U, 0U, 8U)
                                  .primary);
        g_h743_model.rcc.APB2ENR |= RCC_APB2ENR_SPI4EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
        cs = {&g_h743_model.gpio_e, 1U << 11U, true, false, false};
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_gpio_initialize(&cs, cs.mask));
        for (unsigned pin : {12U, 13U, 14U})
            ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pin_alternate(
                                      &g_h743_model.gpio_e, pin, 5U));
    }
    void SelectIdleCs() {
        registers.CFG2 = SPI_CFG2_AFCNTR | SPI_CFG2_MASTER | SPI_CFG2_SSM;
        state.held_cs = &cs;
        state.held_cs_mask = cs.mask;
        const nx_gpio_port_t face{&nx_stm32h743_gpio_ops, &cs};
        ASSERT_EQ(NX_SUCCESS, nx_gpio_port_write(&face, 0U, cs.mask));
        g_h743_model.spi_watch_pins = true;
    }
    void TearDown() override {
        EXPECT_EQ(nullptr, g_h743_model.ignore_write);
        EXPECT_EQ(nullptr, g_h743_model.critical_observer);
        if ((registers.CR1 & SPI_CR1_CSTART) != 0U) {
            nx_stm32h743_spi_wire_suspend_once(&state);
            nx_h743_model_spi_poll(&state);
            if (!nx_stm32h743_spi_wire_stopped(&state))
                std::abort();
        }
        const nx_result_t disabled = nx_stm32h743_spi_wire_disable(&state);
        const nx_result_t released = nx_stm32h743_spi_wire_release_cs(&state);
        const nx_result_t idle = nx_stm32h743_spi_wire_sck_idle(&state);
        const nx_result_t reset = nx_stm32h743_spi_wire_reset(&state);
        EXPECT_EQ(NX_SUCCESS, disabled);
        EXPECT_EQ(NX_SUCCESS, released);
        EXPECT_EQ(NX_SUCCESS, idle);
        EXPECT_EQ(NX_SUCCESS, reset);
        if (disabled != NX_SUCCESS || released != NX_SUCCESS ||
            idle != NX_SUCCESS || reset != NX_SUCCESS)
            std::abort();
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_gpio_stop(&cs, cs.mask));
        EXPECT_EQ(NX_SUCCESS,
                  nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
    }
};
TEST_F(H743SpiWire, ActualExactVClockReadinessIsReadOnly) {
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_clock_ready(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743SpiWire, NonVRejectsBeforeReadingVOnlyTrim) {
    g_h743_model.silicon_id = 0x10030450U;
    const uint32_t reads = g_h743_model.reads;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32h743_spi_wire_clock_ready(&state));
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
    g_h743_model.silicon_id = 0x20030450U;
}
TEST_F(H743SpiWire, UserTrimChangeRejectsWithoutRepairingClock) {
    g_h743_model.rcc.HSICFGR = 0x3fU << RCC_HSICFGR_HSITRIM_Pos;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_wire_clock_ready(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(0x3fU << RCC_HSICFGR_HSITRIM_Pos, g_h743_model.rcc.HSICFGR);
    g_h743_model.rcc.HSICFGR = 0x40U << RCC_HSICFGR_HSITRIM_Pos;
}
TEST_F(H743SpiWire, CalibrationBitsDoNotChangeReviewedUserTrim) {
    g_h743_model.rcc.HSICFGR |= 0xabcU;
    const uint32_t trim = g_h743_model.rcc.HSICFGR;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_clock_ready(&state));
    EXPECT_EQ(trim, g_h743_model.rcc.HSICFGR);
}
TEST_F(H743SpiWire, StoppedParentIsNotAdoptedByClockObservation) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_system_stop(&g_nx_stm32h743_system, 8U));
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_wire_clock_ready(&state));
    EXPECT_FALSE(g_nx_stm32h743_system.started);
}
TEST_F(H743SpiWire, ForeignMuxAndDisabledGateRemainUntouched) {
    for (uint32_t selector : {1U, 2U, 3U}) {
        g_h743_model.rcc.D2CCIP1R = selector << RCC_D2CCIP1R_SPI45SEL_Pos;
        const uint32_t writes = g_h743_model.writes;
        EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_wire_clock_ready(&state));
        EXPECT_EQ(writes, g_h743_model.writes);
    }
    g_h743_model.rcc.D2CCIP1R = 0U;
    g_h743_model.rcc.APB2ENR &= ~(uint32_t)RCC_APB2ENR_SPI4EN;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_wire_clock_ready(&state));
    g_h743_model.rcc.APB2ENR |= RCC_APB2ENR_SPI4EN;
}
TEST_F(H743SpiWire, DividerUsesEnvelopeWithoutAnyMmio) {
    const uint32_t reads = g_h743_model.reads;
    const uint32_t writes = g_h743_model.writes;
    uint32_t divisor = 0U, baud = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_divider(64000000U, 16000000U,
                                                        &divisor, &baud));
    EXPECT_EQ(8U, divisor);
    EXPECT_EQ(2U, baud);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_divider(64000000U, 1500000U,
                                                        &divisor, &baud));
    EXPECT_EQ(64U, divisor);
    EXPECT_EQ(5U, baud);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_divider(32000000U, 8000000U,
                                                        &divisor, &baud));
    EXPECT_EQ(8U, divisor);
    EXPECT_EQ(reads, g_h743_model.reads);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743SpiWire, DividerMinimumIsExactAndFailurePreservesOutputs) {
    uint32_t divisor = 99U, baud = 77U;
    for (uint32_t rate : {250000U, 253906U}) {
        EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_stm32h743_spi_wire_divider(
                                            64000000U, rate, &divisor, &baud));
        EXPECT_EQ(99U, divisor);
        EXPECT_EQ(77U, baud);
    }
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_divider(64000000U, 253907U,
                                                        &divisor, &baud));
    EXPECT_EQ(256U, divisor);
    EXPECT_EQ(7U, baud);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_divider(32000000U, 250000U,
                                                        &divisor, &baud));
    EXPECT_EQ(256U, divisor);
}
TEST_F(H743SpiWire, InvalidDividerParametersHaveNoHardwareEffects) {
    uint32_t divisor = 99U, baud = 77U;
    const uint32_t reads = g_h743_model.reads;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_spi_wire_divider(0U, 1000000U, &divisor, &baud));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_spi_wire_divider(64000000U, 0U, &divisor, &baud));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_spi_wire_divider(
                                    64000000U, 1000000U, nullptr, &baud));
    EXPECT_EQ(99U, divisor);
    EXPECT_EQ(77U, baud);
    EXPECT_EQ(reads, g_h743_model.reads);
}
TEST_F(H743SpiWire, QuietIntervalUsesNominalSameSourceRatio) {
    EXPECT_EQ(3U, nx_stm32h743_spi_wire_quiet_us(64000000U, 2U));
    EXPECT_EQ(3U, nx_stm32h743_spi_wire_quiet_us(64000000U, 64U));
    EXPECT_EQ(4U, nx_stm32h743_spi_wire_quiet_us(64000000U, 128U));
    EXPECT_EQ(6U, nx_stm32h743_spi_wire_quiet_us(64000000U, 256U));
    EXPECT_EQ(10U, nx_stm32h743_spi_wire_quiet_us(32000000U, 256U));
    EXPECT_EQ(0U, nx_stm32h743_spi_wire_quiet_us(0U, 2U));
    EXPECT_EQ(0U, nx_stm32h743_spi_wire_quiet_us(64000000U, 3U));
}
TEST_F(H743SpiWire, SuspendCommandNeverReplaysReadSetStart) {
    registers.CR1 = SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART;
    registers.CR2 = 1U;
    g_h743_model.hold_spi_suspend = true;
    const uint32_t writes = g_h743_model.writes;
    nx_stm32h743_spi_wire_suspend_once(&state);
    EXPECT_EQ(writes + 1U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.spi_start_suspend_commands);
    EXPECT_NE(0U, registers.CR1 & SPI_CR1_CSTART);
    EXPECT_EQ(0U, registers.CR1 & SPI_CR1_CSUSP);
    EXPECT_EQ(1U, g_h743_model.spi_suspend_requests);
    EXPECT_TRUE(g_h743_model.spi_suspend_pending);
    g_h743_model.hold_spi_suspend = false;
}
TEST_F(H743SpiWire, EotBeforeSuspendNeedsNoSyntheticSuspFlag) {
    registers.CR1 = SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_CSTART;
    registers.CR2 = 1U;
    g_h743_model.spi_idle_before_suspend_status = SPI_SR_EOT | SPI_SR_TXC;
    EXPECT_NE(0U, NX_H743_READ(&registers.CR1) & SPI_CR1_CSTART);
    nx_stm32h743_spi_wire_suspend_once(&state);
    EXPECT_EQ(1U, g_h743_model.spi_idle_before_suspend_events);
    EXPECT_EQ(0U, registers.SR & SPI_SR_SUSP);
    EXPECT_TRUE(nx_stm32h743_spi_wire_stopped(&state));
    EXPECT_EQ(0U, g_h743_model.spi_start_suspend_commands);
}
TEST_F(H743SpiWire, StoppedObservationPreservesFiniteCompletionFlags) {
    registers.CR2 = 2U;
    registers.SR = SPI_SR_EOT | SPI_SR_TXC;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_TRUE(nx_stm32h743_spi_wire_stopped(&state));
    EXPECT_EQ(SPI_SR_EOT | SPI_SR_TXC, registers.SR);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743SpiWire, IncompleteFiniteSnapshotsNeverProveStop) {
    registers.CR2 = 1U;
    for (uint32_t status : {uint32_t(SPI_SR_EOT), uint32_t(SPI_SR_TXC), 0U}) {
        registers.SR = status;
        EXPECT_FALSE(nx_stm32h743_spi_wire_stopped(&state));
    }
    registers.CR2 = 0U;
    registers.SR = SPI_SR_EOT | SPI_SR_TXC;
    EXPECT_FALSE(nx_stm32h743_spi_wire_stopped(&state));
}
TEST_F(H743SpiWire, SuspObservationRequiresClearedStart) {
    registers.SR = SPI_SR_SUSP;
    EXPECT_TRUE(nx_stm32h743_spi_wire_stopped(&state));
    registers.CR1 = SPI_CR1_SPE | SPI_CR1_CSTART;
    EXPECT_FALSE(nx_stm32h743_spi_wire_stopped(&state));
}
TEST_F(H743SpiWire, DisableAcknowledgesSuspBeforeClearingSpe) {
    registers.CR1 = SPI_CR1_SPE;
    registers.SR = SPI_SR_SUSP;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_disable(&state));
    EXPECT_EQ(0U, registers.CR1 & SPI_CR1_SPE);
    EXPECT_EQ(0U, registers.SR & SPI_SR_SUSP);
    EXPECT_EQ(0U, g_h743_model.spi_disable_before_stop);
}
TEST_F(H743SpiWire, RefusedSuspAckDoesNotProceedToDisable) {
    registers.CR1 = SPI_CR1_SPE;
    registers.SR = SPI_SR_SUSP;
    g_h743_model.ignore_write = &registers.IFCR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_disable(&state));
    EXPECT_NE(0U, registers.CR1 & SPI_CR1_SPE);
    g_h743_model.ignore_write = nullptr;
}
TEST_F(H743SpiWire, RefusedSpeWriteRemainsVisibleForActualRetry) {
    registers.CR1 = SPI_CR1_SPE;
    g_h743_model.ignore_write = &registers.CR1;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_disable(&state));
    EXPECT_NE(0U, registers.CR1 & SPI_CR1_SPE);
    g_h743_model.ignore_write = nullptr;
}
TEST_F(H743SpiWire, NeverSelectedCsHasNoFabricatedTransition) {
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_release_cs(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743SpiWire, SelectedCsReleasesOnlyAfterActualHighReadback) {
    SelectIdleCs();
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_release_cs(&state));
    EXPECT_EQ(cs.mask, cs.registers->ODR & cs.mask);
    EXPECT_EQ(nullptr, state.held_cs);
    EXPECT_EQ(0U, state.held_cs_mask);
}
TEST_F(H743SpiWire, RefusedCsHighKeepsItsReferenceForRetry) {
    SelectIdleCs();
    g_h743_model.spi_hold_cs_high = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_release_cs(&state));
    EXPECT_EQ(&cs, state.held_cs);
    EXPECT_EQ(0U, cs.registers->ODR & cs.mask);
    g_h743_model.spi_hold_cs_high = false;
}
TEST_F(H743SpiWire, LostCsOutputIsNotMisreadAsARelease) {
    SelectIdleCs();
    cs.registers->MODER &= ~(3U << 22U);
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_release_cs(&state));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(&cs, state.held_cs);
    /* Repair only the external pad fault, never the provider's held lease. */
    cs.registers->MODER |= 1U << 22U;
}
TEST_F(H743SpiWire, CpolLatchAndOutputHandoffPrecedeCheckedReset) {
    registers.CFG2 = SPI_CFG2_AFCNTR | SPI_CFG2_CPOL | SPI_CFG2_CPHA;
    testing::StrictMock<testing::MockFunction<void(bool)>> boundary;
    g_h743_model.critical_context = &boundary;
    g_h743_model.critical_observer = [](void* context, bool entering) {
        static_cast<testing::MockFunction<void(bool)>*>(context)->Call(
            entering);
    };
    {
        testing::InSequence sequence;
        EXPECT_CALL(boundary, Call(true)).WillOnce([&](bool) {
            EXPECT_EQ(2U, (cs.registers->MODER >> 24U) & 3U);
        });
        EXPECT_CALL(boundary, Call(false)).WillOnce([&](bool) {
            EXPECT_EQ(1U, (cs.registers->MODER >> 24U) & 3U);
            EXPECT_NE(0U, cs.registers->ODR & (1U << 12U));
            EXPECT_EQ(0U, g_h743_model.spi_reset_assertions);
        });
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_sck_idle(&state));
    }
    g_h743_model.critical_observer = nullptr;
    g_h743_model.critical_context = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_reset(&state));
    EXPECT_NE(0U, cs.registers->ODR & (1U << 12U));
    EXPECT_EQ(1U, g_h743_model.spi_reset_assertions);
}
TEST_F(H743SpiWire, SckReturnsToAfOnlyWithPreparedRoute) {
    registers.CFG2 = SPI_CFG2_AFCNTR | SPI_CFG2_CPOL;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_sck_idle(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_sck_alternate());
    EXPECT_EQ(2U, (cs.registers->MODER >> 24U) & 3U);
    const uint32_t alternate = cs.registers->AFR[1];
    cs.registers->AFR[1] &= ~(15U << 16U);
    const uint32_t writes = g_h743_model.writes;
    EXPECT_FALSE(nx_stm32h743_spi_wire_pins_ready());
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_spi_wire_sck_alternate());
    EXPECT_EQ(writes, g_h743_model.writes);
    cs.registers->AFR[1] = alternate;
}
TEST_F(H743SpiWire, RefusedSckLatchDoesNotChangePadMode) {
    registers.CFG2 = SPI_CFG2_AFCNTR | SPI_CFG2_CPOL;
    g_h743_model.ignore_write = &cs.registers->BSRR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_sck_idle(&state));
    EXPECT_EQ(2U, (cs.registers->MODER >> 24U) & 3U);
    g_h743_model.ignore_write = nullptr;
}
TEST_F(H743SpiWire, RefusedSckOutputRetainsTheActualAfRoute) {
    registers.CFG2 = SPI_CFG2_AFCNTR | SPI_CFG2_CPOL;
    g_h743_model.spi_hold_sck_output = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_sck_idle(&state));
    EXPECT_EQ(2U, (cs.registers->MODER >> 24U) & 3U);
    g_h743_model.spi_hold_sck_output = false;
}
TEST_F(H743SpiWire, ResetRejectionDoesNotEraseRegistersOrOwnership) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_sck_idle(&state));
    registers.CFG2 = SPI_CFG2_MASTER;
    g_h743_model.ignore_write = state.reset;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32h743_spi_wire_reset(&state));
    EXPECT_EQ(SPI_CFG2_MASTER, registers.CFG2);
    EXPECT_TRUE(cs.initialized);
    g_h743_model.ignore_write = nullptr;
}
TEST_F(H743SpiWire, ReleasedCsAndSckHandoffKeepNeighborRegisters) {
    const uint32_t neighbor = RCC_APB2RSTR_SPI1RST;
    g_h743_model.rcc.APB2RSTR |= neighbor;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_sck_idle(&state));
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_spi_wire_reset(&state));
    EXPECT_NE(0U, g_h743_model.rcc.APB2RSTR & neighbor);
    EXPECT_TRUE(nx_stm32h743_spi_wire_cs_driven(&cs, cs.mask));
}
