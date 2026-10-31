/**
 * \file            u575_spi_byte_test.cpp
 * \brief           Actual U575 default byte accesses and official LL width
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
#include "u575_spi_byte_model.h"
#include <gtest/gtest.h>
#include <vector>
SPI_TypeDef g_u575_spi_byte_decoy;
namespace {
constexpr uint32_t tx_sentinel = 0xa1b2c300U;
constexpr uint32_t rx_sentinel = 0x5e6f7000U;
struct ByteHardware {
    unsigned length;
    unsigned received;
    bool receive_next;
    bool wire_complete;
    std::vector<uint32_t> writes;
    std::vector<uint32_t> reads;
} hardware;
} /* namespace */
/** \brief Schedule external FIFO facts; never replace the byte load/store. */
extern "C" void nx_u575_spi_byte_poll(void* context) {
    auto* port = static_cast<nx_stm32u575_spi_state_t*>(context);
    auto* registers = port->registers;
    nx_u575_spi_poll(context);
    if (hardware.wire_complete)
        return;
    if (hardware.receive_next) {
        hardware.writes.push_back(static_cast<uint32_t>(registers->TXDR));
        registers->RXDR = rx_sentinel | (0x80U + hardware.received);
        registers->SR |= SPI_SR_RXP;
        ++hardware.received;
        hardware.receive_next = false;
        return;
    }
    if (hardware.received != 0U)
        hardware.reads.push_back(static_cast<uint32_t>(registers->RXDR));
    registers->SR &= ~static_cast<uint32_t>(SPI_SR_RXP);
    if (hardware.received == hardware.length) {
        registers->SR |= SPI_SR_EOT | SPI_SR_TXC;
        hardware.wire_complete = true;
    } else {
        registers->TXDR = tx_sentinel;
        registers->SR |= SPI_SR_TXP;
        hardware.receive_next = true;
    }
}
TEST(U575SpiOfficialByte, ExactByteAccessPreservesTheOtherRegisterBytes) {
    SPI_TypeDef registers{};
    registers.TXDR = tx_sentinel | 0x44U;
    registers.RXDR = rx_sentinel | 0x93U;
    nx_u575_spi_official_byte_write(&registers, 0x25U);
    EXPECT_EQ(tx_sentinel | 0x25U, registers.TXDR);
    EXPECT_EQ(0x93U, nx_u575_spi_official_byte_read(&registers));
    EXPECT_EQ(rx_sentinel | 0x93U, registers.RXDR);
}
class U575SpiDefaultByte : public testing::Test {
  protected:
    SPI_TypeDef registers{};
    nx_stm32u575_spi_state_t state{};
    nx_stm32u575_gpio_state_t cs{};
    nx_stm32u575_spi_endpoint_state_t endpoint{};
    nx_spi_endpoint_t device{&nx_stm32u575_spi_endpoint_ops, &endpoint};
    void SetUp() override {
        nx_u575_devices_reset();
        hardware = {};
        g_u575_spi_byte_decoy = {};
        g_u575_spi_byte_decoy.TXDR = 0x10203040U;
        g_u575_spi_byte_decoy.RXDR = 0x50607080U;
        g_u575_spi = &registers;
        registers.SR = SPI_SR_TXP | SPI_SR_TXC;
        registers.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
        g_nx_stm32u575_system.started = true;
        g_nx_stm32u575_system.core_hz = 4000000U;
        g_u575_model.rcc.APB2ENR = RCC_APB2ENR_SPI1EN;
        state.registers = &registers;
        state.reset = &g_u575_model.rcc.APB2RSTR;
        state.reset_mask = RCC_APB2RSTR_SPI1RST;
        state.clock_hz = 4000000U;
        state.poll_limit = 8U;
        cs = {&g_u575_model.gpioa, 16U, true, false, false};
        endpoint = {&state, &cs, 16U, 1000000U, 0U};
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_gpio_initialize(&cs, 16U));
        ASSERT_EQ(NX_SUCCESS, nx_stm32u575_spi_initialize(&state));
    }
    void TearDown() override {
        EXPECT_EQ(0x10203040U, g_u575_spi_byte_decoy.TXDR);
        EXPECT_EQ(0x50607080U, g_u575_spi_byte_decoy.RXDR);
        g_u575_spi = nullptr;
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
    }
};
TEST_F(U575SpiDefaultByte, RealDefaultPortsUseSelectedRamAndOnlyEightBits) {
    const uint8_t tx[] = {0x12U, 0x34U};
    uint8_t rx[2]{};
    Transfer(tx, rx, 2U);
    EXPECT_EQ(0x80U, rx[0]);
    EXPECT_EQ(0x81U, rx[1]);
}
TEST_F(U575SpiDefaultByte, ReceiveOnlyEmitsTheExactDummyByte) {
    uint8_t rx{};
    Transfer(nullptr, &rx, 1U);
    EXPECT_EQ(0x80U, rx);
}
TEST_F(U575SpiDefaultByte, TransmitOnlyStillConsumesTheReceivedByte) {
    const uint8_t tx = 0xabU;
    Transfer(&tx, nullptr, 1U);
}
