/**
 * \file            target_test.cpp
 * \brief           Receive-only F103 target and source withdrawal model.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/i2c/target.h"
#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>

namespace {
constexpr uint32_t wires = GPIO_ODR_ODR6 | GPIO_ODR_ODR7;
constexpr uint32_t irq_sources =
    I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN;
struct Vector {
    uint32_t enabled;
    uint32_t pending;
    uint32_t running;
    uint32_t priority;
};
Vector event;
Vector error;
bool sr1_seen;
bool refuse_reset;
unsigned sr2_reads;
unsigned data_reads;
nx_time_us_t now;

Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == I2C1_EV_IRQn || irq == I2C1_ER_IRQn);
    return irq == I2C1_EV_IRQn ? event : error;
}

class F103Target : public testing::Test {
  protected:
    nx_stm32f1_i2c_target_t target{};
    nx_i2c_target_request_t request{};
    const nx_i2c_target_port_t port{&nx_stm32f1_i2c_target_ops, &target};
    uint8_t rx[2]{};

    void SetUp() override {
        f103_i2c = {};
        f103_i2c_afio = {};
        f103_i2c_rcc = {};
        f103_i2c_rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        f103_i2c_gpio = {};
        f103_i2c_gpio.CRL = UINT32_C(0x44444444);
        f103_i2c_gpio.IDR = wires;
        event = {0U, 0U, 0U, 9U};
        error = {0U, 0U, 0U, 8U};
        sr1_seen = refuse_reset = false;
        sr2_reads = data_reads = 0U;
        now = 10U;
        nx_request_initialize(&request.base);
    }
    void start() {
        ASSERT_EQ(nx_stm32f1_i2c_target_open(&target, 0x52U), NX_SUCCESS);
        ASSERT_EQ(nx_i2c_target_prepare(&port, &request, rx, 2U, nullptr, 0U,
                                        NX_I2C_TARGET_RECEIVE, 100U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_i2c_target_submit(&port, &request), NX_SUCCESS);
        ASSERT_EQ(f103_i2c.CR1, I2C_CR1_PE | I2C_CR1_ACK);
        ASSERT_EQ(f103_i2c.CR2, 8U | irq_sources);
        ASSERT_EQ(event.priority, 0U);
        ASSERT_EQ(error.priority, 0U);
    }
    void pulse(uint32_t status, uint32_t sr2, uint8_t word = 0U) {
        f103_i2c.SR1 = status;
        f103_i2c.SR2 = sr2;
        f103_i2c.DR = word;
        nx_stm32f1_i2c_target_irq(&target);
    }
    void result(nx_result_t expected, size_t count) {
        size_t transferred = 99U;
        nx_result_t actual = NX_SUCCESS;
        ASSERT_EQ(nx_request_result(&request.base, &actual, &transferred),
                  NX_SUCCESS);
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(transferred, count);
    }
};

TEST_F(F103Target, ReceiveStopDrainsSourceAndPreservesExactValidPrefix) {
    ASSERT_NO_FATAL_FAILURE(start());
    pulse(I2C_SR1_ADDR, I2C_SR2_BUSY);
    pulse(I2C_SR1_RXNE, I2C_SR2_BUSY, 0x21U);
    pulse(I2C_SR1_RXNE | I2C_SR1_STOPF, 0U, 0xA2U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(sr2_reads, 1U);
    EXPECT_EQ(data_reads, 2U);
    nx_i2c_target_service(&port);
    result(NX_SUCCESS, 2U);
    EXPECT_EQ(request.rx_received, 2U);
    EXPECT_EQ(request.matched_address, 0x52U);
    EXPECT_EQ(request.address_phases, 1U);
    EXPECT_EQ(rx[0], 0x21U);
    EXPECT_EQ(rx[1], 0xA2U);
    EXPECT_EQ(event.enabled, 0U);
    EXPECT_EQ(error.enabled, 0U);
    EXPECT_EQ(f103_i2c.CR1, I2C_CR1_SWRST);
    EXPECT_EQ(nx_stm32f1_i2c_target_close(&target), NX_SUCCESS);
    EXPECT_EQ(event.priority, 9U);
    EXPECT_EQ(error.priority, 8U);
    EXPECT_EQ(RCC->APB1ENR & RCC_APB1ENR_I2C1EN, 0U);
}

TEST_F(F103Target, ReadAddressAndMissedEv2WindowNeverConsumeData) {
    ASSERT_NO_FATAL_FAILURE(start());
    pulse(I2C_SR1_ADDR | I2C_SR1_TXE, I2C_SR2_BUSY | I2C_SR2_TRA);
    nx_i2c_target_service(&port);
    result(NX_ERROR_IO, 0U);
    EXPECT_EQ(request.faults, NX_I2C_TARGET_PROTOCOL_ERROR);
    EXPECT_EQ(data_reads, 0U);
    ASSERT_EQ(nx_i2c_target_recover(&port), NX_SUCCESS);
    ASSERT_EQ(nx_i2c_target_prepare(&port, &request, rx, 2U, nullptr, 0U,
                                    NX_I2C_TARGET_RECEIVE, 100U),
              NX_SUCCESS);
    ASSERT_EQ(nx_i2c_target_submit(&port, &request), NX_SUCCESS);
    pulse(I2C_SR1_ADDR, I2C_SR2_BUSY);
    pulse(I2C_SR1_RXNE | I2C_SR1_BTF, I2C_SR2_BUSY, 0xACU);
    nx_i2c_target_service(&port);
    result(NX_ERROR_IO, 0U);
    EXPECT_EQ(request.faults, NX_I2C_TARGET_OVERRUN);
    EXPECT_EQ(data_reads, 0U);
}

TEST_F(F103Target, CancelBlocksLateReadsAndRestartRejectsOldReadyToken) {
    ASSERT_NO_FATAL_FAILURE(start());
    pulse(I2C_SR1_ADDR, I2C_SR2_BUSY);
    ASSERT_EQ(nx_i2c_target_cancel(&port, &request), NX_SUCCESS);
    pulse(I2C_SR1_RXNE, I2C_SR2_BUSY, 0x42U);
    EXPECT_EQ(data_reads, 0U);
    nx_i2c_target_service(&port);
    result(NX_ERROR_CANCELLED, 0U);
    ASSERT_EQ(nx_i2c_target_prepare(&port, &request, rx, sizeof(rx), nullptr,
                                    0U, NX_I2C_TARGET_RECEIVE, 100U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_i2c_target_close(&target), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_i2c_target_open(&target, 0x52U), NX_SUCCESS);
    EXPECT_EQ(nx_i2c_target_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(I2C1->CR1, I2C_CR1_SWRST);
    EXPECT_EQ(event.enabled | error.enabled, 0U);
    EXPECT_EQ(nx_stm32f1_i2c_target_close(&target), NX_SUCCESS);
}

TEST_F(F103Target, ClockLossAfterStopDoesNotPublishSuccess) {
    ASSERT_NO_FATAL_FAILURE(start());
    pulse(I2C_SR1_ADDR, I2C_SR2_BUSY);
    pulse(I2C_SR1_RXNE | I2C_SR1_STOPF, 0U, 0x21U);
    f103_i2c_rcc.CFGR = RCC_CFGR_PPRE1_DIV2;
    nx_i2c_target_service(&port);
    result(NX_ERROR_IO, 1U);
}

TEST_F(F103Target, TimeoutRetainsUntilResetAndActualVectorsQuiesce) {
    ASSERT_NO_FATAL_FAILURE(start());
    pulse(I2C_SR1_ADDR, I2C_SR2_BUSY);
    pulse(I2C_SR1_RXNE, I2C_SR2_BUSY, 0x21U);
    now = 100U;
    refuse_reset = true;
    nx_i2c_target_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_stm32f1_i2c_target_close(&target), NX_ERROR_BUSY);
    refuse_reset = false;
    event.running = 1U;
    nx_i2c_target_service(&port);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    event.running = 0U;
    nx_i2c_target_service(&port);
    result(NX_ERROR_TIMEOUT, 1U);
    EXPECT_EQ(nx_stm32f1_i2c_target_close(&target), NX_SUCCESS);
}
} /* namespace */

extern "C" {
I2C_TypeDef f103_i2c{};
RCC_TypeDef f103_i2c_rcc{};
AFIO_TypeDef f103_i2c_afio{};
GPIO_TypeDef f103_i2c_gpio{};

bool nx_stm32f1_system_live(void) {
    return true;
}

nx_time_us_t nx_time_now_us(void) {
    return now;
}

uint32_t f103_i2c_read(const volatile uint32_t* field) {
    const uint32_t value = *field;
    if (field == &f103_i2c.SR1) {
        sr1_seen = true;
    } else if (field == &f103_i2c.SR2 && sr1_seen) {
        ++sr2_reads;
        f103_i2c.SR1 &= ~static_cast<uint32_t>(I2C_SR1_ADDR);
        sr1_seen = false;
    } else if (field == &f103_i2c.DR) {
        ++data_reads;
        f103_i2c.SR1 &= ~static_cast<uint32_t>(I2C_SR1_RXNE | I2C_SR1_BTF);
    }
    return value;
}

void f103_i2c_write(volatile uint32_t* field, uint32_t value) {
    if (field == &f103_i2c.CR1) {
        if ((value & I2C_CR1_SWRST) != 0U) {
            if (refuse_reset) {
                return;
            }
            f103_i2c.CR2 = f103_i2c.SR1 = f103_i2c.SR2 = f103_i2c.OAR1 = 0U;
        }
        if (sr1_seen) {
            f103_i2c.SR1 &= ~static_cast<uint32_t>(I2C_SR1_STOPF);
            sr1_seen = false;
        }
    } else if (field == &f103_i2c_gpio.BSRR) {
        f103_i2c_gpio.ODR =
            (f103_i2c_gpio.ODR & ~(value >> 16U)) | (value & 0xFFFFU);
    }
    *field = value;
}

uint32_t nx_test_target_enabled(IRQn_Type irq) {
    return vector(irq).enabled;
}
uint32_t nx_test_target_pending(IRQn_Type irq) {
    return vector(irq).pending;
}
uint32_t nx_test_target_active(IRQn_Type irq) {
    return vector(irq).running;
}
uint32_t nx_test_target_priority(IRQn_Type irq) {
    return vector(irq).priority;
}
void nx_test_target_enable(IRQn_Type irq) {
    vector(irq).enabled = 1U;
}
void nx_test_target_disable(IRQn_Type irq) {
    vector(irq).enabled = 0U;
}
void nx_test_target_ack(IRQn_Type irq) {
    vector(irq).pending = 0U;
}
void nx_test_target_set_priority(IRQn_Type irq, uint32_t value) {
    vector(irq).priority = value;
}
}
