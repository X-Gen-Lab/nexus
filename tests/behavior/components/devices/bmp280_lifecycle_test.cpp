/**
 * \file            bmp280_lifecycle_test.cpp
 * \brief           BMP280 calibration, conversion and settled transport tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-26
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/bmp280.h"
#include <array>
#include <gtest/gtest.h>

namespace {
constexpr nx_bmp280_calibration_t calibration{27504,  26435, -1000,  36477,
                                              -10685, 3024,  2855,   140,
                                              -7,     15500, -14600, 6000};

struct Bmp280Boundary {
    std::array<uint8_t, 256> registers{};
    nx_time_us_t now = 0U;
    unsigned calls = 0U;
    unsigned writes = 0U;
    bool borrowed = false;
    nx_result_t failure = NX_SUCCESS;

    static nx_time_us_t read_clock(void* context) {
        return static_cast<Bmp280Boundary*>(context)->now;
    }

    static nx_result_t exchange(void* context, const uint8_t* tx, uint8_t* rx,
                                size_t length, nx_time_us_t deadline) {
        auto& self = *static_cast<Bmp280Boundary*>(context);
        EXPECT_FALSE(self.borrowed);
        EXPECT_GE(length, 2U);
        EXPECT_NE(tx, nullptr);
        EXPECT_GE(deadline, self.now);
        if (self.borrowed || length < 2U || tx == nullptr ||
            deadline < self.now) {
            return NX_ERROR_INVALID;
        }
        self.borrowed = true;
        ++self.calls;
        if (self.failure != NX_SUCCESS) {
            self.borrowed = false;
            return self.failure;
        }
        if ((tx[0] & 0x80U) != 0U) {
            EXPECT_NE(rx, nullptr);
            if (rx == nullptr) {
                self.borrowed = false;
                return NX_ERROR_INVALID;
            }
            rx[0] = 0xffU;
            for (size_t i = 1U; i < length; ++i) {
                rx[i] = self.registers[static_cast<uint8_t>(tx[0] + i - 1U)];
            }
        } else {
            EXPECT_EQ(length, 2U);
            if (length != 2U) {
                self.borrowed = false;
                return NX_ERROR_INVALID;
            }
            self.registers[tx[0] | 0x80U] = tx[1];
            ++self.writes;
        }
        self.borrowed = false;
        return NX_SUCCESS;
    }
};

class Bmp280Lifecycle : public testing::Test {
  protected:
    Bmp280Boundary boundary;
    nx_bmp280_t device{};
    nx_bmp280_transport_port_t transport{
        &boundary,
        Bmp280Boundary::exchange,
        {Bmp280Boundary::read_clock, &boundary}};

    void SetUp() override {
        boundary.registers[0xd0] = 0x58U;
        const std::array<int32_t, 12> words{27504,  26435, -1000,  36477,
                                            -10685, 3024,  2855,   140,
                                            -7,     15500, -14600, 6000};
        for (size_t i = 0U; i < words.size(); ++i) {
            const auto bits = static_cast<uint16_t>(words[i]);
            boundary.registers[0x88U + i * 2U] = static_cast<uint8_t>(bits);
            boundary.registers[0x89U + i * 2U] =
                static_cast<uint8_t>(bits >> 8U);
        }
        constexpr uint32_t raw_pressure = 415148U;
        constexpr uint32_t raw_temperature = 519888U;
        for (size_t i = 0U; i < 3U; ++i) {
            const auto shift = static_cast<unsigned>(16U - i * 8U);
            boundary.registers[0xf7U + i] =
                static_cast<uint8_t>((raw_pressure << 4U) >> shift);
            boundary.registers[0xfaU + i] =
                static_cast<uint8_t>((raw_temperature << 4U) >> shift);
        }
    }
};

TEST(Bmp280Compensation, BoschVectorAndInvalidSamplesPreserveOutput) {
    nx_bmp280_sample_t sample{-9999, 123U};
    ASSERT_EQ(nx_bmp280_compensate(&calibration, 519888U, 415148U, &sample),
              NX_SUCCESS);
    EXPECT_EQ(sample.centi_celsius, 2508);
    EXPECT_EQ(sample.pressure_pa, 100653U);
    const auto previous = sample;
    EXPECT_EQ(nx_bmp280_compensate(&calibration, 0x80000U, 415148U, &sample),
              NX_ERROR_IO);
    EXPECT_EQ(sample.centi_celsius, previous.centi_celsius);
    EXPECT_EQ(sample.pressure_pa, previous.pressure_pa);
    EXPECT_EQ(nx_bmp280_compensate(&calibration, 0x100000U, 415148U, &sample),
              NX_ERROR_INVALID);
    auto broken = calibration;
    broken.p1 = 0U;
    EXPECT_EQ(nx_bmp280_compensate(&broken, 519888U, 415148U, &sample),
              NX_ERROR_INVALID);
}

TEST(Bmp280Compensation, DeterministicCoefficientInputsKeepGoldenVectorValid) {
    uint32_t seed = 7U;
    nx_bmp280_sample_t sample{};
    for (unsigned i = 0U; i < 10000U; ++i) {
        std::array<uint8_t, 24> bytes{};
        for (auto& byte : bytes) {
            seed = seed * 1664525U + 1013904223U;
            byte = static_cast<uint8_t>(seed >> 24U);
        }
        nx_bmp280_calibration_t coefficients{};
        if (nx_bmp280_parse_calibration(bytes.data(), bytes.size(),
                                        &coefficients) == NX_SUCCESS) {
            (void)nx_bmp280_compensate(&coefficients, seed & 0xfffffU,
                                       (seed >> 4U) & 0xfffffU, &sample);
        }
    }
    ASSERT_EQ(nx_bmp280_compensate(&calibration, 519888U, 415148U, &sample),
              NX_SUCCESS);
    EXPECT_EQ(sample.centi_celsius, 2508);
    EXPECT_EQ(sample.pressure_pa, 100653U);
}

TEST_F(Bmp280Lifecycle, InitializationRejectsIdNvmAndTransportFailure) {
    boundary.registers[0xd0] = 0x60U;
    EXPECT_EQ(nx_bmp280_init(&device, transport, 100000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(boundary.borrowed);
    boundary.registers[0xd0] = 0x58U;
    boundary.registers[0xf3] = 1U;
    EXPECT_EQ(nx_bmp280_init(&device, transport, 100000U), NX_ERROR_BUSY);
    boundary.registers[0xf3] = 0U;
    boundary.failure = NX_ERROR_IO;
    EXPECT_EQ(nx_bmp280_init(&device, transport, 100000U), NX_ERROR_IO);
    boundary.failure = NX_SUCCESS;
    ASSERT_EQ(nx_bmp280_init(&device, transport, 100000U), NX_SUCCESS);
    EXPECT_EQ(device.calibration.p2, -10685);
    EXPECT_EQ(boundary.writes, 1U);
}

TEST_F(Bmp280Lifecycle,
       ConversionWaitsAndRecoversFromTimeoutAndTransportError) {
    ASSERT_EQ(nx_bmp280_init(&device, transport, 100000U), NX_SUCCESS);
    ASSERT_EQ(nx_bmp280_begin(&device, 100000U), NX_SUCCESS);
    EXPECT_EQ(boundary.registers[0xf4], 0x25U);
    nx_bmp280_sample_t sample{};
    const unsigned calls = boundary.calls;
    EXPECT_EQ(nx_bmp280_poll(&device, &sample), NX_ERROR_BUSY);
    EXPECT_EQ(boundary.calls, calls);
    boundary.now = 8000U;
    boundary.registers[0xf3] = 8U;
    EXPECT_EQ(nx_bmp280_poll(&device, &sample), NX_ERROR_BUSY);
    boundary.registers[0xf3] = 0U;
    ASSERT_EQ(nx_bmp280_poll(&device, &sample), NX_SUCCESS);
    EXPECT_EQ(sample.centi_celsius, 2508);
    EXPECT_EQ(sample.pressure_pa, 100653U);
    EXPECT_EQ(nx_bmp280_poll(&device, &sample), NX_ERROR_STATE);
    ASSERT_EQ(nx_bmp280_begin(&device, 9000U), NX_SUCCESS);
    boundary.now = 9000U;
    EXPECT_EQ(nx_bmp280_poll(&device, &sample), NX_ERROR_TIMEOUT);
    boundary.registers[0xf3] = 8U;
    EXPECT_EQ(nx_bmp280_begin(&device, 20000U), NX_ERROR_BUSY);
    boundary.registers[0xf3] = 0U;
    ASSERT_EQ(nx_bmp280_begin(&device, 20000U), NX_SUCCESS);
    boundary.now = 18000U;
    boundary.failure = NX_ERROR_IO;
    EXPECT_EQ(nx_bmp280_poll(&device, &sample), NX_ERROR_IO);
    EXPECT_FALSE(device.measuring);
    EXPECT_FALSE(boundary.borrowed);
    nx_bmp280_stop(&device);
    EXPECT_EQ(nx_bmp280_begin(&device, 30000U), NX_ERROR_STATE);
}
} /* namespace */
