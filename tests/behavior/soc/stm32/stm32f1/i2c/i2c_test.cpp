/**
 * \file            i2c_test.cpp
 * \brief           Exercise live F103 polling I2C with a register bus
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/i2c/instance.h"
#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include "stm32f1xx.h"
#include <array>
#include <gtest/gtest.h>

I2C_TypeDef f103_i2c{};
RCC_TypeDef f103_i2c_rcc{};
AFIO_TypeDef f103_i2c_afio{};
GPIO_TypeDef f103_i2c_gpio{};

static constexpr uint32_t scl = 1U << 6U;
static constexpr uint32_t sda = 1U << 7U;
static constexpr uint32_t lines = scl | sda;
static constexpr uint8_t address = 0x49U;

static struct {
    nx_time_us_t now;
    uint32_t writes;
    uint32_t starts;
    uint32_t stops;
    uint32_t resets;
    uint32_t holds;
    uint32_t reads;
    uint32_t address_reads;
    uint32_t address_writes;
    uint32_t bytes_sent;
    uint32_t nack_tx_index;
    uint32_t late_error;
    uint32_t controls_after_loss;
    size_t read_length;
    bool arbitration;
    bool lose_start;
    bool hold_stop;
    bool false_busy;
    bool read_direction;
} bus;

extern "C" uint32_t f103_i2c_read(const volatile uint32_t* field) {
    if (field == &I2C1->SR2 && (I2C1->SR1 & I2C_SR1_ADDR) != 0U) {
        EXPECT_GT(bus.address_reads, 0U);
        if (bus.read_direction) {
            const bool held = (GPIOB->IDR & scl) == 0U;
            const uint32_t flags = I2C1->CR1 & (I2C_CR1_ACK | I2C_CR1_POS);
            EXPECT_EQ(bus.read_length == 1U   ? 0U
                      : bus.read_length == 2U ? I2C_CR1_ACK | I2C_CR1_POS
                                              : I2C_CR1_ACK,
                      flags);
            EXPECT_EQ(bus.read_length <= 2U, held);
        }
        I2C1->SR1 &= ~(uint32_t)I2C_SR1_ADDR;
        I2C1->SR1 |=
            bus.read_direction ? (I2C_SR1_BTF | I2C_SR1_RXNE) : I2C_SR1_TXE;
    } else if (field == &I2C1->SR1 && (I2C1->SR1 & I2C_SR1_ADDR) != 0U) {
        ++bus.address_reads;
    } else if (field == &I2C1->DR) {
        if (bus.reads + 1U == bus.read_length) {
            I2C1->SR1 |= bus.late_error;
            if ((bus.late_error & I2C_SR1_ARLO) != 0U) {
                I2C1->SR2 &= ~(uint32_t)I2C_SR2_MSL;
            }
        }
        return 0x30U + bus.reads++;
    }
    return *field;
}

extern "C" void f103_i2c_write(volatile uint32_t* field, uint32_t value) {
    ++bus.writes;
    if (field == &I2C1->CR1) {
        if ((bus.late_error & I2C_SR1_ARLO) != 0U &&
            bus.reads == bus.read_length && bus.read_length != 0U) {
            ++bus.controls_after_loss;
        }
        if ((value & (I2C_CR1_START | I2C_CR1_STOP)) != 0U &&
            bus.read_direction && (I2C1->SR1 & I2C_SR1_SB) == 0U) {
            EXPECT_EQ(0U, value & I2C_CR1_ACK);
            EXPECT_EQ(0U, GPIOB->IDR & scl);
        }
        if ((value & I2C_CR1_SWRST) != 0U) {
            ++bus.resets;
            I2C1->CR2 = 0U;
            I2C1->SR1 = 0U;
            I2C1->SR2 = bus.false_busy ? I2C_SR2_BUSY : 0U;
        }
        if ((value & I2C_CR1_START) != 0U) {
            ++bus.starts;
            if (!bus.lose_start) {
                I2C1->SR1 = I2C_SR1_SB | I2C_SR1_RXNE;
                I2C1->SR2 = I2C_SR2_BUSY | I2C_SR2_MSL;
                value &= ~(uint32_t)I2C_CR1_START;
            }
        }
        if ((value & I2C_CR1_STOP) != 0U) {
            ++bus.stops;
            if (!bus.hold_stop) {
                I2C1->SR2 = 0U;
                value &= ~(uint32_t)I2C_CR1_STOP;
            }
            bus.read_direction = false;
        }
    } else if (field == &I2C1->DR) {
        if ((I2C1->SR1 & I2C_SR1_SB) != 0U) {
            EXPECT_EQ(address, value >> 1U);
            bus.read_direction = (value & 1U) != 0U;
            if (bus.read_direction) {
                ++bus.address_reads;
            } else {
                ++bus.address_writes;
            }
            I2C1->SR1 = I2C_SR1_ADDR;
            if (bus.arbitration) {
                I2C1->SR1 = I2C_SR1_ARLO;
                I2C1->SR2 = 0U;
            }
        } else {
            ++bus.bytes_sent;
            I2C1->SR1 = bus.bytes_sent == bus.nack_tx_index
                            ? I2C_SR1_AF
                            : (I2C_SR1_BTF | I2C_SR1_TXE);
        }
    } else if (field == &GPIOB->BSRR) {
        GPIOB->ODR = (GPIOB->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
    } else if (field == &GPIOB->CRL) {
        if (((value >> 24U) & 0xFU) == 6U && (GPIOB->ODR & scl) == 0U) {
            ++bus.holds;
            GPIOB->IDR &= ~scl;
        } else {
            GPIOB->IDR |= scl;
        }
    } else if (field == &I2C1->SR1) {
        I2C1->SR1 &= value;
        return;
    }
    *field = value;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++bus.now;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return true;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
extern "C" bool nx_arch_in_isr(void) {
    return false;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return false;
}
extern "C" void nx_arch_dsb(void) {
}

class F103I2C : public testing::Test {
  protected:
    nx_stm32f1_i2c_t state{};
    nx_stm32f1_i2c_endpoint_t addressed{&state, address};
    const nx_i2c_port_t controller{&nx_stm32f1_i2c_ops, &state};
    const nx_i2c_endpoint_t face{&nx_stm32f1_i2c_endpoint_ops, &addressed};

    void SetUp() override {
        bus = {};
        I2C1->CR1 = 0U;
        I2C1->CR2 = 0U;
        I2C1->SR1 = 0U;
        I2C1->SR2 = 0U;
        RCC->APB1RSTR = 0U;
        RCC->APB1ENR = 0U;
        RCC->APB2ENR = 0U;
        RCC->CFGR = 0U;
        AFIO->MAPR = 0U;
        GPIOB->CRL = UINT32_C(0x44444444);
        GPIOB->IDR = lines;
        GPIOB->ODR = 0U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_i2c_prepare(&state, 8000000U, 80000U));
    }
};

TEST_F(F103I2C, SupportsWriteRepeatedStartAndShortReads) {
    ASSERT_TRUE(nx_i2c_endpoint_on_port(&face, &controller));
    EXPECT_EQ(50U, I2C1->CCR);
    ASSERT_EQ(9U, I2C1->TRISE);
    const uint8_t index = 5U;
    for (size_t length : {1U, 2U, 3U, 4U}) {
        std::array<uint8_t, 4> output{};
        nx_i2c_message_t messages[] = {
            nx_i2c_message_write(&index, 1U),
            nx_i2c_message_read(output.data(), length)};
        const auto before = bus.reads;
        bus.read_length = length;
        size_t progress = 0U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_i2c_endpoint_transaction(&face, messages, 2U,
                                              bus.now + 500U, &progress));
        EXPECT_EQ(length + 1U, progress);
        for (size_t i = 0; i < length; ++i) {
            EXPECT_EQ(static_cast<uint8_t>(0x30U + before + i), output[i]);
        }
        EXPECT_EQ(lines, GPIOB->ODR & lines);
    }
    EXPECT_EQ(8U, bus.starts);
    EXPECT_EQ(4U, bus.stops);
    EXPECT_EQ(5U, bus.holds);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_i2c_close(&state));
    EXPECT_EQ(0U, RCC->APB1ENR);
    EXPECT_EQ(0U, RCC->APB2ENR);
    EXPECT_EQ(UINT32_C(0x44444444), GPIOB->CRL);
}

TEST_F(F103I2C, PreservesAddressAndConsecutiveRestartAfterRead) {
    uint8_t input[2]{};
    const uint8_t output = 0x65U;
    nx_i2c_message_t messages[] = {nx_i2c_message_read(input, sizeof(input)),
                                   nx_i2c_message_write(&output, 1U)};
    bus.read_length = sizeof(input);
    size_t progress = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_i2c_endpoint_transaction(
                              &face, messages, 2U, bus.now + 100U, &progress));
    EXPECT_EQ(2U, bus.starts);
    EXPECT_EQ(1U, bus.stops);
    EXPECT_EQ(3U, progress);
    EXPECT_EQ(0x30U, input[0]);
    EXPECT_EQ(0x31U, input[1]);
}

TEST_F(F103I2C, RejectsMisroutedPinsClockAndAliasedProgress) {
    uint8_t data = 1U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    const auto writes = bus.writes;
    auto* aliased = reinterpret_cast<size_t*>(&message);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 100U,
                                          aliased));
    EXPECT_EQ(writes, bus.writes);
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_i2c_close(&state));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_i2c_prepare(&state, 8000000U, 100000U));
    EXPECT_EQ(0U, RCC->APB1ENR);
    AFIO->MAPR = AFIO_MAPR_I2C1_REMAP;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f1_i2c_prepare(&state, 8000000U, 80000U));
    EXPECT_EQ(static_cast<uint32_t>(AFIO_MAPR_I2C1_REMAP), AFIO->MAPR);
    EXPECT_EQ(0U, RCC->APB2ENR);
}

TEST_F(F103I2C, ArbitrationLossNeverIssuesStopAndNackReportsSubmittedBytes) {
    uint8_t data[2] = {0U, 1U};
    nx_i2c_message_t message = nx_i2c_message_write(data, sizeof(data));
    size_t progress = 8U;
    bus.arbitration = true;
    ASSERT_EQ(NX_ERROR_ARBITRATION,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 200U,
                                          &progress));
    EXPECT_EQ(0U, progress);
    EXPECT_EQ(0U, bus.stops);
    bus.arbitration = false;
    bus.nack_tx_index = 2U;
    ASSERT_EQ(NX_ERROR_NACK,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 200U,
                                          &progress));
    EXPECT_EQ(2U, progress);
    EXPECT_EQ(1U, bus.stops);
}

TEST_F(F103I2C, StartTimeoutResetsLocalControllerThenNeedsExplicitRecovery) {
    uint8_t data = 1U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t progress = 7U;
    bus.lose_start = true;
    ASSERT_EQ(NX_ERROR_TIMEOUT,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 20U,
                                          &progress));
    EXPECT_EQ(0U, progress);
    EXPECT_EQ(1U, bus.resets);
    EXPECT_EQ(0U, bus.stops);
    ASSERT_EQ(NX_ERROR_STATE,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 20U,
                                          &progress));
    bus.lose_start = false;
    ASSERT_EQ(NX_SUCCESS, nx_i2c_port_recover(&controller));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_transaction(
                              &face, &message, 1U, bus.now + 20U, &progress));
}

TEST_F(F103I2C, BusyAndStuckBusNeverDriveRecoveryClocks) {
    I2C1->SR2 = I2C_SR2_BUSY;
    bus.false_busy = true;
    uint8_t data = 1U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t progress = 0U;
    const auto writes = bus.writes;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 20U,
                                          &progress));
    EXPECT_EQ(writes, bus.writes);
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_port_recover(&controller));
    GPIOB->IDR &= ~sda;
    const auto resets = bus.resets;
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_port_recover(&controller));
    EXPECT_EQ(resets, bus.resets);
    EXPECT_EQ(0U, bus.holds);
    EXPECT_EQ(0U, bus.starts);
    EXPECT_EQ(0U, bus.stops);
}

TEST_F(F103I2C, StopTimeoutRetainsFaultAfterAllPayloadBytes) {
    const uint8_t data = 8U;
    nx_i2c_message_t message = nx_i2c_message_write(&data, 1U);
    size_t progress = 0U;
    bus.hold_stop = true;
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_endpoint_transaction(
                               &face, &message, 1U, bus.now + 100U, &progress));
    EXPECT_EQ(1U, progress);
    EXPECT_EQ(1U, bus.stops);
    EXPECT_EQ(1U, bus.resets);
    EXPECT_EQ(NX_ERROR_STATE,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 100U,
                                          &progress));
}

TEST_F(F103I2C, LateBusErrorDuringLastReadCannotReportSuccess) {
    uint8_t data = 0U;
    nx_i2c_message_t message = nx_i2c_message_read(&data, 1U);
    size_t progress = 0U;
    bus.read_length = 1U;
    bus.late_error = I2C_SR1_BERR;
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_endpoint_transaction(
                               &face, &message, 1U, bus.now + 100U, &progress));
    EXPECT_EQ(1U, progress);
    EXPECT_EQ(0U, I2C1->SR1 & I2C_SR1_BERR);
}

TEST_F(F103I2C, LateArbitrationLossForbidsFurtherControlWrites) {
    uint8_t data = 0U;
    nx_i2c_message_t message = nx_i2c_message_read(&data, 1U);
    size_t progress = 0U;
    bus.read_length = 1U;
    bus.late_error = I2C_SR1_ARLO;
    EXPECT_EQ(NX_ERROR_ARBITRATION,
              nx_i2c_endpoint_transaction(&face, &message, 1U, bus.now + 100U,
                                          &progress));
    EXPECT_EQ(0U, bus.controls_after_loss);
    EXPECT_EQ(0U, I2C1->SR1 & I2C_SR1_ARLO);
}

TEST_F(F103I2C, KeepsPeripheralInResetWhileExternalLineRemainsLow) {
    GPIOB->IDR &= ~sda;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_i2c_close(&state));
    EXPECT_EQ(static_cast<uint32_t>(I2C_CR1_SWRST), I2C1->CR1);
    EXPECT_NE(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    GPIOB->IDR |= sda;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_i2c_close(&state));
    EXPECT_EQ(0U, RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
}
