/**
 * \file            i2c_official_leaf_test.cpp
 * \brief           Actual default I2C data paths and official leaf semantics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_official_leaf_model.h"
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#include <gtest/gtest.h>
#include <vector>

extern "C" nx_time_us_t nx_time_now_us(void) {
    return 100U;
}

#if defined(NX_I2C_LEAF_V1)
TEST(I2cOfficialLeaf, ReceiveSelectsOnlyEightBitsWithoutChangingData) {
    I2C_TypeDef registers{};
    registers.DR = 0xa1b2c395U;
    EXPECT_EQ(0x95U, nx_i2c_leaf_official_receive(&registers));
    EXPECT_EQ(0xa1b2c395U, registers.DR);
}

TEST(I2cOfficialLeaf, TransmitRmwRetainsOtherBitsAndCannotReplaceWordStore) {
    I2C_TypeDef registers{};
    registers.DR = 0xa1b2c395U;
    nx_i2c_leaf_official_transmit(&registers, 0x26U);
    EXPECT_EQ(0xa1b2c326U, registers.DR);
}

TEST(I2cOfficialLeaf, AddressClearUsesReadOnlyStatusAccess) {
    I2C_TypeDef registers{};
    registers.SR1 = 0x12345678U;
    registers.SR2 = 0x87654321U;
    nx_i2c_leaf_official_clear_address(&registers);
    EXPECT_EQ(0x12345678U, registers.SR1);
    EXPECT_EQ(0x87654321U, registers.SR2);
}

class I2cDefaultLeaf : public testing::Test {
  protected:
    I2C_TypeDef registers{};
    uint32_t lines = 3U;
    nx_i2c_v1_state_t state{};
    nx_i2c_v1_endpoint_t endpoint_state{&state, 0x50U};
    nx_i2c_port_t bus{&nx_i2c_v1_async_ops, &state};
    nx_i2c_endpoint_t endpoint{&nx_i2c_v1_async_endpoint_ops, &endpoint_state};
    nx_i2c_request_t request{};
    uint8_t byte{};
    nx_i2c_message_t message{&byte, 1U, false};

    void SetUp() override {
        state.registers = reinterpret_cast<volatile uint32_t*>(&registers);
        state.line_inputs = &lines;
        state.line_mask = 3U;
#if defined(STM32F103xB)
        state.peripheral_mhz = 8U;
#else
        state.peripheral_mhz = 42U;
#endif
        ASSERT_EQ(NX_SUCCESS, nx_i2c_v1_initialize(&state));
        nx_request_initialize(&request.base);
    }

    void TearDown() override {
        if (state.active != nullptr)
            (void)nx_i2c_port_cancel(&bus, &request);
        registers.CR1 &= ~((1U << 8U) | (1U << 9U));
        registers.SR1 = 0U;
        registers.SR2 = 0U;
        nx_i2c_port_service(&bus);
        EXPECT_EQ(NX_SUCCESS, nx_i2c_port_stop(&bus));
    }

    void Begin(bool read) {
        message.read = read;
        ASSERT_EQ(NX_SUCCESS,
                  nx_i2c_request_prepare(&request, &message, 1U, 1000U));
        ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_submit(&endpoint, &request));
        registers.DR = 0xa1b2c355U;
        registers.SR2 = 3U;
        registers.SR1 = 1U;
        nx_i2c_v1_irq(&state);
        EXPECT_EQ(read ? 0xa1U : 0xa0U, registers.DR);
        registers.SR1 = 2U;
        nx_i2c_v1_irq(&state);
    }

    void Release() {
        EXPECT_EQ(NX_I2C_V1_DRAIN, state.phase);
        EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
        registers.CR1 &= ~(1U << 9U);
        registers.SR1 = 0U;
        registers.SR2 = 0U;
        nx_i2c_port_service(&bus);
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(nullptr, state.active);
    }
};

TEST_F(I2cDefaultLeaf, ReceiveUsesSelectedDataAndRetainsLoanUntilWireDrain) {
    Begin(true);
    ASSERT_EQ(NX_I2C_V1_READ_ONE, state.phase);
    EXPECT_EQ(0U, registers.CR1 & ((1U << 10U) | (1U << 11U)));
    EXPECT_NE(0U, registers.CR1 & (1U << 9U));
    registers.DR = 0xa1b2c395U;
    registers.SR1 = 1U << 6U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(0x95U, byte);
    EXPECT_EQ(0xa1b2c395U, registers.DR);
    EXPECT_EQ(1U, state.transferred);
    Release();
}

TEST_F(I2cDefaultLeaf, TransmitOverwritesWholeDataWordAndPreservesNeighbors) {
    byte = 0x26U;
    Begin(false);
    ASSERT_EQ(NX_I2C_V1_WRITE, state.phase);
    const uint32_t address = registers.OAR1;
    registers.DR = 0xa1b2c395U;
    registers.SR1 = 1U << 7U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(0x26U, registers.DR);
    EXPECT_EQ(address, registers.OAR1);
    registers.SR1 = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(1U, state.transferred);
    Release();
}
#else
namespace {
constexpr uint32_t transmit_sentinel = 0xa1b2c355U;
constexpr uint32_t receive_sentinel = 0xd4e5f600U;
struct Hardware {
    nx_i2c_v2_registers_t* registers{};
    uint32_t* reset{};
    uint32_t command{};
    unsigned remaining{};
    unsigned received{};
    bool offered{};
    std::vector<uint32_t> controls;
    std::vector<uint32_t> transmitted;
    std::vector<uint32_t> received_words;
} hardware;
} /* namespace */

extern "C" void nx_fault_observe(nx_fault_domain_t, nx_result_t, const char*,
                                 unsigned) {
    ADD_FAILURE() << "The bounded default data probe must not park";
}

extern "C" void nx_i2c_leaf_hardware_write(volatile uint32_t* reg,
                                           uint32_t value) {
    nx_i2c_leaf_default_write(reg, value);
    if (reg == &hardware.registers->icr)
        hardware.registers->isr &= ~value;
    if (reg == hardware.reset && (value & 1U) != 0U)
        *hardware.registers = {};
}

extern "C" void nx_i2c_leaf_hardware_poll(nx_i2c_v2_state_t* state) {
    auto* registers = state->config->registers;
    const uint32_t command = registers->cr2;
    if (command != hardware.command) {
        hardware.command = command;
        hardware.controls.push_back(command);
        hardware.remaining = (command >> 16U) & 255U;
        hardware.offered = false;
    }
    if (hardware.offered) {
        if ((command & (1U << 10U)) != 0U)
            hardware.received_words.push_back(
                static_cast<uint32_t>(registers->rxdr));
        else
            hardware.transmitted.push_back(
                static_cast<uint32_t>(registers->txdr));
        --hardware.remaining;
        hardware.offered = false;
    }
    if (hardware.remaining != 0U) {
        registers->txdr = transmit_sentinel;
        registers->rxdr = receive_sentinel | (0x80U + hardware.received);
        if ((command & (1U << 10U)) != 0U)
            ++hardware.received;
        registers->isr =
            (1U << 15U) |
            ((command & (1U << 10U)) != 0U ? (1U << 2U) : (1U << 1U));
        hardware.offered = true;
    } else {
        registers->isr =
            (command & (1U << 25U)) != 0U
                ? (1U << 5U)
                : ((command & (1U << 24U)) != 0U ? (1U << 7U) : (1U << 6U));
    }
}

TEST(I2cOfficialLeaf, ReceiveSelectsOnlyEightBitsWithoutChangingData) {
    I2C_TypeDef registers{};
    registers.RXDR = 0xa1b2c395U;
    EXPECT_EQ(0x95U, nx_i2c_leaf_official_receive(&registers));
    EXPECT_EQ(0xa1b2c395U, registers.RXDR);
}

TEST(I2cOfficialLeaf, TransmitOverwritesWholeDataWordAndPreservesReceive) {
    I2C_TypeDef registers{};
    registers.TXDR = 0xa1b2c395U;
    registers.RXDR = 0x55667788U;
    nx_i2c_leaf_official_transmit(&registers, 0x26U);
    EXPECT_EQ(0x26U, registers.TXDR);
    EXPECT_EQ(0x55667788U, registers.RXDR);
}

class I2cDefaultLeaf : public testing::Test {
  protected:
    nx_i2c_v2_registers_t registers{};
    uint32_t reset{};
    nx_i2c_v2_config_t config{&registers, &reset, 1U,   0x30420f13U,
                              16U,        false,  false};
    nx_i2c_v2_state_t state{};
    nx_i2c_v2_endpoint_state_t endpoint_state{&state, 0x50U};
    nx_i2c_endpoint_t endpoint{&nx_i2c_v2_endpoint_ops, &endpoint_state};
    nx_i2c_port_t bus{&nx_i2c_v2_ops, &state};
    void SetUp() override {
        hardware = {};
        hardware.registers = &registers;
        hardware.reset = &reset;
        ASSERT_EQ(NX_SUCCESS, nx_i2c_v2_bind(&state, &config));
        ASSERT_EQ(NX_SUCCESS, nx_i2c_v2_initialize(&state));
    }
    void TearDown() override {
        EXPECT_FALSE(state.active);
        EXPECT_EQ(NX_SUCCESS, nx_i2c_port_stop(&bus));
        hardware = {};
    }
};

TEST_F(I2cDefaultLeaf, TransmitUsesDefaultWordStoreForEachSelectedByte) {
    uint8_t bytes[] = {0x26U, 0x39U};
    nx_i2c_message_t message{bytes, sizeof(bytes), false};
    size_t done{};
    ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_transaction(&endpoint, &message, 1U,
                                                      1000U, &done));
    EXPECT_EQ(sizeof(bytes), done);
    EXPECT_EQ((std::vector<uint32_t>{0x26U, 0x39U}), hardware.transmitted);
    EXPECT_EQ(0U, registers.cr2);
    EXPECT_EQ(0U, registers.isr);
}

TEST_F(I2cDefaultLeaf, ReceiveUsesDefaultLoadAndPreservesUpperDataBits) {
    uint8_t bytes[2]{};
    nx_i2c_message_t message{bytes, sizeof(bytes), true};
    size_t done{};
    ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_transaction(&endpoint, &message, 1U,
                                                      1000U, &done));
    EXPECT_EQ(sizeof(bytes), done);
    EXPECT_EQ(0x80U, bytes[0]);
    EXPECT_EQ(0x81U, bytes[1]);
    EXPECT_EQ((std::vector<uint32_t>{receive_sentinel | 0x80U,
                                     receive_sentinel | 0x81U}),
              hardware.received_words);
    EXPECT_TRUE(hardware.transmitted.empty());
}
#endif
