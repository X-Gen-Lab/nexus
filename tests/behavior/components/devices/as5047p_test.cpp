/**
 * \file            as5047p_test.cpp
 * \brief           Real AS5047P protocol against a pipelined SPI boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/as5047p.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace {
/** \brief Only model the sensor wire registers and previous-command reply. */
class Sensor {
  public:
    nx_time_us_t now{10U};
    uint16_t pending{0x1234U};
    uint16_t angle{0x2345U};
    uint16_t diagnostic{0x0180U};
    uint16_t settings{0x0001U};
    uint16_t error{0U};
    unsigned fail_at{0U};
    unsigned corrupt_at{0U};
    unsigned expire_at{0U};
    bool short_transfer{false};
    std::vector<uint16_t> commands;
    static uint16_t parity(uint16_t word) {
        uint16_t folded = word;
        folded ^= static_cast<uint16_t>(folded >> 8U);
        folded ^= static_cast<uint16_t>(folded >> 4U);
        folded ^= static_cast<uint16_t>(folded >> 2U);
        folded ^= static_cast<uint16_t>(folded >> 1U);
        return static_cast<uint16_t>(word | ((folded & 1U) << 15U));
    }
    static nx_time_us_t time(void* context) {
        return static_cast<Sensor*>(context)->now++;
    }
    static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                                size_t length, nx_time_us_t deadline,
                                size_t* transferred) {
        auto& self = *static_cast<Sensor*>(context);
        EXPECT_EQ(length, 2U);
        if (length != 2U || tx == nullptr || rx == nullptr) {
            return NX_ERROR_INVALID;
        }
        uint16_t command =
            static_cast<uint16_t>((static_cast<uint16_t>(tx[0]) << 8U) | tx[1]);
        self.commands.push_back(command);
        if (self.commands.size() == self.fail_at) {
            *transferred = 1U;
            return NX_ERROR_TIMEOUT;
        }
        uint16_t response = self.pending;
        if (self.commands.size() == self.corrupt_at) {
            response ^= 0x8000U;
        }
        rx[0] = static_cast<uint8_t>(response >> 8U);
        rx[1] = static_cast<uint8_t>(response);
        EXPECT_EQ(parity(command & 0x7fffU), command);
        EXPECT_NE(command & 0x4000U, 0U);
        uint16_t reg = command & 0x3fffU;
        uint16_t data = 0U;
        if (reg == 0x0018U) {
            data = self.settings;
        } else if (reg == 0x3ffcU) {
            data = self.diagnostic;
        } else if (reg == 0x3ffeU || reg == 0x3fffU) {
            data = self.angle;
        } else if (reg == 0x0001U) {
            data = self.error;
        } else {
            EXPECT_EQ(reg, 0U);
        }
        self.pending = parity(
            static_cast<uint16_t>(data | (self.error != 0U ? 0x4000U : 0U)));
        if (reg == 0x0001U) {
            self.error = 0U;
        }
        if (self.commands.size() == self.expire_at) {
            self.now = deadline;
        }
        *transferred = self.short_transfer ? 1U : 2U;
        return NX_SUCCESS;
    }
    static const nx_spi_endpoint_ops_t ops;
    nx_spi_endpoint_t endpoint{&ops, this};
};
const nx_spi_endpoint_ops_t Sensor::ops{transfer, nullptr, nullptr, nullptr,
                                        nullptr};

class AS5047P : public testing::Test {
  protected:
    Sensor media;
    nx_as5047p_t device{};
    nx_as5047p_config_t config{
        &media.endpoint, {Sensor::time, &media}, 1U, 1U, false};
};

TEST_F(AS5047P, ReadsTheReplyToItsOwnCommandInsteadOfThePreviousFrame) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    nx_as5047p_sample_t sample{};
    ASSERT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_SUCCESS);
    EXPECT_EQ(sample.raw_angle, media.angle);
    EXPECT_EQ(sample.diagnostics, media.diagnostic);
    EXPECT_EQ(sample.observed.source, NX_IO_TIMESTAMP_SERVICE_OBSERVATION);
    EXPECT_FALSE(sample.observed.uncertainty_known);
    EXPECT_EQ(sample.observed.domain, 1U);
    ASSERT_GE(media.commands.size(), 2U);
    EXPECT_EQ(media.commands[media.commands.size() - 2U] & 0x3fffU, 0x3ffeU);
    EXPECT_EQ(media.commands.back() & 0x3fffU, 0U);
    EXPECT_EQ(nx_as5047p_stop(&device), NX_SUCCESS);
}

TEST_F(AS5047P, RejectsBadParityWithoutPublishingAnAngle) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    media.corrupt_at = static_cast<unsigned>(media.commands.size()) + 4U;
    nx_as5047p_sample_t sample{};
    sample.raw_angle = 99U;
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_IO);
    EXPECT_EQ(sample.raw_angle, 99U);
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_STATE);
}

TEST_F(AS5047P, ClearsErrorExplicitlyThenRequiresFreshInitialization) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    media.error = 4U;
    nx_as5047p_sample_t sample{};
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_IO);
    uint16_t flags = 0xffffU;
    ASSERT_EQ(nx_as5047p_clear_error(&device, &flags, 1000U), NX_SUCCESS);
    EXPECT_EQ(flags, 4U);
    EXPECT_EQ(media.error, 0U);
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_STATE);
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_SUCCESS);
}

TEST_F(AS5047P, RejectsUnreadyOrOverflowedDiagnosticsAndReportsFieldWarnings) {
    media.diagnostic = 0x0080U;
    EXPECT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_ERROR_BUSY);
    media.diagnostic = 0x0380U;
    EXPECT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_ERROR_IO);
    media.diagnostic = 0x0580U;
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    nx_as5047p_sample_t sample{};
    ASSERT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_SUCCESS);
    EXPECT_NE(sample.diagnostics & 0x0400U, 0U);
}

TEST_F(AS5047P, CompensatedSelectionChecksActualSensorSettings) {
    config.compensated = true;
    media.settings = 0x0041U;
    EXPECT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_ERROR_STATE);
    media.settings = 0x0001U;
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    nx_as5047p_sample_t sample{};
    ASSERT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_SUCCESS);
    EXPECT_EQ(media.commands[media.commands.size() - 2U] & 0x3fffU, 0x3fffU);
}

TEST_F(AS5047P, ExpiredAndOverlappingCallsCannotAccessSpi) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    size_t before = media.commands.size();
    nx_as5047p_sample_t sample{};
    EXPECT_EQ(nx_as5047p_read(&device, &sample, media.now), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_as5047p_read(&device,
                              reinterpret_cast<nx_as5047p_sample_t*>(&device),
                              1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(media.commands.size(), before);
}

TEST_F(AS5047P, TransportAndFinalDeadlineErrorsRetainTheSameBinding) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    media.expire_at = static_cast<unsigned>(media.commands.size()) + 4U;
    nx_as5047p_sample_t sample{};
    sample.raw_angle = 55U;
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(sample.raw_angle, 55U);
    nx_as5047p_config_t other = config;
    EXPECT_EQ(nx_as5047p_init(&device, &other, 2000U), NX_ERROR_STATE);
    EXPECT_EQ(nx_as5047p_stop(&device), NX_SUCCESS);
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 2000U), NX_ERROR_STATE);
}

TEST_F(AS5047P, ClockRollbackCannotResumeWhenNumericTimeLaterCatchesUp) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    media.now = 0U;
    nx_as5047p_sample_t sample{};
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_STATE);
    media.now = 500U;
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_STATE);
}

TEST_F(AS5047P, FailedAndShortFramesDoNotPublishOrAutomaticallyReplay) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    media.fail_at = static_cast<unsigned>(media.commands.size()) + 1U;
    nx_as5047p_sample_t sample{};
    sample.raw_angle = 77U;
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(sample.raw_angle, 77U);
    size_t before = media.commands.size();
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_STATE);
    EXPECT_EQ(media.commands.size(), before);
    EXPECT_EQ(nx_as5047p_stop(&device), NX_SUCCESS);
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    media.short_transfer = true;
    EXPECT_EQ(nx_as5047p_read(&device, &sample, 1000U), NX_ERROR_IO);
    EXPECT_EQ(sample.raw_angle, 77U);
}

TEST_F(AS5047P, InstancesKeepTheirEndpointAngleAndClockIdentity) {
    ASSERT_EQ(nx_as5047p_init(&device, &config, 1000U), NX_SUCCESS);
    Sensor other_media;
    other_media.angle = 123U;
    nx_as5047p_t other_device{};
    nx_as5047p_config_t other_config{
        &other_media.endpoint, {Sensor::time, &other_media}, 2U, 3U, false};
    ASSERT_EQ(nx_as5047p_init(&other_device, &other_config, 1000U), NX_SUCCESS);
    nx_as5047p_sample_t first{};
    nx_as5047p_sample_t second{};
    ASSERT_EQ(nx_as5047p_read(&device, &first, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_as5047p_read(&other_device, &second, 1000U), NX_SUCCESS);
    EXPECT_EQ(first.raw_angle, 0x2345U);
    EXPECT_EQ(second.raw_angle, 123U);
    EXPECT_EQ(first.observed.domain, 1U);
    EXPECT_EQ(second.observed.domain, 2U);
    EXPECT_EQ(second.observed.epoch, 3U);
}
} /* namespace */
