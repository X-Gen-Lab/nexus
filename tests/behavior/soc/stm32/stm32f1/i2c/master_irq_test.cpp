/**
 * \file            master_irq_test.cpp
 * \brief           F103 controlled I2C master START retirement
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/i2c/master_irq.h"
#include "system/platform.h"
#include <gtest/gtest.h>

namespace {
constexpr unsigned cr1 = 0U;
constexpr unsigned cr2 = 1U;
constexpr unsigned data = 4U;
constexpr unsigned sr1 = 5U;
constexpr unsigned sr2 = 6U;
constexpr unsigned ccr = 7U;
constexpr uint32_t start_bit = 1U << 8U;
constexpr uint32_t stop_bit = 1U << 9U;
constexpr uint32_t swrst_bit = 1U << 15U;
constexpr uint32_t sb = 1U;
constexpr uint32_t addr = 1U << 1U;
constexpr uint32_t btf = 1U << 2U;
constexpr uint32_t txe = 1U << 7U;
constexpr uint32_t berr = 1U << 8U;
constexpr uint32_t arlo = 1U << 9U;
constexpr uint32_t af = 1U << 10U;
constexpr uint32_t master_busy = 3U;
constexpr uint32_t wires = GPIO_IDR_IDR6 | GPIO_IDR_IDR7;
nx_time_us_t now;
bool retirement_blocked;
unsigned retirements;
struct Vector {
    uint32_t enabled, pending, active, priority;
};
Vector event;
Vector error;
Vector& vector(IRQn_Type irq) {
    EXPECT_TRUE(irq == I2C1_EV_IRQn || irq == I2C1_ER_IRQn);
    return irq == I2C1_EV_IRQn ? event : error;
}

class F103MasterIrq : public testing::Test {
  protected:
    volatile uint32_t* registers = &f103_i2c.CR1;
    nx_stm32f1_i2c_irq_cold_t cold{};
    nx_i2c_v1_state_t state{};
    nx_i2c_v1_endpoint_t target{&state, 0x52U, 7U};
    const nx_i2c_port_t bus{&nx_i2c_v1_async_ops, &state};
    const nx_i2c_endpoint_t endpoint{&nx_i2c_v1_async_endpoint_ops, &target};
    nx_i2c_request_t request{};
    const uint8_t payload[2]{11U, 29U};
    uint8_t input[1]{0xccU};
    nx_i2c_message_t messages[2]{nx_i2c_message_write(payload, 1U),
                                 nx_i2c_message_write(payload + 1U, 1U)};

    void SetUp() override {
        now = 10U;
        retirements = 0U;
        retirement_blocked = false;
        f103_i2c = {};
        f103_i2c_afio = {};
        f103_i2c_rcc = {};
        f103_i2c_rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        f103_i2c_gpio = {};
        f103_i2c_gpio.CRL = UINT32_C(0x44444444);
        f103_i2c_gpio.IDR = wires;
        event = {0U, 0U, 0U, 9U};
        error = {0U, 0U, 0U, 8U};
        ASSERT_EQ(NX_SUCCESS, nx_stm32f1_i2c_irq_open(&state, &cold));
        nx_request_initialize(&request.base);
    }
    void submit(size_t count) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_i2c_request_prepare(&request, messages, count, 100U));
        ASSERT_EQ(NX_SUCCESS, nx_i2c_endpoint_submit(&endpoint, &request));
        ASSERT_NE(0U, registers[cr1] & start_bit);
    }
    void irq(uint32_t flags, uint32_t bus_owner = master_busy) {
        registers[sr1] = flags;
        registers[sr2] = bus_owner;
        nx_i2c_v1_irq(&state);
    }
    void result(nx_result_t expected, size_t bytes) {
        nx_result_t code = NX_SUCCESS;
        size_t transferred = 99U;
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &code, &transferred));
        EXPECT_EQ(expected, code);
        EXPECT_EQ(bytes, transferred);
    }
};
} /* namespace */

extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}

extern "C" {
I2C_TypeDef f103_i2c{};
RCC_TypeDef f103_i2c_rcc{};
AFIO_TypeDef f103_i2c_afio{};
GPIO_TypeDef f103_i2c_gpio{};

bool nx_stm32f1_system_live(void) {
    return true;
}

uint32_t f103_i2c_read(const volatile uint32_t* field) {
    return *field;
}

void f103_i2c_write(volatile uint32_t* field, uint32_t value) {
    if (field == &f103_i2c.CR1 && value == I2C_CR1_SWRST) {
        ++retirements;
        if (retirement_blocked) {
            return;
        }
        f103_i2c.SR1 = 0U;
        f103_i2c.SR2 &= I2C_SR2_BUSY;
        f103_i2c.CR2 = 0U;
    }
    if (field == &f103_i2c_gpio.BSRR) {
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
    return vector(irq).active;
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
void nx_test_target_set_priority(IRQn_Type irq, uint32_t priority) {
    vector(irq).priority = priority;
}
}

TEST_F(F103MasterIrq, PendingStartTimeoutReleasesLoanEvenWhilePeerHoldsBus) {
    ASSERT_NO_FATAL_FAILURE(submit(1U));
    now = 100U;
    f103_i2c_gpio.IDR = 0U;
    registers[sr2] = 2U;
    nx_i2c_port_service(&bus);
    ASSERT_NO_FATAL_FAILURE(result(NX_ERROR_TIMEOUT, 0U));
    EXPECT_EQ(1U, retirements);
    EXPECT_EQ(swrst_bit, registers[cr1]);
    EXPECT_EQ(2U, registers[sr2]);
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_port_recover(&bus));
    f103_i2c_gpio.IDR = wires;
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_port_recover(&bus));
    f103_i2c_gpio.IDR = GPIO_IDR_IDR7;
    irq(0U, 0U);
    EXPECT_EQ(NX_ERROR_IO, nx_i2c_port_recover(&bus));
    f103_i2c_gpio.IDR = wires;
    EXPECT_EQ(NX_SUCCESS, nx_i2c_port_recover(&bus));
    EXPECT_EQ(50U, registers[ccr]);
}

TEST_F(F103MasterIrq, BusErrorWithStartPendingDoesNotAttemptStop) {
    ASSERT_NO_FATAL_FAILURE(submit(1U));
    irq(berr);
    EXPECT_EQ(0U, registers[cr1] & stop_bit);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    EXPECT_EQ(0U, retirements);
    nx_i2c_port_service(&bus);
    ASSERT_NO_FATAL_FAILURE(result(NX_ERROR_IO, 0U));
    EXPECT_EQ(1U, retirements);
}

TEST_F(F103MasterIrq, FailedResetRetainsLoanAndLateIrqCannotReadBuffer) {
    ASSERT_NO_FATAL_FAILURE(submit(1U));
    retirement_blocked = true;
    now = 100U;
    nx_i2c_port_service(&bus);
    ASSERT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    irq(sb | txe | btf);
    EXPECT_EQ(0U, registers[data]);
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_port_stop(&bus));
    retirement_blocked = false;
    event.active = 1U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_i2c_port_stop(&bus));
    event.active = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_i2c_irq_close(&state, &cold));
    EXPECT_EQ(event.priority, 9U);
    EXPECT_EQ(error.priority, 8U);
    EXPECT_EQ(event.enabled | error.enabled, 0U);
    EXPECT_EQ(0U, f103_i2c_rcc.APB1ENR & RCC_APB1ENR_I2C1EN);
    ASSERT_NO_FATAL_FAILURE(result(NX_ERROR_TIMEOUT, 0U));
}

TEST_F(F103MasterIrq, ReceiveRejectedBeforeAccessToControllerOrDescriptor) {
    messages[1] = nx_i2c_message_read(input, sizeof(input));
    ASSERT_EQ(NX_SUCCESS, nx_i2c_request_prepare(&request, messages, 2U, 100U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_i2c_endpoint_submit(&endpoint, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(1U, registers[cr1]);
    EXPECT_EQ(8U, registers[cr2]);
    EXPECT_EQ(0xccU, input[0]);
}

TEST_F(F103MasterIrq, RepeatedStartCountsAcknowledgedPrefixAndDrainsStop) {
    ASSERT_NO_FATAL_FAILURE(submit(2U));
    irq(sb);
    EXPECT_EQ(0xa4U, registers[data]);
    irq(addr);
    irq(txe);
    EXPECT_EQ(11U, registers[data]);
    irq(btf);
    EXPECT_NE(0U, registers[cr1] & start_bit);
    EXPECT_EQ(0U, registers[cr1] & stop_bit);
    irq(sb);
    irq(addr);
    irq(txe);
    EXPECT_EQ(29U, registers[data]);
    irq(btf);
    EXPECT_NE(0U, registers[cr1] & stop_bit);
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    registers[cr1] &= ~stop_bit;
    irq(0U, 0U);
    nx_i2c_port_service(&bus);
    ASSERT_NO_FATAL_FAILURE(result(NX_SUCCESS, 2U));
    EXPECT_EQ(0U, retirements);
}

TEST_F(F103MasterIrq, ArbitrationLossNeverInjectsStopIntoWire) {
    ASSERT_NO_FATAL_FAILURE(submit(1U));
    registers[cr1] &= ~start_bit;
    irq(arlo, 2U);
    EXPECT_EQ(0U, registers[cr1] & stop_bit);
    nx_i2c_port_service(&bus);
    ASSERT_NO_FATAL_FAILURE(result(NX_ERROR_ARBITRATION, 0U));
    EXPECT_EQ(0U, retirements);
}

TEST_F(F103MasterIrq, NackPreservesAcknowledgedPrefixOfTwoMessages) {
    ASSERT_NO_FATAL_FAILURE(submit(2U));
    irq(sb);
    irq(addr);
    irq(txe);
    irq(btf);
    irq(sb);
    irq(af);
    EXPECT_NE(0U, registers[cr1] & stop_bit);
    registers[cr1] &= ~stop_bit;
    irq(0U, 0U);
    nx_i2c_port_service(&bus);
    ASSERT_NO_FATAL_FAILURE(result(NX_ERROR_NACK, 1U));
    EXPECT_EQ(0U, retirements);
}

TEST_F(F103MasterIrq, StuckStopCannotPublishSuccessfulTransfer) {
    ASSERT_NO_FATAL_FAILURE(submit(1U));
    irq(sb);
    irq(addr);
    irq(txe);
    irq(btf);
    EXPECT_NE(0U, registers[cr1] & stop_bit);
    now = 1010U;
    nx_i2c_port_service(&bus);
    ASSERT_NO_FATAL_FAILURE(result(NX_ERROR_IO, 1U));
    EXPECT_EQ(1U, retirements);
}
