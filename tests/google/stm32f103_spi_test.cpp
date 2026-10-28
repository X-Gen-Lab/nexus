/**
 * \file            stm32f103_spi_test.cpp
 * \brief           Actual F103 short polling SPI and static CS contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "nexus/core/fault.h"
#include "stm32f103_provider.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
class SpiFaultBoundary {
  public:
    MOCK_METHOD(void, Observe, ());
};
struct SpiPark {};
static SpiFaultBoundary* s_spi_fault;
class F103Spi : public testing::Test {
  protected:
    nx_stm32f103_spi_state_t state{};
    nx_stm32f103_gpio_state_t cs{};
    nx_stm32f103_spi_endpoint_state_t endpoint{};
    const nx_spi_port_t port{&nx_stm32f103_spi_ops, &state};
    const nx_spi_endpoint_t face{&nx_stm32f103_spi_endpoint_ops, &endpoint};
    void TearDown() override {
        if (nx_arch_irq_is_masked())
            nx_arch_irq_restore(nx_arch_irq_state_t{0U});
        s_spi_fault = nullptr;
    }
    void SetUp() override {
        nx_f103_model_reset();
        g_nx_stm32f103_system.started = true;
        g_nx_stm32f103_system.core_hz = 8000000U;
        g_f103_model.rcc.APB2ENR =
            RCC_APB2ENR_SPI1EN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
        g_f103_model.timer.CNT = 100U;
        state.registers = &g_f103_model.spi;
        state.rcc = &g_f103_model.rcc;
        state.clock_hz = 8000000U;
        cs.registers = &g_f103_model.gpio;
        cs.mask = 16U;
        cs.output = cs.initialized = cs.retained = true;
        g_f103_model.gpio.ODR = 16U;
        endpoint.port = &state;
        endpoint.cs = &cs;
        endpoint.cs_mask = 16U;
        endpoint.mode = 0U;
        endpoint.frequency_hz = 1000000U;
        g_f103_model.spi.SR = SPI_SR_TXE;
    }
};
TEST_F(F103Spi, ExactColdStateInitializesWithoutStartingWire) {
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(0U, g_f103_model.spi.CR1 & SPI_CR1_SPE);
    EXPECT_EQ(0U, g_f103_model.spi.CR2);
}
TEST_F(F103Spi, ForeignDmaOrEnabledSpiRejectsBeforeEffects) {
    g_f103_model.spi.CR2 = SPI_CR2_RXDMAEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_spi_initialize(&state));
    EXPECT_EQ(0U, g_f103_model.writes);
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Spi, PollingTransferHoldsOneCsAndDrainsWire) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    uint8_t tx[] = {0x12U, 0x34U};
    uint8_t rx[2]{};
    size_t count = 99U;
    EXPECT_EQ(NX_SUCCESS,
              nx_spi_endpoint_transfer(&face, tx, rx, 2, 1000, &count));
    EXPECT_EQ(2U, count);
    EXPECT_EQ(0xedU, rx[0]);
    EXPECT_EQ(0xcbU, rx[1]);
    EXPECT_EQ(1U, g_f103_model.cs_assertions);
    EXPECT_EQ(1U, g_f103_model.cs_releases);
    EXPECT_FALSE(g_f103_model.spi_cs_violation);
    EXPECT_EQ(16U, g_f103_model.gpio.ODR & 16U);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(0U, g_f103_model.spi.CR1 & SPI_CR1_SPE);
}
TEST_F(F103Spi, TimeoutResetsWireBeforeReturningAndRequiresRecovery) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    g_f103_model.hold_spi_rx = true;
    uint8_t tx = 0x5aU;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_spi_endpoint_transfer(&face, &tx, nullptr, 1, 105, &count));
    EXPECT_EQ(0U, count);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, g_f103_model.spi.SR & SPI_SR_BSY);
    EXPECT_EQ(16U, g_f103_model.gpio.ODR & 16U);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_spi_endpoint_transfer(&face, &tx, nullptr, 1, 1000, &count));
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&port));
    EXPECT_FALSE(state.fault);
}
TEST_F(F103Spi, DeadlineCrossingDuringCsReleaseReturnsOnlyAfterInactiveProof) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    g_f103_model.advance_on_cs_release = 1000U;
    uint8_t tx = 1U;
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_spi_endpoint_transfer(&face, &tx, nullptr, 1, 500, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(16U, g_f103_model.gpio.ODR & 16U);
    EXPECT_FALSE(state.active);
    EXPECT_EQ(0U, g_f103_model.spi.SR & SPI_SR_BSY);
}
TEST_F(F103Spi, InvalidBuffersAndNeverDeadlineHaveNoCsOrHardwareEffects) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    uint8_t tx = 0U;
    size_t count = 99U;
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_transfer(
                                    &face, nullptr, nullptr, 1, 1000, &count));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_spi_endpoint_transfer(&face, &tx, nullptr, 1,
                                       NX_DEADLINE_NEVER, &count));
    EXPECT_EQ(before, g_f103_model.writes);
    EXPECT_EQ(0U, g_f103_model.cs_assertions);
}
TEST_F(F103Spi, ReceiveOutputCannotOverwriteProviderOrCsState) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(&face, nullptr,
                                       reinterpret_cast<uint8_t*>(&state), 1,
                                       1000, &count));
    EXPECT_EQ(0U, g_f103_model.cs_assertions);
}
TEST_F(F103Spi, StopRejectsActiveExecutionAndRetainsStateOnWithdrawalFailure) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    state.active = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_spi_port_stop(&port));
    state.active = false;
    g_f103_model.spi.CR1 |= SPI_CR1_SPE;
    g_f103_model.ignore_write = &g_f103_model.spi.CR1;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_stop(&port));
    EXPECT_TRUE(state.retained);
    g_f103_model.ignore_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&port));
    EXPECT_FALSE(state.retained);
}
TEST_F(F103Spi, GpioCsRouteAcceptsPa4AndPreservesPa5) {
    cs.initialized = cs.retained = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f103_gpio_initialize(&cs, 16U));
    EXPECT_EQ(2U, (g_f103_model.gpio.CRL >> 16U) & 15U);
    EXPECT_EQ(4U, (g_f103_model.gpio.CRL >> 20U) & 15U);
}

TEST_F(F103Spi, UnresettableWireRetainsTheWholeExecutionBeforeFault) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    g_f103_model.hold_spi_rx = true;
    g_f103_model.hold_spi_reset = true;
    testing::StrictMock<SpiFaultBoundary> boundary;
    s_spi_fault = &boundary;
    EXPECT_CALL(boundary, Observe()).WillOnce(testing::Throw(SpiPark{}));
    uint8_t tx = 1U;
    size_t count = 0U;
    EXPECT_THROW(nx_spi_endpoint_transfer(&face, &tx, nullptr, 1, 105, &count),
                 SpiPark);
    EXPECT_TRUE(state.active);
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(nx_arch_irq_is_masked());
    EXPECT_EQ(0U, g_f103_model.gpio.ODR & 16U);
}
TEST_F(F103Spi, FailedCsReleaseCannotReturnACompletedInterval) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_initialize(&state));
    g_f103_model.hold_cs_high = true;
    testing::StrictMock<SpiFaultBoundary> boundary;
    s_spi_fault = &boundary;
    EXPECT_CALL(boundary, Observe()).WillOnce(testing::Throw(SpiPark{}));
    uint8_t tx = 1U;
    size_t count = 0U;
    EXPECT_THROW(nx_spi_endpoint_transfer(&face, &tx, nullptr, 1, 1000, &count),
                 SpiPark);
    EXPECT_TRUE(state.active);
    EXPECT_TRUE(state.retained);
    EXPECT_EQ(0U, g_f103_model.gpio.ODR & 16U);
}
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    (void)domain;
    (void)reason;
    (void)file;
    (void)line;
    if (s_spi_fault != nullptr)
        s_spi_fault->Observe();
}

TEST_F(F103Spi, StaticPrepareRejectsRetainedCsBeforeStateBindingOrPinWrites) {
    state = {};
    uint32_t before = g_f103_model.writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f103_spi_prepare(&state, &cs));
    EXPECT_EQ(nullptr, state.registers);
    EXPECT_EQ(before, g_f103_model.writes);
}
TEST_F(F103Spi, StaticPrepareAndCloseKeepChipSelectInactiveAndWithdrawOwner) {
    state = {};
    cs = {};
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_prepare(&state, &cs));
    EXPECT_TRUE(state.retained);
    EXPECT_TRUE(cs.retained);
    EXPECT_EQ(16U, g_f103_model.gpio.ODR & 16U);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f103_spi_close(&state, &cs));
    EXPECT_FALSE(state.retained);
    EXPECT_FALSE(cs.retained);
    EXPECT_EQ(16U, g_f103_model.gpio.ODR & 16U);
}
