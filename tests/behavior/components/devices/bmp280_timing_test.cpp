/**
 * \file            bmp280_timing_test.cpp
 * \brief           Forced-conversion finite timing and settled-port contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bmp280.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class SensorPort {
  public:
    MOCK_METHOD(nx_result_t, Exchange,
                (const uint8_t*, uint8_t*, size_t, nx_time_us_t));
    nx_time_us_t now = 100U;
};

nx_result_t Exchange(void* context, const uint8_t* tx, uint8_t* rx,
                     size_t bytes, nx_time_us_t deadline) {
    return static_cast<SensorPort*>(context)->Exchange(tx, rx, bytes, deadline);
}

nx_time_us_t ReadClock(void* context) {
    return static_cast<SensorPort*>(context)->now;
}

class Bmp280Timing : public testing::Test {
  protected:
    testing::StrictMock<SensorPort> port;
    nx_bmp280_t device{};
    void SetUp() override {
        ON_CALL(port, Exchange(testing::_, testing::_, testing::_, testing::_))
            .WillByDefault(testing::Invoke(
                [](const uint8_t*, uint8_t* rx, size_t bytes, nx_time_us_t) {
                    if (rx != nullptr)
                        std::memset(rx, 0, bytes);
                    return NX_SUCCESS;
                }));
        device.initialized = true;
        device.transport = {&port, Exchange, {ReadClock, &port}};
    }
};

TEST_F(Bmp280Timing, UnrepresentableReadyTimeRejectsBeforeTransportEffects) {
    port.now = UINT64_MAX - 7999U;
    EXPECT_CALL(port, Exchange(testing::_, testing::_, testing::_, testing::_))
        .Times(0);
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_bmp280_begin(&device, NX_DEADLINE_NEVER));
    EXPECT_FALSE(device.measuring);
}

TEST_F(Bmp280Timing, ExhaustionDuringConfigurationRetainsNoSoftwareConversion) {
    testing::InSequence sequence;
    EXPECT_CALL(port,
                Exchange(testing::_, testing::NotNull(), 2U, NX_DEADLINE_NEVER))
        .WillOnce(testing::Invoke(
            [](const uint8_t* tx, uint8_t* rx, size_t, nx_time_us_t) {
                EXPECT_EQ(0xf3U, tx[0]);
                rx[1] = 0U;
                return NX_SUCCESS;
            }));
    EXPECT_CALL(port,
                Exchange(testing::_, testing::IsNull(), 2U, NX_DEADLINE_NEVER))
        .WillOnce(testing::Invoke(
            [this](const uint8_t* tx, uint8_t*, size_t, nx_time_us_t) {
                EXPECT_EQ(0x74U, tx[0]);
                EXPECT_EQ(0x25U, tx[1]);
                port.now = UINT64_MAX - 7999U;
                return NX_SUCCESS;
            }));
    EXPECT_EQ(NX_ERROR_EXHAUSTED, nx_bmp280_begin(&device, NX_DEADLINE_NEVER));
    EXPECT_FALSE(device.measuring);
    nx_bmp280_sample_t sample{123, 456U};
    EXPECT_EQ(NX_ERROR_STATE, nx_bmp280_poll(&device, &sample));
    EXPECT_EQ(123, sample.centi_celsius);
    EXPECT_EQ(456U, sample.pressure_pa);
}
} /* namespace */
