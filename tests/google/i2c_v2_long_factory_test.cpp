/**
 * \file            i2c_v2_long_factory_test.cpp
 * \brief           Long-message handshakes through actual normal TOML factories
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v2_long_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#if defined(NX_I2C_LONG_G474)
#define SELECTED_BUS    NX_I2C_ID_BUS
#define SELECTED_DEVICE NX_I2C_LONG_DEVICE_ID_SENSOR_REGISTERS
#elif defined(NX_I2C_LONG_H743)
#define SELECTED_BUS    NX_I2C_ID_BUS1
#define SELECTED_DEVICE NX_I2C_LONG_DEVICE_ID_DEVICE1
#else
#define SELECTED_BUS    NX_I2C_ID_CONTROL
#define SELECTED_DEVICE NX_I2C_LONG_DEVICE_ID_SENSOR
#endif
namespace {
testing::MockFunction<void(nx_i2c_long_factory_event_t, bool)>* observer;
void observed(nx_i2c_long_factory_event_t event, bool read) {
    if (observer != nullptr)
        observer->Call(event, read);
}
class I2CLongNormalFactory : public testing::Test {
  protected:
    std::array<uint8_t, 511> tx{};
    std::array<uint8_t, 257> rx{};
    nx_i2c_message_t messages[2]{};
    const nx_i2c_port_t* bus{};
    const nx_i2c_long_endpoint_t* device{};
    nx_i2c_v2_state_t* state{};
    void SetUp() override {
        nx_i2c_long_factory_reset();
        observer = nullptr;
        for (size_t i = 0U; i < tx.size(); ++i)
            tx[i] = static_cast<uint8_t>(i ^ 0xA5U);
        rx.fill(0xEEU);
        messages[0] = {tx.data(), tx.size(), false};
        messages[1] = {rx.data(), rx.size(), true};
        bus = nx_factory_i2c(SELECTED_BUS);
        device = nx_factory_i2c_long_device(SELECTED_DEVICE);
        ASSERT_NE(bus, nullptr);
        ASSERT_NE(device, nullptr);
        EXPECT_EQ(0U, g_long_i2c.addresses);
        EXPECT_TRUE(g_long_i2c.controls.empty());
        EXPECT_FALSE(g_long_i2c.running);
        auto started = nx_platform_start();
        ASSERT_EQ(started.primary, NX_SUCCESS);
        ASSERT_EQ(started.cleanup, NX_SUCCESS);
        state = static_cast<nx_i2c_v2_state_t*>(bus->context);
        ASSERT_NE(state, nullptr);
        ASSERT_EQ(state->registers, g_long_i2c.registers);
        ASSERT_TRUE(state->initialized);
        ASSERT_TRUE(state->retained);
        EXPECT_EQ(state->timing, NX_I2C_LONG_EXPECTED_TIMING);
        EXPECT_EQ(state->registers->timing, NX_I2C_LONG_EXPECTED_TIMING);
#if defined(NX_I2C_LONG_G474)
        EXPECT_EQ(RCC->CCIPR & RCC_CCIPR_I2C1SEL, RCC_CCIPR_I2C1SEL_1);
#elif defined(NX_I2C_LONG_H743)
        EXPECT_EQ(RCC->D2CCIP2R & RCC_D2CCIP2R_I2C123SEL, 0U);
#else
        EXPECT_EQ(RCC->CCIPR1 & RCC_CCIPR1_I2C1SEL, RCC_CCIPR1_I2C1SEL_1);
        EXPECT_NE(RCC->CR & RCC_CR_HSIRDY, 0U);
#endif
    }
    void TearDown() override {
        g_long_i2c.refuse_next_reload = false;
        g_long_i2c.refuse_stop_clear = false;
        g_long_i2c.refuse_control_clear = false;
        g_long_i2c.tcr_delay_polls = 0U;
        g_long_i2c.event = nullptr;
        observer = nullptr;
        if (nx_platform_stop() != NX_SUCCESS ||
            (state != nullptr && (state->active || state->retained))) {
            /* Do not destroy buffers or the external register objects while
             * failed cleanup still retains the actual provider owner. */
            std::abort();
        }
    }
    nx_result_t Transfer(size_t* count, nx_time_us_t lifetime = 10000U) {
        return nx_i2c_long_endpoint_transaction(
            device, messages, 2U, nx_deadline_after(nx_time_now_us(), lifetime),
            count);
    }
    void CheckPayload() {
        ASSERT_GE(g_long_i2c.transmitted.size(), tx.size());
        EXPECT_TRUE(
            std::equal(tx.begin(), tx.end(),
                       g_long_i2c.transmitted.data() +
                           (g_long_i2c.transmitted.size() - tx.size())));
        for (size_t i = 0U; i < rx.size(); ++i)
            EXPECT_EQ(rx[i], static_cast<uint8_t>(i * 37U + 0x5AU));
    }
    void CheckRecoveryThenFreshTransfer() {
        const unsigned before = g_long_i2c.addresses;
        size_t rejected = 99U;
        EXPECT_EQ(Transfer(&rejected), NX_ERROR_STATE);
        EXPECT_EQ(rejected, 0U);
        EXPECT_EQ(g_long_i2c.addresses, before);
        g_long_i2c.refuse_stop_clear = false;
        g_long_i2c.refuse_control_clear = false;
        ASSERT_EQ(nx_i2c_port_recover(bus), NX_SUCCESS);
        EXPECT_FALSE(state->fault);
        EXPECT_EQ(state->registers->timing, NX_I2C_LONG_EXPECTED_TIMING);
        size_t done = 0U;
        ASSERT_EQ(Transfer(&done), NX_SUCCESS);
        EXPECT_EQ(done, tx.size() + rx.size());
        EXPECT_EQ(g_long_i2c.addresses, before + 2U);
        EXPECT_EQ(g_long_i2c.illegal_controls, 0U);
        CheckPayload();
    }
};
TEST_F(I2CLongNormalFactory, LongWriteReadPreservesMessageBoundariesAndTiming) {
    testing::StrictMock<
        testing::MockFunction<void(nx_i2c_long_factory_event_t, bool)>>
        calls;
    observer = &calls;
    g_long_i2c.event = observed;
    EXPECT_CALL(calls, Call(NX_LONG_ADDRESS, false)).Times(1);
    EXPECT_CALL(calls, Call(NX_LONG_ADDRESS, true)).Times(1);
    EXPECT_CALL(calls, Call(NX_LONG_STOP, false)).Times(1);
    /* Each TCR is independently delayed. CR2 writes cannot themselves invent
     * a reload boundary, and do not clear the sticky physical STOP flag. */
    g_long_i2c.tcr_delay_polls = 3U;
    size_t done = 0U;
    ASSERT_EQ(Transfer(&done), NX_SUCCESS);
    EXPECT_EQ(done, 768U);
    EXPECT_EQ(g_long_i2c.addresses, 2U);
    EXPECT_EQ(g_long_i2c.stops, 1U);
    EXPECT_EQ(g_long_i2c.resets, 0U);
    EXPECT_EQ(g_long_i2c.illegal_controls, 0U);
    const size_t rx_chunks = state->receive_reload_one ? rx.size() : 2U;
    EXPECT_EQ(g_long_i2c.reloads, 2U + rx_chunks - 1U);
    ASSERT_EQ(g_long_i2c.controls.size(), 3U + rx_chunks);
    for (size_t i = 0U; i < g_long_i2c.controls.size(); ++i) {
        const auto entry = g_long_i2c.controls[i];
        ASSERT_TRUE(entry.accepted);
        EXPECT_EQ((entry.value & I2C_CR2_START) != 0U, i == 0U || i == 3U);
        EXPECT_EQ((entry.value & I2C_CR2_AUTOEND) != 0U,
                  i + 1U == g_long_i2c.controls.size());
        EXPECT_EQ((entry.value & I2C_CR2_RD_WRN) != 0U, i >= 3U);
        const auto* endpoint =
            static_cast<const nx_i2c_v2_endpoint_state_t*>(device->context);
        ASSERT_EQ(endpoint->port, state);
        EXPECT_EQ(entry.value & (uint32_t)I2C_CR2_SADD,
                  static_cast<uint32_t>(endpoint->address) << 1U);
        if (i >= 3U && state->receive_reload_one) {
            EXPECT_EQ((entry.value >> 16U) & 255U, 1U);
        }
    }
    EXPECT_EQ(state->registers->cr2, 0U);
    EXPECT_EQ(state->registers->isr & (I2C_ISR_BUSY | I2C_ISR_STOPF), 0U);
    CheckPayload();
    g_long_i2c.event = nullptr;
    observer = nullptr;
}
TEST_F(I2CLongNormalFactory,
       RefusedFreshTcrReloadNeverReplaysOrRestartsPrefix) {
    g_long_i2c.tcr_delay_polls = 5U;
    g_long_i2c.refuse_next_reload = true;
    size_t done = 0U;
    ASSERT_EQ(Transfer(&done, 2000U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(done, 255U);
    EXPECT_EQ(g_long_i2c.transmitted.size(), 255U);
    EXPECT_EQ(g_long_i2c.addresses, 1U);
    EXPECT_EQ(g_long_i2c.stops, 0U);
    EXPECT_EQ(g_long_i2c.refused_reloads, 1U);
    EXPECT_EQ(g_long_i2c.resets, 1U);
    EXPECT_EQ(g_long_i2c.illegal_controls, 0U);
    EXPECT_TRUE(state->fault);
    EXPECT_FALSE(state->active);
    CheckRecoveryThenFreshTransfer();
}
TEST_F(I2CLongNormalFactory, RefusedStopFlagClearCannotReportReusableSuccess) {
    g_long_i2c.refuse_stop_clear = true;
    size_t done = 0U;
    EXPECT_EQ(Transfer(&done), NX_ERROR_IO);
    EXPECT_EQ(done, 768U);
    EXPECT_EQ(g_long_i2c.stops, 1U);
    EXPECT_TRUE(state->fault);
    EXPECT_FALSE(state->active);
    EXPECT_TRUE(state->retained);
    /* All bytes are register progress. Physical STOP/BUSY proof already
     * allows caller-buffer return; a cleanup failure must not fake reuse. */
    CheckRecoveryThenFreshTransfer();
}
TEST_F(I2CLongNormalFactory, RefusedControlClearCannotReportReusableSuccess) {
    g_long_i2c.refuse_control_clear = true;
    size_t done = 0U;
    EXPECT_EQ(Transfer(&done), NX_ERROR_IO);
    EXPECT_EQ(done, 768U);
    EXPECT_EQ(g_long_i2c.stops, 1U);
    EXPECT_TRUE(state->fault);
    EXPECT_FALSE(state->active);
    EXPECT_TRUE(state->retained);
    CheckRecoveryThenFreshTransfer();
}
} /* namespace */
