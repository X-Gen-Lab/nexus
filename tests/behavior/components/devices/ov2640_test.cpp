/**
 * \file            ov2640_test.cpp
 * \brief           Real ST sensor initialization and checked SCCB boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ov2640.h"
#include <array>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {
/** \brief Only settled SCCB register access, external wait and time are
 * modeled. */
class CameraBus {
  public:
    std::array<std::array<uint8_t, 256U>, 2U> registers{};
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    nx_time_us_t now{10U};
    nx_time_us_t deadline_seen{};
    size_t fail_at{SIZE_MAX};
    unsigned waits{};
    uint8_t bank{};
    CameraBus() {
        registers[1][0x0aU] = 0x26U;
        registers[1][0x0bU] = 0x41U;
        registers[1][0x09U] = 0x02U;
    }
    static nx_time_us_t clock(void* context) {
        return static_cast<CameraBus*>(context)->now;
    }
    static nx_result_t write(void* context, uint8_t reg, uint8_t value,
                             nx_time_us_t deadline) {
        auto& self = *static_cast<CameraBus*>(context);
        self.writes.emplace_back(reg, value);
        self.deadline_seen = deadline;
        if (self.writes.size() == self.fail_at)
            return NX_ERROR_IO;
        if (reg == 0xffU)
            self.bank = value & 1U;
        else
            self.registers[self.bank][reg] = value;
        return NX_SUCCESS;
    }
    static nx_result_t read(void* context, uint8_t reg, uint8_t* value,
                            nx_time_us_t deadline) {
        auto& self = *static_cast<CameraBus*>(context);
        self.deadline_seen = deadline;
        *value = self.registers[self.bank][reg];
        return NX_SUCCESS;
    }
    static nx_result_t wait(void* context, uint32_t milliseconds,
                            nx_time_us_t deadline) {
        auto& self = *static_cast<CameraBus*>(context);
        ++self.waits;
        self.deadline_seen = deadline;
        self.now += static_cast<uint64_t>(milliseconds) * 1000U;
        return self.now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
    }
    nx_ov2640_port_t port{this, write, read, wait, {clock, this}};
};
class OV2640 : public testing::Test {
  protected:
    CameraBus bus;
    nx_ov2640_config_t config{bus.port, NX_OV2640_QQVGA, 24000000U, 0x41U};
    nx_ov2640_t device{};
};
TEST_F(OV2640, ActualStTableProgramsSelectedRgb565AndExplicitResolution) {
    ASSERT_EQ(nx_ov2640_initialize(&device, &config, 1000000U), NX_SUCCESS);
    EXPECT_TRUE(device.initialized);
    EXPECT_FALSE(device.uncertain);
    EXPECT_GT(bus.writes.size(), 200U);
    EXPECT_GT(bus.waits, 200U);
    EXPECT_EQ(bus.registers[0][0xdaU], 0x09U);
    EXPECT_EQ(bus.registers[0][0x5aU], 0x28U);
    EXPECT_EQ(bus.registers[0][0x5bU], 0x1eU);
    EXPECT_EQ(bus.deadline_seen, 1000000U);
    EXPECT_EQ(nx_ov2640_stop(&device, 2000000U), NX_SUCCESS);
    EXPECT_NE(bus.registers[1][0x09U] & 0x10U, 0U);
    EXPECT_EQ(device.config, nullptr);
}
TEST_F(OV2640, WrongExactIdentityNeverExecutesTheInitializationTable) {
    bus.registers[1][0x0bU] = 0x42U;
    EXPECT_EQ(nx_ov2640_initialize(&device, &config, 1000000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_LT(bus.writes.size(), 3U);
    EXPECT_EQ(bus.waits, 0U);
    EXPECT_TRUE(device.uncertain);
    EXPECT_EQ(device.config, &config);
    EXPECT_EQ(nx_ov2640_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(OV2640, FirstSupplierVoidCallbackFailureSuppressesLaterAccesses) {
    bus.fail_at = 5U;
    EXPECT_EQ(nx_ov2640_initialize(&device, &config, 1000000U), NX_ERROR_IO);
    EXPECT_EQ(bus.writes.size(), 5U);
    EXPECT_TRUE(device.uncertain);
    auto other_config = config;
    EXPECT_EQ(nx_ov2640_initialize(&device, &other_config, 1000000U),
              NX_ERROR_BUSY);
    bus.fail_at = SIZE_MAX;
    EXPECT_EQ(nx_ov2640_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(OV2640, WaitTimeoutRetainsBindingAndStopsTheRemainingTable) {
    EXPECT_EQ(nx_ov2640_initialize(&device, &config, 200000U),
              NX_ERROR_TIMEOUT);
    EXPECT_TRUE(device.uncertain);
    EXPECT_FALSE(device.initialized);
    EXPECT_EQ(bus.waits, 1U);
    EXPECT_LT(bus.writes.size(), 5U);
    EXPECT_EQ(nx_ov2640_stop(&device, 1000000U), NX_SUCCESS);
}
TEST_F(OV2640, InvalidClockAndResolutionHaveNoSensorAccess) {
    config.xclk_hz = 25000000U;
    EXPECT_EQ(nx_ov2640_initialize(&device, &config, 1000000U),
              NX_ERROR_INVALID);
    EXPECT_TRUE(bus.writes.empty());
    config.xclk_hz = 24000000U;
    config.resolution = NX_OV2640_UNKNOWN;
    EXPECT_EQ(nx_ov2640_initialize(&device, &config, 1000000U),
              NX_ERROR_INVALID);
    EXPECT_TRUE(bus.writes.empty());
}
TEST_F(OV2640, StopFailurePreservesTheOwnerAndNeverRestartsCapture) {
    ASSERT_EQ(nx_ov2640_initialize(&device, &config, 1000000U), NX_SUCCESS);
    bus.fail_at = bus.writes.size() + 1U;
    EXPECT_EQ(nx_ov2640_stop(&device, 2000000U), NX_ERROR_IO);
    EXPECT_EQ(device.config, &config);
    EXPECT_TRUE(device.uncertain);
    bus.fail_at = SIZE_MAX;
    EXPECT_EQ(nx_ov2640_stop(&device, 2000000U), NX_SUCCESS);
}
TEST_F(OV2640, TwoExplicitPhysicalPortsKeepIndependentSensorState) {
    ASSERT_EQ(nx_ov2640_initialize(&device, &config, 1000000U), NX_SUCCESS);
    CameraBus second_bus;
    nx_ov2640_config_t second_config{second_bus.port, NX_OV2640_QVGA, 24000000U,
                                     0x41U};
    nx_ov2640_t second{};
    ASSERT_EQ(nx_ov2640_initialize(&second, &second_config, 1000000U),
              NX_SUCCESS);
    EXPECT_EQ(bus.registers[0][0x5aU], 0x28U);
    EXPECT_EQ(second_bus.registers[0][0x5aU], 0x50U);
    EXPECT_EQ(nx_ov2640_stop(&device, 2000000U), NX_SUCCESS);
    EXPECT_TRUE(second.initialized);
    EXPECT_EQ(nx_ov2640_stop(&second, 2000000U), NX_SUCCESS);
}
} /* namespace */
