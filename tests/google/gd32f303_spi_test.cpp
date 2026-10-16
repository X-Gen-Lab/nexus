/**
 * \file            gd32f303_spi_test.cpp
 * \brief           GD303 real polling transactions, failure drain and CS leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f303_provider.h"
#include "model.h"
#include "nexus/core/fault.h"
#include "terminal_watchdog.h"
#include <gtest/gtest.h>
#include <sys/mman.h>
static nx_gd32f303_spi_state_t* s_spi_fault_state;
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    const bool retained =
        s_spi_fault_state != nullptr && s_spi_fault_state->active &&
        s_spi_fault_state->retained && s_spi_fault_state->fault &&
        s_spi_fault_state->held_cs != nullptr;
    nexus_test::RecordTerminalObservation(
        domain == NX_FAULT_PLATFORM && reason == NX_ERROR_IO &&
        file != nullptr && line != 0U && retained &&
        (GPIO_OCTL(GPIOA) & 16U) == 0U && nx_arch_irq_is_masked());
}
class GD303SPI : public testing::Test {
  protected:
    nx_gd32f303_spi_state_t state{};
    nx_gd32f303_gpio_state_t cs{};
    nx_gd32f303_pin_lease_t lease{};
    nx_gd32f303_spi_endpoint_state_t binding{};
    const nx_spi_port_t port{&nx_gd32f303_spi_ops, &state};
    const nx_spi_endpoint_t endpoint{&nx_gd32f303_spi_endpoint_ops, &binding};
    void* mmio = MAP_FAILED;
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(0x40000000U), 0x30000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(0x40000000U));
        g_gd303_ignore = nullptr;
        g_gd303_writes = g_gd303_spi_bytes = 0;
        g_gd303_spi_bad_cs = g_gd303_spi_hold_rx = false;
        g_gd303_hold_cs_release = g_gd303_hold_spi_reset_release = false;
        s_spi_fault_state = &state;
        g_gd303_irq[SPI0_IRQn] = g_gd303_pending[SPI0_IRQn] = false;
        g_nx_gd32f303_system.started = true;
        g_nx_gd32f303_system.core_hz = 8000000U;
        RCU_APB2EN = RCU_APB2EN_SPI0EN | RCU_APB2EN_AFEN;
        GPIO_CTL0(GPIOA) = 0x44444444U;
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_pins_acquire(&lease, 0xf0));
        cs.mask = 16;
        cs.output = true;
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_gpio_initialize(&cs, 16));
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_spi0_pins_prepare());
        state.registers = reinterpret_cast<nx_gd32f303_spi_registers_t*>(
            static_cast<uintptr_t>(SPI0));
        state.clock_hz = 8000000;
        state.poll_limit = 4;
        SPI_STAT(SPI0) = SPI_STAT_TBE;
        ASSERT_EQ(NX_SUCCESS, nx_gd32f303_spi_initialize(&state));
        binding = {&state, &cs, 16, 1000000, 0};
    }
    void TearDown() override {
        if (mmio != MAP_FAILED)
            munmap(mmio, 0x30000U);
    }
};
TEST_F(GD303SPI, OneWholeCsIntervalAndByteDuplex) {
    uint8_t tx[2]{0x12, 0x34};
    uint8_t rx[2]{};
    size_t transferred = 0;
    ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&endpoint, tx, rx, 2, 1000,
                                                   &transferred));
    EXPECT_EQ(2U, transferred);
    EXPECT_EQ(0xedU, rx[0]);
    EXPECT_EQ(0xcbU, rx[1]);
    EXPECT_FALSE(g_gd303_spi_bad_cs);
    EXPECT_EQ(2U, g_gd303_spi_bytes);
    EXPECT_EQ(16U, GPIO_OCTL(GPIOA) & 16U);
    EXPECT_EQ(0U, SPI_CTL0(SPI0) & SPI_CTL0_SPIEN);
}
TEST_F(GD303SPI, ExpiredDeadlineAndUnsupportedSpanDoNotAssertCs) {
    uint8_t tx = 0x11;
    size_t transferred = 7;
    unsigned before = g_gd303_writes;
    EXPECT_EQ(
        NX_ERROR_TIMEOUT,
        nx_spi_endpoint_transfer(&endpoint, &tx, nullptr, 1, 0, &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_EQ(before, g_gd303_writes);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_spi_endpoint_transfer(&endpoint, &tx, nullptr, 1,
                                       NX_DEADLINE_NEVER, &transferred));
    EXPECT_EQ(before, g_gd303_writes);
}

TEST_F(GD303SPI, MissingRxTimesOutAndResetDrainsBeforeRecovery) {
    uint8_t byte = 0x55;
    size_t transferred = 0;
    g_gd303_spi_hold_rx = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1, 1000,
                                       &transferred));
    EXPECT_EQ(0U, transferred);
    EXPECT_TRUE(state.fault);
    EXPECT_EQ(0U, SPI_STAT(SPI0) & SPI_STAT_TRANS);
    EXPECT_EQ(16U, GPIO_OCTL(GPIOA) & 16U);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1, 1000,
                                       &transferred));
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&port));
    EXPECT_FALSE(state.fault);
}
TEST_F(GD303SPI, RefusedResetKeepsControllerOwnershipForExplicitRetry) {
    g_gd303_ignore = &RCU_APB2RST;
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_stop(&port));
    EXPECT_TRUE(state.retained);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&port));
    EXPECT_FALSE(state.retained);
}
TEST_F(GD303SPI, RefusedChipSelectReleaseRetainsTheExactCsObject) {
    GPIO_OCTL(GPIOA) = 0U;
    state.held_cs = &cs;
    state.held_cs_mask = 16U;
    g_gd303_ignore = &GPIO_BOP(GPIOA);
    EXPECT_EQ(NX_ERROR_IO, nx_spi_port_stop(&port));
    EXPECT_EQ(&cs, state.held_cs);
    EXPECT_TRUE(state.retained);
    g_gd303_ignore = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_spi_port_recover(&port));
    EXPECT_EQ(nullptr, state.held_cs);
    EXPECT_EQ(16U, GPIO_OCTL(GPIOA) & 16U);
}
TEST_F(GD303SPI, BufferOverlapAndOutputAliasingRejectWithoutHardwareEffects) {
    uint8_t data[4]{};
    size_t transferred = 0;
    unsigned before = g_gd303_writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_spi_endpoint_transfer(&endpoint, data, data + 1, 2, 1000,
                                       &transferred));
    EXPECT_EQ(before, g_gd303_writes);
    auto* alias = reinterpret_cast<size_t*>(&state);
    EXPECT_EQ(NX_ERROR_INVALID, nx_spi_endpoint_transfer(
                                    &endpoint, data, nullptr, 1, 1000, alias));
    EXPECT_EQ(before, g_gd303_writes);
}
TEST_F(GD303SPI, SameBufferFullDuplexAndDummyTxRemainExplicit) {
    uint8_t data[2]{0x11, 0x22};
    size_t transferred = 0;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&endpoint, data, data, 2,
                                                   1000, &transferred));
    EXPECT_EQ(0xeeU, data[0]);
    EXPECT_EQ(0xddU, data[1]);
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&endpoint, nullptr, data, 2,
                                                   1000, &transferred));
    EXPECT_EQ(0U, data[0]);
    EXPECT_EQ(0U, data[1]);
}
TEST_F(GD303SPI, DividerAndPhaseAreEndpointFactsOnOneSharedController) {
    binding.frequency_hz = 31250;
    binding.mode = 3;
    uint8_t byte = 0x11;
    size_t transferred = 0;
    EXPECT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1,
                                                   1000, &transferred));
    EXPECT_EQ(SPI_CTL0_CKPL | SPI_CTL0_CKPH,
              SPI_CTL0(SPI0) & (SPI_CTL0_CKPL | SPI_CTL0_CKPH));
    EXPECT_EQ(7U, (SPI_CTL0(SPI0) >> 3U) & 7U);
}

TEST_F(GD303SPI, RefusedFailureResetCannotReturnOrReleaseTheInterval) {
    g_gd303_spi_hold_rx = true;
    g_gd303_ignore = &RCU_APB2RST;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        uint8_t byte = 1U;
        size_t transferred = 0U;
        (void)nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1, 1000,
                                       &transferred);
    }));
}
TEST_F(GD303SPI,
       RefusedResetReleaseKeepsTheExecutionAfterFaultObserverReturns) {
    g_gd303_spi_hold_rx = true;
    g_gd303_hold_spi_reset_release = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        uint8_t byte = 1U;
        size_t transferred = 0U;
        (void)nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1, 1000,
                                       &transferred);
    }));
}
TEST_F(GD303SPI, RefusedCsInactiveCannotReturnAfterACompletedByte) {
    g_gd303_hold_cs_release = true;
    EXPECT_TRUE(nexus_test::ObserveTerminal([&] {
        uint8_t byte = 1U;
        size_t transferred = 0U;
        (void)nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1, 1000,
                                       &transferred);
    }));
}
