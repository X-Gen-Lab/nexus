/**
 * \file            ina226_test.cpp
 * \brief           Triggered INA226 scaling and failure against an I2C boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ina226.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
/** \brief Model register effects only; production code owns all sequencing. */
class Monitor {
  public:
    std::array<uint16_t, 256> registers{};
    nx_time_us_t now{10U};
    nx_time_us_t last_deadline{0U};
    unsigned calls{0U};
    unsigned writes{0U};
    unsigned fail_at{0U};
    unsigned expire_at{0U};
    bool short_read{false};
    bool ignore_shutdown_write{false};
    Monitor() {
        registers[0xfe] = 0x5449U;
        registers[0xff] = 0x2260U;
        registers[0x00] = 0x4127U;
        registers[0x01] = 0x1f40U;
        registers[0x02] = 0x2570U;
        registers[0x03] = 0x12b8U;
        registers[0x04] = 0x2710U;
    }
    static nx_time_us_t time(void* context) {
        return static_cast<Monitor*>(context)->now++;
    }
    static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                                   size_t count, nx_time_us_t deadline,
                                   size_t* transferred) {
        auto& self = *static_cast<Monitor*>(context);
        ++self.calls;
        self.last_deadline = deadline;
        if (self.calls == self.fail_at) {
            *transferred = 0U;
            return NX_ERROR_NACK;
        }
        EXPECT_FALSE(messages[0].read);
        uint8_t reg = messages[0].buffer.tx[0];
        if (count == 2U) {
            EXPECT_EQ(messages[0].length, 1U);
            EXPECT_TRUE(messages[1].read);
            EXPECT_EQ(messages[1].length, 2U);
            uint16_t value = self.registers[reg];
            messages[1].buffer.rx[0] = static_cast<uint8_t>(value >> 8U);
            messages[1].buffer.rx[1] = static_cast<uint8_t>(value);
            if (reg == 6U) {
                self.registers[6] &= static_cast<uint16_t>(~0x0008U);
            }
            *transferred = self.short_read ? 2U : 3U;
        } else {
            EXPECT_EQ(count, 1U);
            EXPECT_EQ(messages[0].length, 3U);
            ++self.writes;
            uint16_t value = static_cast<uint16_t>(
                (static_cast<uint16_t>(messages[0].buffer.tx[1]) << 8U) |
                messages[0].buffer.tx[2]);
            if (!(self.ignore_shutdown_write && reg == 0U &&
                  (value & 7U) == 0U)) {
                self.registers[reg] = value;
            }
            if (reg == 0U && (self.registers[0] & 7U) != 0U) {
                self.registers[6] &= static_cast<uint16_t>(~0x0008U);
            }
            *transferred = 3U;
        }
        if (self.calls == self.expire_at) {
            self.now = deadline;
        }
        return NX_SUCCESS;
    }
    static const nx_i2c_endpoint_ops_t ops;
    nx_i2c_endpoint_t endpoint{&ops, this};
};
const nx_i2c_endpoint_ops_t Monitor::ops{transaction, nullptr, nullptr,
                                         nullptr};

class INA226 : public testing::Test {
  protected:
    Monitor media;
    nx_ina226_t device{};
    nx_ina226_config_t config{&media.endpoint,
                              {Monitor::time, &media},
                              1U,
                              1U,
                              2000U,
                              1000000U,
                              0U,
                              4U,
                              4U};
};

TEST_F(INA226, CalibrationUsesCheckedExplicitShuntAndCurrentUnits) {
    uint16_t calibration = 0U;
    EXPECT_EQ(nx_ina226_calibration(2000U, 1000000U, &calibration), NX_SUCCESS);
    EXPECT_EQ(calibration, 2560U);
    EXPECT_EQ(nx_ina226_calibration(0U, 1000000U, &calibration),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_ina226_calibration(1U, 1U, &calibration), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_ina226_calibration(UINT32_MAX, UINT32_MAX, &calibration),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(calibration, 2560U);
}

TEST_F(INA226, VerifiesIdentityAndCalibratesBeforeTriggeredMeasurement) {
    media.registers[0xff] = 0x2261U;
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    EXPECT_EQ(media.registers[5], 2560U);
    EXPECT_EQ(media.registers[0] & 7U, 0U);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    nx_ina226_sample_t sample{};
    EXPECT_EQ(nx_ina226_poll(&device, &sample), NX_ERROR_BUSY);
    media.registers[6] = 0x0008U;
    ASSERT_EQ(nx_ina226_poll(&device, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.shunt_nanovolts, 20000000);
    EXPECT_EQ(sample.bus_microvolts, 11980000U);
    EXPECT_EQ(sample.current_nanoamps, INT64_C(10000000000));
    EXPECT_EQ(sample.power_nanowatts, UINT64_C(119800000000));
    EXPECT_EQ(sample.observed.source, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    EXPECT_FALSE(sample.observed.uncertainty_known);
    EXPECT_EQ(media.last_deadline, 1000U);
    EXPECT_EQ(nx_ina226_poll(&device, &sample), NX_ERROR_STATE);
    EXPECT_EQ(nx_ina226_stop(&device), NX_SUCCESS);
}

TEST_F(INA226, WrongIdentityAndForeignAlertOwnershipRejectBeforeWrites) {
    media.registers[0xfe] = 0xffffU;
    EXPECT_EQ(nx_ina226_init(&device, &config, 1000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(media.writes, 0U);
    media.registers[0xfe] = 0x5449U;
    media.registers[0xff] = 0x2262U;
    EXPECT_EQ(nx_ina226_init(&device, &config, 1000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(media.writes, 0U);
    media.registers[0xff] = 0x2260U;
    media.registers[6] = 0x8000U;
    EXPECT_EQ(nx_ina226_init(&device, &config, 1000U), NX_ERROR_STATE);
    EXPECT_EQ(media.writes, 0U);
}

TEST_F(INA226, MathOverflowCannotPublishPlausibleCurrentOrPower) {
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    media.registers[6] = 0x000cU;
    nx_ina226_sample_t sample{};
    sample.bus_microvolts = 123U;
    EXPECT_EQ(nx_ina226_poll(&device, &sample), NX_ERROR_IO);
    EXPECT_EQ(sample.bus_microvolts, 123U);
    EXPECT_EQ(nx_ina226_stop(&device), NX_ERROR_STATE);
    ASSERT_EQ(nx_ina226_shutdown(&device, 1000U), NX_SUCCESS);
    EXPECT_EQ(media.registers[0] & 7U, 0U);
    EXPECT_EQ(nx_ina226_stop(&device), NX_SUCCESS);
}

TEST_F(INA226, ResetCalibrationAndShortReadsCloseSampling) {
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    media.registers[5] = 0U;
    EXPECT_EQ(nx_ina226_begin(&device, 1000U), NX_ERROR_STATE);
    ASSERT_EQ(nx_ina226_shutdown(&device, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    media.short_read = true;
    nx_ina226_sample_t sample{};
    EXPECT_EQ(nx_ina226_poll(&device, &sample), NX_ERROR_IO);
    EXPECT_EQ(nx_ina226_begin(&device, 1000U), NX_ERROR_STATE);
}

TEST_F(INA226, IssuedWriteFailureRetainsBindingUntilShutdownRecovery) {
    media.fail_at = 4U;
    EXPECT_EQ(nx_ina226_init(&device, &config, 1000U), NX_ERROR_NACK);
    nx_ina226_config_t other = config;
    EXPECT_EQ(nx_ina226_init(&device, &other, 1000U), NX_ERROR_STATE);
    EXPECT_EQ(nx_ina226_stop(&device), NX_ERROR_STATE);
    ASSERT_EQ(nx_ina226_shutdown(&device, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_ina226_stop(&device), NX_SUCCESS);
}

TEST_F(INA226, ExpiredBudgetAndOverlappingOutputDoNotTouchI2c) {
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    unsigned before = media.calls;
    EXPECT_EQ(nx_ina226_begin(&device, media.now), NX_ERROR_TIMEOUT);
    EXPECT_EQ(media.calls, before);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    before = media.calls;
    EXPECT_EQ(
        nx_ina226_poll(&device, reinterpret_cast<nx_ina226_sample_t*>(&device)),
        NX_ERROR_INVALID);
    EXPECT_EQ(media.calls, before);
}

TEST_F(INA226, FinalReadDeadlineFailureDoesNotPublishASample) {
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    media.registers[6] = 0x0008U;
    media.expire_at = media.calls + 7U;
    nx_ina226_sample_t sample{};
    sample.bus_microvolts = 555U;
    EXPECT_EQ(nx_ina226_poll(&device, &sample), NX_ERROR_TIMEOUT);
    EXPECT_EQ(sample.bus_microvolts, 555U);
    EXPECT_EQ(nx_ina226_stop(&device), NX_ERROR_STATE);
}

TEST_F(INA226, InitializationMustObserveShutdownRatherThanATriggeredMode) {
    media.registers[0] = 0x4123U;
    media.ignore_shutdown_write = true;
    EXPECT_EQ(nx_ina226_init(&device, &config, 1000U), NX_ERROR_STATE);
    EXPECT_EQ(nx_ina226_stop(&device), NX_ERROR_STATE);
}

TEST_F(INA226, NegativeShuntAndCurrentUseTheFullSignedRange) {
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    media.registers[1] = 0x8000U;
    media.registers[4] = 0x8000U;
    media.registers[6] = 0x0008U;
    nx_ina226_sample_t sample{};
    ASSERT_EQ(nx_ina226_poll(&device, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.shunt_nanovolts, -81920000);
    EXPECT_EQ(sample.current_nanoamps, INT64_C(-32768000000));
}

TEST_F(INA226, InstancesKeepIndependentCalibrationAndConversionState) {
    ASSERT_EQ(nx_ina226_init(&device, &config, 1000U), NX_SUCCESS);
    Monitor other_media;
    nx_ina226_t other_device{};
    nx_ina226_config_t other_config{&other_media.endpoint,
                                    {Monitor::time, &other_media},
                                    2U,
                                    3U,
                                    2000U,
                                    2000000U,
                                    0U,
                                    4U,
                                    4U};
    ASSERT_EQ(nx_ina226_init(&other_device, &other_config, 1000U), NX_SUCCESS);
    EXPECT_EQ(media.registers[5], 2560U);
    EXPECT_EQ(other_media.registers[5], 1280U);
    ASSERT_EQ(nx_ina226_begin(&device, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_ina226_begin(&other_device, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_ina226_shutdown(&other_device, 1000U), NX_SUCCESS);
    media.registers[6] = 0x0008U;
    nx_ina226_sample_t sample{};
    EXPECT_EQ(nx_ina226_poll(&device, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.observed.domain, 1U);
    EXPECT_EQ(nx_ina226_poll(&other_device, &sample), NX_ERROR_STATE);
}
} /* namespace */
