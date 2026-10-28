/**
 * \file            gd32_i2c_cold_test.cpp
 * \brief           Concrete I2C publisher ownership and retained rollback
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_i2c_cold.h"
#include "nexus/arch/arch.h"
NVIC_Type g_gd32_model_nvic;
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
bool in_isr;
bool masked;
bool privileged;
nx_time_us_t model_time;
volatile uint32_t* refused;
uint32_t refused_bits;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* writes;

class Gd32I2cCold : public testing::Test {
  protected:
    void* mapping = MAP_FAILED;
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        observer;
    std::array<nx_gd32_cold_pin_config_t, 2> pins{{
        {GPIOB, 6U, 4U, 2U, 1U, true},
        {GPIOB, 7U, 4U, 2U, 1U, true},
    }};
    std::array<uint16_t, 2> saved{};
    nx_gd32_cold_pins_receipt_t receipt{};

    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        g_gd32_model_nvic = {};
        in_isr = false;
        masked = false;
        privileged = true;
        model_time = 0U;
        refused = nullptr;
        refused_bits = 0U;
        writes = &observer;
    }

    void TearDown() override {
        writes = nullptr;
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
    }

    nx_gd32_i2c_config_t config{I2C0,  pins.data(), saved.data(), nullptr,
                                0x50U, 5U,          true};
    nx_gd32_i2c_cold_t cold{};

    nx_result_t Acquire() {
        return nx_gd32_i2c_cold_acquire(&config, &cold);
    }

    nx_result_t Release() {
        return nx_gd32_i2c_cold_release(&config, &cold);
    }
};
} /* namespace */

/** \brief Model the external monotonic clock for retained driver faces. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return ++model_time;
}

/** \brief Inject only external MMIO refusal; execute the real C algorithm. */
extern "C" void nx_gd32_cold_model_write(volatile uint32_t* reg,
                                         uint32_t value) {
    if (writes != nullptr)
        writes->Call(reg, value);
    if (reg != refused || ((value ^ *reg) & refused_bits) == 0U)
        *reg = value;
}
/** \brief Model CPU context independently of the production transaction. */
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
/** \brief Observe the incoming interrupt mask without changing it. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
/** \brief Observe privilege before cold hardware acquisition. */
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/** \brief Host ordering is not physical bus timing. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief The model does not execute a physical instruction pipeline. */
extern "C" void nx_arch_isb(void) {
}

/** \brief Model the SDK reset line, leaving the C driver under test intact. */
extern "C" void i2c_deinit(uint32_t registers) {
    std::memset(reinterpret_cast<void*>(static_cast<uintptr_t>(registers)), 0,
                0x28U);
}
/** \brief Model the SDK gate release at the external MMIO boundary. */
extern "C" void rcu_periph_clock_disable(rcu_periph_enum clock) {
    RCU_REG_VAL(clock) &= ~BIT(RCU_BIT_POS(clock));
}

TEST_F(Gd32I2cCold, AcquiresExactClockAndPadsThenRestoresOriginals) {
    GPIO_PUD(GPIOB) = 2U << 12U;
    const uint16_t before = nx_gd32_pin_capture(GPIOB, 6U);
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    EXPECT_NE(RCU_APB1EN & RCU_APB1EN_I2C0EN, 0U);
    EXPECT_EQ(cold.pins.captured, 3U);
    ASSERT_EQ(Release(), NX_SUCCESS);
    EXPECT_EQ(nx_gd32_pin_capture(GPIOB, 6U), before);
    EXPECT_EQ(RCU_APB1EN & RCU_APB1EN_I2C0EN, 0U);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_FALSE(cold.owned);
}
TEST_F(Gd32I2cCold, ForeignPublisherRejectsBeforeClockOrPinEffects) {
    NVIC->ISPR[0] = 1U << 31U;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Acquire(), NX_ERROR_BUSY);
    EXPECT_EQ(RCU_APB1EN, 0U);
    EXPECT_EQ(RCU_AHB1EN, 0U);
    EXPECT_FALSE(cold.owned);
}
TEST_F(Gd32I2cCold, InvalidLineTupleCannotAcquireClock) {
    pins[1].pin = 10U;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    EXPECT_EQ(Acquire(), NX_ERROR_INVALID);
    EXPECT_EQ(RCU_APB1EN, 0U);
}
TEST_F(Gd32I2cCold, ClockWriteFailureCanBeRetriedWithoutLostOwnership) {
    refused = &RCU_APB1EN;
    refused_bits = RCU_APB1EN_I2C0EN;
    EXPECT_EQ(Acquire(), NX_ERROR_IO);
    EXPECT_FALSE(cold.owned);
    refused = nullptr;
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    EXPECT_EQ(Release(), NX_SUCCESS);
}
TEST_F(Gd32I2cCold, RestoresPrioritiesAfterObservedPublisherWithdrawal) {
    NVIC->IPR[31] = 0xA0U;
    NVIC->IPR[32] = 0xB0U;
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    NVIC->IPR[31] = 0x50U;
    NVIC->IPR[32] = 0x50U;
    EXPECT_EQ(Release(), NX_SUCCESS);
    EXPECT_EQ(NVIC->IPR[31], 0xA0U);
    EXPECT_EQ(NVIC->IPR[32], 0xB0U);
}
TEST_F(Gd32I2cCold, FailedPublisherDrainRetainsPinsAndControllerClock) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    NVIC->IABR[0] = 1U << 31U;
    EXPECT_EQ(Release(), NX_ERROR_BUSY);
    EXPECT_TRUE(cold.owned);
    EXPECT_EQ(cold.pins.captured, 3U);
    EXPECT_NE(RCU_APB1EN & RCU_APB1EN_I2C0EN, 0U);
    NVIC->IABR[0] = 0U;
    EXPECT_EQ(Release(), NX_SUCCESS);
}
TEST_F(Gd32I2cCold, FailedPinRollbackRetainsReceiptForExplicitRetry) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    refused = &GPIO_CTL(GPIOB);
    refused_bits = (3U << 12U) | (3U << 14U);
    EXPECT_EQ(Release(), NX_ERROR_IO);
    EXPECT_TRUE(cold.owned);
    EXPECT_NE(cold.pins.captured, 0U);
    EXPECT_NE(RCU_AHB1EN & RCU_AHB1EN_PBEN, 0U);
    refused = nullptr;
    EXPECT_EQ(Release(), NX_SUCCESS);
}
TEST_F(Gd32I2cCold, CommitRejectsMissingPublisherWithoutReleasingEngineOwner) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    cold.prepared = true;
    EXPECT_EQ(nx_gd32_i2c_cold_commit(&config, &cold), NX_ERROR_IO);
    EXPECT_TRUE(cold.prepared);
    EXPECT_TRUE(cold.owned);
    cold.prepared = false;
    EXPECT_EQ(Release(), NX_SUCCESS);
}
TEST_F(Gd32I2cCold, CommitVerifiesEncodedPriorityAndActualEnabledVectors) {
    ASSERT_EQ(Acquire(), NX_SUCCESS);
    cold.prepared = true;
    I2C_CTL0(I2C0) = I2C_CTL0_I2CEN;
    NVIC->ISER[0] = 1U << 31U;
    NVIC->ISER[1] = 1U;
    EXPECT_EQ(nx_gd32_i2c_cold_commit(&config, &cold), NX_SUCCESS);
    EXPECT_EQ(NVIC->IPR[31], 0x50U);
    EXPECT_EQ(NVIC->IPR[32], 0x50U);
    NVIC->ISER[0] = 0U;
    NVIC->ISER[1] = 0U;
    cold.prepared = false;
    EXPECT_EQ(Release(), NX_SUCCESS);
}

TEST_F(Gd32I2cCold, PollingPrepareAndCloseExecuteRealControllerLifecycle) {
    config.asynchronous = false;
    nx_gd32_i2c_state_t port{};
    nx_gd32_i2c_endpoint_state_t endpoint{};
    ASSERT_EQ(nx_gd32_i2c_prepare(&port, &endpoint, &config, &cold),
              NX_SUCCESS);
    EXPECT_TRUE(port.initialized);
    EXPECT_TRUE(cold.prepared);
    EXPECT_EQ(port.controller, &nx_gd32_i2c0_controller);
    EXPECT_EQ(endpoint.port, &port);
    EXPECT_EQ(endpoint.address, 0x50U);
    EXPECT_EQ(I2C_CKCFG(I2C0), 250U);
    EXPECT_EQ(I2C_CTL0(I2C0), I2C_CTL0_I2CEN | I2C_CTL0_ACKEN);
    ASSERT_EQ(nx_gd32_i2c_close(&port, &config, &cold), NX_SUCCESS);
    EXPECT_FALSE(port.initialized);
    EXPECT_EQ(port.controller, nullptr);
    EXPECT_FALSE(cold.owned);
    EXPECT_EQ(RCU_APB1EN & RCU_APB1EN_I2C0EN, 0U);
}

TEST_F(Gd32I2cCold, ActivePollingOwnerCannotReleaseBorrowedHardware) {
    config.asynchronous = false;
    nx_gd32_i2c_state_t port{};
    nx_gd32_i2c_endpoint_state_t endpoint{};
    ASSERT_EQ(nx_gd32_i2c_prepare(&port, &endpoint, &config, &cold),
              NX_SUCCESS);
    port.active = true;
    EXPECT_EQ(nx_gd32_i2c_close(&port, &config, &cold), NX_ERROR_BUSY);
    EXPECT_TRUE(cold.prepared);
    EXPECT_TRUE(port.initialized);
    EXPECT_NE(RCU_APB1EN & RCU_APB1EN_I2C0EN, 0U);
    port.active = false;
    EXPECT_EQ(nx_gd32_i2c_close(&port, &config, &cold), NX_SUCCESS);
}
