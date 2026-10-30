/**
 * \file            h743_spi_byte_test.cpp
 * \brief           Actual H743 default byte ports and locked LL access width
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_spi_byte_model.h"
#include "nexus/core/fault.h"
#include "stm32h743_provider.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <vector>

SPI_TypeDef g_h743_spi_byte_decoy;
namespace {
constexpr uint32_t tx_sentinel = 0xa1b2c300U;
constexpr uint32_t rx_sentinel = 0x5e6f7000U;
struct ByteHardware {
    unsigned length;
    unsigned received;
    bool awaiting_rx;
    bool wire_complete;
    std::vector<uint32_t> writes;
    std::vector<uint32_t> reads;
} hardware;
} /* namespace */

extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    ADD_FAILURE() << "Unexpected nonreturning SPI withdrawal failure";
    std::abort();
}
/** \brief Supply external readiness without replacing the byte access. */
extern "C" void nx_h743_spi_byte_poll(void* context) {
    auto* port = static_cast<nx_stm32h743_spi_state_t*>(context);
    auto* registers = port->registers;
    nx_h743_model_spi_poll(context);
    if (hardware.wire_complete)
        return;
    if (hardware.awaiting_rx) {
        hardware.writes.push_back(static_cast<uint32_t>(registers->TXDR));
        registers->RXDR = rx_sentinel | (0x80U + hardware.received);
        registers->SR |= SPI_SR_RXP;
        ++hardware.received;
        hardware.awaiting_rx = false;
        return;
    }
    if (hardware.received != 0U)
        hardware.reads.push_back(static_cast<uint32_t>(registers->RXDR));
    registers->SR &= ~static_cast<uint32_t>(SPI_SR_RXP);
    if (hardware.received == hardware.length) {
        registers->SR |= SPI_SR_EOT | SPI_SR_TXC;
        registers->CR1 &= ~static_cast<uint32_t>(SPI_CR1_CSTART);
        const uint32_t divisor =
            2U << ((registers->CFG1 & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos);
        g_h743_model.spi_tail_until =
            g_h743_model.timer.CNT + (divisor + 63U) / 64U;
        hardware.wire_complete = true;
    } else {
        registers->TXDR = tx_sentinel;
        registers->SR |= SPI_SR_TXP;
        hardware.awaiting_rx = true;
    }
}

TEST(H743SpiOfficialByte, ExactByteAccessPreservesOtherRegisterBytes) {
    SPI_TypeDef registers{};
    registers.TXDR = tx_sentinel | 0x44U;
    registers.RXDR = rx_sentinel | 0x93U;
    nx_h743_spi_official_byte_write(&registers, 0x25U);
    EXPECT_EQ(tx_sentinel | 0x25U, registers.TXDR);
    EXPECT_EQ(0x93U, nx_h743_spi_official_byte_read(&registers));
    EXPECT_EQ(rx_sentinel | 0x93U, registers.RXDR);
}

class H743SpiDefaultByte : public testing::Test {
  protected:
    SPI_TypeDef registers{};
    nx_stm32h743_spi_state_t state{};
    nx_stm32h743_gpio_state_t cs{};
    nx_stm32h743_spi_endpoint_state_t endpoint{};
    nx_spi_port_t port{&nx_stm32h743_spi_ops, &state};
    nx_spi_endpoint_t device{&nx_stm32h743_spi_endpoint_ops, &endpoint};
    void SetUp() override {
        nx_h743_model_reset();
        hardware = {};
        g_h743_spi_byte_decoy = {};
        g_h743_spi_byte_decoy.TXDR = 0x10203040U;
        g_h743_spi_byte_decoy.RXDR = 0x50607080U;
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.rcc.APB2ENR = RCC_APB2ENR_SPI4EN;
        g_h743_model.rcc.AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
        g_h743_model.spi = &registers;
        registers.SR = SPI_SR_TXP | SPI_SR_TXC;
        registers.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
        state.registers = &registers;
        state.reset = &g_h743_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI4RST;
        state.clock_hz = 64000000U;
        state.poll_limit = 8U;
        cs = {&g_h743_model.gpio_e, 1U << 11U, true, false, false};
        endpoint = {&state, &cs, cs.mask, 1000000U, 0U};
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_gpio_initialize(&cs, cs.mask));
        for (unsigned pin : {12U, 13U, 14U})
            ASSERT_EQ(NX_SUCCESS, nx_stm32h743_pin_alternate(
                                      &g_h743_model.gpio_e, pin, 5U));
        g_h743_model.spi_watch_pins = true;
        ASSERT_EQ(NX_SUCCESS, nx_stm32h743_spi_initialize(&state));
    }
    void TearDown() override {
        EXPECT_EQ(0x10203040U, g_h743_spi_byte_decoy.TXDR);
        EXPECT_EQ(0x50607080U, g_h743_spi_byte_decoy.RXDR);
        EXPECT_EQ(NX_SUCCESS, nx_spi_port_stop(&port));
        EXPECT_EQ(NX_SUCCESS, nx_stm32h743_gpio_stop(&cs, cs.mask));
        g_h743_model.spi = nullptr;
    }
    void Transfer(const uint8_t* tx, uint8_t* rx, unsigned length) {
        hardware.length = length;
        size_t count = 99U;
        ASSERT_EQ(NX_SUCCESS, nx_spi_endpoint_transfer(&device, tx, rx, length,
                                                       10000U, &count));
        EXPECT_EQ(length, count);
        ASSERT_EQ(length, hardware.writes.size());
        ASSERT_EQ(length, hardware.reads.size());
        for (unsigned i = 0U; i < length; ++i) {
            EXPECT_EQ(tx_sentinel | (tx == nullptr ? 0xffU : tx[i]),
                      hardware.writes[i]);
            EXPECT_EQ(rx_sentinel | (0x80U + i), hardware.reads[i]);
        }
        EXPECT_EQ(0U, registers.CR1 & SPI_CR1_SPE);
        EXPECT_EQ(cs.mask, cs.registers->ODR & cs.mask);
        EXPECT_FALSE(state.active);
        EXPECT_EQ(0U, g_h743_model.spi_sent);
        EXPECT_EQ(0U, g_h743_model.spi_received);
        EXPECT_EQ(0U, g_h743_model.spi_reset_order_errors);
    }
};
TEST_F(H743SpiDefaultByte, SelectedRamUsesOnlyEightRegisterBits) {
    const uint8_t tx[] = {0x12U, 0x34U};
    uint8_t rx[2]{};
    Transfer(tx, rx, 2U);
    EXPECT_EQ(0x80U, rx[0]);
    EXPECT_EQ(0x81U, rx[1]);
}
TEST_F(H743SpiDefaultByte, ReceiveOnlyEmitsExactDummyByte) {
    uint8_t rx{};
    Transfer(nullptr, &rx, 1U);
    EXPECT_EQ(0x80U, rx);
}
TEST_F(H743SpiDefaultByte, TransmitOnlyStillConsumesReceiveReadiness) {
    const uint8_t tx = 0xabU;
    Transfer(&tx, nullptr, 1U);
}
