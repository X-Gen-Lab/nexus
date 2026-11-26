/**
 * \file            wm8978_test.cpp
 * \brief           Actual WM8978 playback control and uncertain-state exit
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/wm8978.h"
#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {
/** \brief Only completed I2C writes and monotonic clock facts are modeled. */
class CodecBus {
  public:
    nx_time_us_t now{100U};
    nx_time_us_t last_deadline{};
    size_t fail_at{SIZE_MAX};
    unsigned clock_reads{};
    unsigned expire_read{UINT32_MAX};
    nx_time_us_t expire_at{};
    bool short_write{};
    std::vector<std::pair<uint8_t, uint16_t>> registers;
    static nx_time_us_t clock(void* context) {
        auto& self = *static_cast<CodecBus*>(context);
        ++self.clock_reads;
        if (self.clock_reads == self.expire_read)
            self.now = self.expire_at;
        return self.now;
    }
    static nx_result_t transfer(void* context, nx_i2c_message_t* messages,
                                size_t count, nx_time_us_t deadline,
                                size_t* done) {
        auto& self = *static_cast<CodecBus*>(context);
        if (count != 1U || messages[0].read || messages[0].length != 2U)
            return NX_ERROR_IO;
        const uint8_t* bytes = messages[0].buffer.tx;
        const uint16_t value = static_cast<uint16_t>(
            (static_cast<uint16_t>(bytes[0] & 1U) << 8U) | bytes[1]);
        self.registers.emplace_back(static_cast<uint8_t>(bytes[0] >> 1U),
                                    value);
        self.last_deadline = deadline;
        *done = self.short_write ? 1U : 2U;
        return self.registers.size() == self.fail_at ? NX_ERROR_IO : NX_SUCCESS;
    }
    const nx_i2c_endpoint_ops_t ops{transfer, nullptr, nullptr, nullptr};
    const nx_i2c_endpoint_t endpoint{&ops, this};
};
class WM8978 : public testing::Test {
  protected:
    CodecBus bus;
    nx_wm8978_config_t config{
        &bus.endpoint, {CodecBus::clock, &bus}, 1000U, 12288000U, 1000U, 16U};
    nx_wm8978_t device{};
    void ready() {
        ASSERT_EQ(nx_wm8978_initialize(&device, &config, 1000000U), NX_SUCCESS);
        bus.now += config.vmid_settle_us;
        ASSERT_EQ(nx_wm8978_service(&device), NX_SUCCESS);
    }
};
TEST_F(WM8978, BiasSettlesCooperativelyBeforeEnablingMutedOut1) {
    ASSERT_EQ(nx_wm8978_initialize(&device, &config, 1000000U), NX_SUCCESS);
    ASSERT_FALSE(bus.registers.empty());
    EXPECT_EQ(bus.registers.front(), (std::pair<uint8_t, uint16_t>{0U, 0U}));
    EXPECT_EQ(bus.registers.back(), (std::pair<uint8_t, uint16_t>{1U, 7U}));
    const size_t before = bus.registers.size();
    EXPECT_EQ(nx_wm8978_service(&device), NX_ERROR_BUSY);
    EXPECT_EQ(bus.registers.size(), before);
    bus.now = 1100U;
    ASSERT_EQ(nx_wm8978_service(&device), NX_SUCCESS);
    EXPECT_EQ(device.phase, NX_WM8978_READY);
    EXPECT_TRUE(device.muted);
    EXPECT_EQ(bus.last_deadline, 1000000U);
    EXPECT_NE(std::find(bus.registers.begin(), bus.registers.end(),
                        std::pair<uint8_t, uint16_t>{4U, 0x10U}),
              bus.registers.end());
    EXPECT_NE(std::find(bus.registers.begin(), bus.registers.end(),
                        std::pair<uint8_t, uint16_t>{6U, 0U}),
              bus.registers.end());
    EXPECT_EQ(nx_wm8978_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(WM8978, GainUpdateUsesStereoLatchAndPreservesExplicitMute) {
    ready();
    bus.registers.clear();
    ASSERT_EQ(nx_wm8978_volume(&device, -12, -6, 1200U), NX_SUCCESS);
    ASSERT_EQ(bus.registers.size(), 2U);
    EXPECT_EQ(bus.registers[0], (std::pair<uint8_t, uint16_t>{52U, 0x6dU}));
    EXPECT_EQ(bus.registers[1], (std::pair<uint8_t, uint16_t>{53U, 0x173U}));
    EXPECT_EQ(device.left_db, -12);
    EXPECT_EQ(device.right_db, -6);
    ASSERT_EQ(nx_wm8978_mute(&device, false, 1200U), NX_SUCCESS);
    EXPECT_FALSE(device.muted);
    EXPECT_EQ(bus.registers.back(), (std::pair<uint8_t, uint16_t>{10U, 0U}));
    EXPECT_EQ(nx_wm8978_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(WM8978, InvalidClockFormatAndGainCauseNoControlWrite) {
    config.word_bits = 18U;
    EXPECT_EQ(nx_wm8978_initialize(&device, &config, 1000000U),
              NX_ERROR_INVALID);
    EXPECT_TRUE(bus.registers.empty());
    config.word_bits = 16U;
    config.mclk_hz = 10000000U;
    EXPECT_EQ(nx_wm8978_initialize(&device, &config, 1000000U),
              NX_ERROR_INVALID);
    EXPECT_TRUE(bus.registers.empty());
    config.mclk_hz = 12288000U;
    ready();
    const size_t before = bus.registers.size();
    EXPECT_EQ(nx_wm8978_volume(&device, -58, 0, 1200U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_wm8978_volume(&device, 0, 7, 1200U), NX_ERROR_INVALID);
    EXPECT_EQ(bus.registers.size(), before);
    EXPECT_EQ(nx_wm8978_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(WM8978, PartialControlFailureRetainsExactBindingUntilStopSucceeds) {
    ready();
    bus.fail_at = bus.registers.size() + 1U;
    EXPECT_EQ(nx_wm8978_volume(&device, -12, -6, 1200U), NX_ERROR_IO);
    EXPECT_EQ(device.phase, NX_WM8978_UNCERTAIN);
    EXPECT_EQ(device.config, &config);
    auto different = config;
    EXPECT_EQ(nx_wm8978_initialize(&device, &different, 2000000U),
              NX_ERROR_BUSY);
    bus.fail_at = bus.registers.size() + 1U;
    EXPECT_EQ(nx_wm8978_stop(&device, 2000000U), NX_ERROR_IO);
    EXPECT_EQ(device.config, &config);
    bus.fail_at = SIZE_MAX;
    ASSERT_EQ(nx_wm8978_stop(&device, 2000000U), NX_SUCCESS);
    EXPECT_EQ(device.config, nullptr);
    const size_t size = bus.registers.size();
    EXPECT_EQ(bus.registers[size - 3U], (std::pair<uint8_t, uint16_t>{1U, 0U}));
    EXPECT_EQ(bus.registers[size - 2U], (std::pair<uint8_t, uint16_t>{2U, 0U}));
    EXPECT_EQ(bus.registers[size - 1U], (std::pair<uint8_t, uint16_t>{3U, 0U}));
}
TEST_F(WM8978, ShortWritesAndClockRollbackNeverBecomeReady) {
    bus.short_write = true;
    EXPECT_EQ(nx_wm8978_initialize(&device, &config, 1000000U), NX_ERROR_IO);
    EXPECT_EQ(device.phase, NX_WM8978_UNCERTAIN);
    bus.short_write = false;
    ASSERT_EQ(nx_wm8978_stop(&device, 2000000U), NX_SUCCESS);
    ASSERT_EQ(nx_wm8978_initialize(&device, &config, 1000000U), NX_SUCCESS);
    bus.now = 99U;
    EXPECT_EQ(nx_wm8978_service(&device), NX_ERROR_STATE);
    EXPECT_TRUE(device.clock_failed);
    const size_t before = bus.registers.size();
    bus.now = 10000U;
    EXPECT_EQ(nx_wm8978_stop(&device, 2000000U), NX_ERROR_STATE);
    EXPECT_EQ(bus.registers.size(), before);
    EXPECT_EQ(device.config, &config);
}
TEST_F(WM8978, InsufficientWholeStartupBudgetHasNoDeviceEffects) {
    EXPECT_EQ(nx_wm8978_initialize(&device, &config, 1100U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(bus.registers.empty());
    EXPECT_EQ(device.config, nullptr);
}
TEST_F(WM8978, DeadlineAfterLeftShadowRetainsUncertainPairResponsibility) {
    ready();
    bus.expire_read = bus.clock_reads + 3U;
    bus.expire_at = 1200U;
    EXPECT_EQ(nx_wm8978_volume(&device, -12, -6, 1200U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(device.phase, NX_WM8978_UNCERTAIN);
    EXPECT_EQ(device.config, &config);
    EXPECT_EQ(device.left_db, -57);
    EXPECT_EQ(device.right_db, -57);
    EXPECT_EQ(nx_wm8978_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(WM8978, ReinitializeCannotRepairAClockRollbackNumerically) {
    ready();
    bus.fail_at = bus.registers.size() + 1U;
    ASSERT_EQ(nx_wm8978_volume(&device, -12, -6, 1200U), NX_ERROR_IO);
    bus.now = 1099U;
    EXPECT_EQ(nx_wm8978_initialize(&device, &config, 1000000U), NX_ERROR_STATE);
    EXPECT_TRUE(device.clock_failed);
    const size_t before = bus.registers.size();
    bus.now = 10000U;
    EXPECT_EQ(nx_wm8978_initialize(&device, &config, 1000000U), NX_ERROR_STATE);
    EXPECT_EQ(bus.registers.size(), before);
    EXPECT_EQ(device.config, &config);
}
} /* namespace */
