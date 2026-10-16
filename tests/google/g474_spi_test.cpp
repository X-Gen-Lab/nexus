/**
 * \file            g474_spi_test.cpp
 * \brief           G474 actual FIFO SPI byte-width, CS, deadline and reset
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_model.h"
#include "nexus/core/fault.h"
#include "stm32g474_provider.h"
#include "terminal_watchdog.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
namespace {
class SpiFaultPort {
  public:
    MOCK_METHOD(void, Observe,
                (nx_fault_domain_t, nx_result_t, const char*, unsigned));
};
SpiFaultPort* spi_fault_port;
} /* namespace */
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    spi_fault_port->Observe(domain, reason, file, line);
}
class G474Spi : public testing::Test {
  protected:
    testing::NiceMock<SpiFaultPort> fault_mock;
    SPI_TypeDef registers{};
    nx_stm32g474_spi_state_t state{};
    nx_stm32g474_gpio_state_t cs{};
    nx_stm32g474_spi_endpoint_state_t endpoint{};
    nx_spi_port_t face{&nx_stm32g474_spi_ops, &state};
    nx_spi_endpoint_t device{&nx_stm32g474_spi_endpoint_ops, &endpoint};
    void SetUp() override {
        nx_g474_model_reset();
        spi_fault_port = &fault_mock;
        state.registers = &registers;
        state.reset = &g_g474_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI1RST;
        state.clock_hz = 170000000;
        state.poll_limit = 8;
        registers.SR = SPI_SR_TXE;
        g_g474_model.spi[0] = &registers;
        g_nx_stm32g474_system.started = true;
        g_nx_stm32g474_system.core_hz = 170000000;
        g_g474_model.rcc.APB2ENR = RCC_APB2ENR_SPI1EN;
        g_g474_model.rcc.APB1ENR1 = RCC_APB1ENR1_SPI2EN;
        cs = {&g_g474_model.gpio, 1U << 4U, true, false, false};
        endpoint = {&state, &cs, 1U << 4U, 1000000, 0};
        ASSERT_EQ(NX_SUCCESS, nx_stm32g474_gpio_initialize(&cs, 1U << 4U));
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32g474_spi_initialize(&state));
    }
};
TEST_F(G474Spi, G4EightBitThresholdAndColdAdmissionAreExact) {
    Start();
    EXPECT_EQ((7U << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH, registers.CR2);
    EXPECT_EQ(0U, registers.CR1);
    EXPECT_TRUE(state.retained);
}
TEST_F(G474Spi, ForeignBusyPeripheralIsRejectedBeforeAnyWrite) {
    registers.SR |= SPI_SR_BSY;
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32g474_spi_initialize(&state));
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Spi, AlreadyExpiredTransferDoesNotAssertCs) {
    Start();
    uint8_t tx = 42;
    size_t transferred = 17;
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_spi_endpoint_transfer(&device, &tx, nullptr,
                                                         1, 0, &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_NE(0U, g_g474_model.gpio.ODR & cs.mask);
}

TEST_F(G474Spi, ByteWidthFullDuplexDrainsFifoBeforeCsRelease) {
    Start();
    uint8_t tx[2] = {0x12, 0x34};
    uint8_t rx[2] = {};
    size_t transferred = 0;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, tx, rx, 2, 10000,
                                                   &transferred));
    EXPECT_EQ(2U, transferred);
    EXPECT_EQ(0xedU, rx[0]);
    EXPECT_EQ(0xcbU, rx[1]);
    EXPECT_EQ(2U, g_g474_model.spi_transmitted);
    EXPECT_EQ(0U, registers.SR & (SPI_SR_BSY | SPI_SR_FRLVL | SPI_SR_FTLVL));
    EXPECT_EQ(0U, registers.CR1 & SPI_CR1_SPE);
    EXPECT_NE(0U, g_g474_model.gpio.ODR & cs.mask);
}
TEST_F(G474Spi, StalledByteTimesOutAndRequiresExplicitRecovery) {
    Start();
    g_g474_model.hold_spi_progress = true;
    uint8_t tx = 0x12;
    size_t transferred = 0;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_spi_endpoint_transfer(&device, &tx, nullptr,
                                                         1, 100, &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, registers.SR & SPI_SR_BSY);
    EXPECT_EQ(NX_ERROR_STATE, nx_spi_endpoint_transfer(&device, &tx, nullptr, 1,
                                                       100, &transferred));
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&face));
    g_g474_model.hold_spi_progress = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, &tx, nullptr, 1,
                                                   100, &transferred));
}
TEST_F(G474Spi, PrivateResetFailureRetainsTheControllerForStopRetry) {
    Start();
    state.fault = true;
    registers.CR1 |= SPI_CR1_SPE;
    registers.SR |= SPI_SR_BSY;
    g_g474_model.hold_spi_reset = true;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_recover(&face));
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_stop(&face));
    EXPECT_TRUE(state.retained);
    g_g474_model.hold_spi_reset = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&face));
    EXPECT_FALSE(state.retained);
}

TEST_F(G474Spi, PartialBufferOverlapAndStateAliasRejectBeforeCs) {
    Start();
    uint8_t data[3] = {1, 2, 3};
    size_t transferred = 0;
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(&device, data, data + 1, 2, 100,
                                       &transferred));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(&device,
                                       reinterpret_cast<const uint8_t*>(&state),
                                       nullptr, 1, 100, &transferred));
    EXPECT_EQ(writes, g_g474_model.writes);
}
TEST_F(G474Spi, TwoControllersAndEndpointsHaveSeparateState) {
    Start();
    SPI_TypeDef second_registers{};
    second_registers.SR = SPI_SR_TXE;
    nx_stm32g474_spi_state_t second{};
    second.registers = &second_registers;
    second.reset = &g_g474_model.rcc.APB1RSTR1;
    second.reset_mask = RCC_APB1RSTR1_SPI2RST;
    second.clock_hz = 170000000;
    second.poll_limit = 8;
    g_g474_model.spi[1] = &second_registers;
    ASSERT_EQ(NX_SUCCESS, nx_stm32g474_spi_initialize(&second));
    const nx_spi_port_t second_face{&nx_stm32g474_spi_ops, &second};
    EXPECT_TRUE(nx_spi_endpoint_on_port(&device, &face));
    EXPECT_FALSE(nx_spi_endpoint_on_port(&device, &second_face));
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&second_face));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.initialized);
}

TEST_F(G474Spi, PrivateRetainedCsCleanupCannotBeForgottenByRecovery) {
    Start();
    /* A private recovery lease is distinct from a polling call. The polling
     * terminal tests prove that a live caller frame never returns here. */
    state.fault = true;
    state.held_cs = &cs;
    state.held_cs_mask = cs.mask;
    g_g474_model.gpio.ODR &= ~cs.mask;
    g_g474_model.spi_transmitted = 1;
    g_g474_model.hold_spi_cs_release = true;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_recover(&face));
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(&cs, state.held_cs);
    g_g474_model.hold_spi_cs_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&face));
    EXPECT_NE(0U, g_g474_model.gpio.ODR & cs.mask);
    EXPECT_EQ(nullptr, state.held_cs);
}

TEST_F(G474Spi, RatedClockCeilingRejectsAnEightyFiveMegahertzEndpoint) {
    Start();
    endpoint.frequency_hz = 85000000;
    uint8_t tx = 0x12;
    size_t transferred = 0;
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(
        NX_ERROR_UNSUPPORTED,
        nx_spi_endpoint_transfer(&device, &tx, nullptr, 1, 100, &transferred));
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_EQ(0U, g_g474_model.spi_transmitted);
}
TEST_F(G474Spi, RccResetRegisterCannotServeAsReceiveBuffer) {
    Start();
    uint8_t tx = 0x12;
    size_t transferred = 0;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(&device, &tx,
                                       reinterpret_cast<uint8_t*>(
                                           const_cast<uint32_t*>(state.reset)),
                                       1, 100, &transferred));
    EXPECT_EQ(0U, *state.reset);
}

TEST_F(G474Spi, MissingControllerClockRejectsBeforeAnyRegisterWrite) {
    g_g474_model.rcc.APB2ENR = 0U;
    const uint32_t writes = g_g474_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32g474_spi_initialize(&state));
    EXPECT_EQ(writes, g_g474_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(G474Spi, RefusedResetAssertAndReleaseCannotReopenTheBus) {
    Start();
    g_g474_model.hold_spi_progress = true;
    uint8_t tx = 0x12;
    size_t transferred = 0;
    ASSERT_EQ(NX_ERROR_TIMEOUT, nx_spi_endpoint_transfer(&device, &tx, nullptr,
                                                         1, 100, &transferred));
    g_g474_model.ignore_write = state.reset;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_recover(&face));
    EXPECT_TRUE(state.fault);
    g_g474_model.ignore_write = nullptr;
    g_g474_model.hold_spi_reset_release = true;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_recover(&face));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(state.fault);
    g_g474_model.hold_spi_reset_release = false;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&face));
}
TEST_F(G474Spi, RefusedCsAssertionNeverStartsTheSpiShifter) {
    Start();
    g_g474_model.ignore_write = &g_g474_model.gpio.BSRR;
    uint8_t tx = 0x12;
    size_t transferred = 0;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_endpoint_transfer(&device, &tx, nullptr, 1,
                                                    100, &transferred));
    EXPECT_EQ(0U, g_g474_model.spi_transmitted);
    EXPECT_EQ(0U, registers.CR1 & SPI_CR1_SPE);
    EXPECT_NE(0U, g_g474_model.gpio.ODR & cs.mask);
}

TEST_F(G474Spi, RefusedCsWithdrawalParksInsteadOfReturningCallerBuffers) {
    Start();
    g_g474_model.hold_spi_cs_release = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(fault_mock, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                        testing::_, testing::_))
            .WillOnce([&](nx_fault_domain_t, nx_result_t, const char* file,
                          unsigned line) {
                nexus_test::RecordTerminalObservation(
                    nx_arch_irq_is_masked() && state.active && state.retained &&
                    state.fault && state.held_cs == &cs &&
                    state.held_cs_mask == cs.mask && file != nullptr &&
                    line != 0U && (g_g474_model.gpio.ODR & cs.mask) == 0U);
            });
        uint8_t tx = 0x12;
        size_t transferred = 0;
        (void)nx_spi_endpoint_transfer(&device, &tx, nullptr, 1, 100,
                                       &transferred);
    }));
}
TEST_F(G474Spi, RefusedResetAssertParksWithTheLiveExecutionOwner) {
    Start();
    g_g474_model.hold_spi_progress = true;
    g_g474_model.ignore_write = state.reset;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(fault_mock, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                        testing::_, testing::_))
            .WillOnce([&](nx_fault_domain_t, nx_result_t, const char* file,
                          unsigned line) {
                nexus_test::RecordTerminalObservation(
                    nx_arch_irq_is_masked() && state.active && state.retained &&
                    state.fault && state.held_cs == &cs && file != nullptr &&
                    line != 0U && (registers.SR & SPI_SR_BSY) != 0U);
            });
        uint8_t tx = 0x12;
        size_t transferred = 0;
        (void)nx_spi_endpoint_transfer(&device, &tx, nullptr, 1, 100,
                                       &transferred);
    }));
}
TEST_F(G474Spi, RefusedResetReleaseParksAfterTheObserverReturns) {
    Start();
    g_g474_model.hold_spi_progress = true;
    g_g474_model.hold_spi_reset_release = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        EXPECT_CALL(fault_mock, Observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                                        testing::_, testing::_))
            .WillOnce([&](nx_fault_domain_t, nx_result_t, const char* file,
                          unsigned line) {
                nexus_test::RecordTerminalObservation(
                    nx_arch_irq_is_masked() && state.active && state.retained &&
                    state.fault && state.held_cs == &cs && file != nullptr &&
                    line != 0U && (*state.reset & state.reset_mask) != 0U);
            });
        uint8_t tx = 0x12;
        size_t transferred = 0;
        (void)nx_spi_endpoint_transfer(&device, &tx, nullptr, 1, 100,
                                       &transferred);
    }));
}

TEST_F(G474Spi, SpiPadSpeedMatchesTheDatasheetClockCharacterization) {
    EXPECT_EQ(NX_SUCCESS, nx_stm32g474_pin_alternate(&g_g474_model.gpio, 5, 5));
    EXPECT_EQ(3U, (g_g474_model.gpio.OSPEEDR >> 10U) & 3U);
    EXPECT_EQ(5U, (g_g474_model.gpio.AFR[0] >> 20U) & 15U);
}
