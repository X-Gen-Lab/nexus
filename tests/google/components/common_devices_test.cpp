/**
 * \file            common_devices_test.cpp
 * \brief           Final GPIO callback deadline and clock-domain regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/software_i2c.h"
#include "nexus/components/software_spi.h"
#include <array>
#include <functional>
#include <gtest/gtest.h>

namespace {
/** \brief Model external GPIO completion time without replacing bus logic. */
class GpioBoundary {
  public:
    struct Pin {
        GpioBoundary* boundary;
        unsigned index;
        bool high;
    };
    std::array<Pin, 4> pins{{{this, 0U, true},
                             {this, 1U, true},
                             {this, 2U, true},
                             {this, 3U, true}}};
    std::array<nx_gpio_port_t, 4> faces{};
    nx_time_us_t now{100U};
    std::function<void(unsigned, bool)> after_write;
    std::function<void(unsigned)> after_read;

    GpioBoundary() {
        for (unsigned i = 0U; i < faces.size(); ++i) {
            faces[i] = {&operations, &pins[i]};
        }
    }

    static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
        auto& pin = *static_cast<Pin*>(context);
        EXPECT_EQ(set | reset, 1U);
        EXPECT_EQ(set & reset, 0U);
        pin.high = set != 0U;
        if (pin.boundary->after_write) {
            pin.boundary->after_write(pin.index, pin.high);
        }
        return NX_SUCCESS;
    }

    static nx_result_t read(const void* context, uint32_t* value) {
        const auto& pin = *static_cast<const Pin*>(context);
        *value = pin.high ? 1U : 0U;
        if (pin.boundary->after_read) {
            pin.boundary->after_read(pin.index);
        }
        return NX_SUCCESS;
    }

    static nx_time_us_t clock(void* context) {
        auto& boundary = *static_cast<GpioBoundary*>(context);
        if (boundary.now != NX_DEADLINE_NEVER) {
            ++boundary.now;
        }
        return boundary.now;
    }

    static const nx_gpio_ops_t operations;
};
const nx_gpio_ops_t GpioBoundary::operations = {GpioBoundary::write,
                                                GpioBoundary::read, nullptr};

struct FinalObservation {
    nx_time_us_t callback_time;
    nx_result_t expected;
};

class SpiFinalObservation : public testing::TestWithParam<FinalObservation> {};

TEST_P(SpiFinalObservation, ChipSelectWithdrawalCannotHideExpiredClock) {
    GpioBoundary pins;
    nx_software_spi_t owner{};
    const nx_software_spi_config_t config{{&pins.faces[0], 1U},
                                          {&pins.faces[1], 1U},
                                          {&pins.faces[2], 1U},
                                          {&pins.faces[3], 1U},
                                          {GpioBoundary::clock, &pins},
                                          1U,
                                          1U,
                                          1U,
                                          8U,
                                          1U};
    ASSERT_EQ(nx_software_spi_init(&owner, &config), NX_SUCCESS);
    bool selected = false;
    pins.after_write = [&](unsigned pin, bool high) {
        if (pin == 3U) {
            if (high && selected) {
                pins.now = GetParam().callback_time;
            }
            selected = !high;
        }
    };
    nx_spi_endpoint_t endpoint = nx_software_spi_endpoint(&owner);
    const uint8_t byte = 0xa5U;
    size_t transferred = 0U;
    EXPECT_EQ(nx_spi_endpoint_transfer(&endpoint, &byte, nullptr, 1U, 1000U,
                                       &transferred),
              GetParam().expected);
    EXPECT_EQ(transferred, 1U);
    EXPECT_TRUE(nx_software_spi_faulted(&owner));
    EXPECT_EQ(owner.config, &config);
    EXPECT_TRUE(pins.pins[3].high);
    EXPECT_FALSE(pins.pins[0].high);
    EXPECT_FALSE(pins.pins[1].high);
}

INSTANTIATE_TEST_SUITE_P(
    CallbackCompletion, SpiFinalObservation,
    testing::Values(FinalObservation{1000U, NX_ERROR_TIMEOUT},
                    FinalObservation{0U, NX_ERROR_IO},
                    FinalObservation{NX_DEADLINE_NEVER, NX_ERROR_IO}));

class I2cFinalObservation : public testing::TestWithParam<FinalObservation> {};

TEST_P(I2cFinalObservation, StopReadbackCannotHideExpiredClock) {
    GpioBoundary pins;
    nx_software_i2c_t owner{};
    const nx_software_i2c_config_t config{{&pins.faces[0], 1U},
                                          {&pins.faces[1], 1U},
                                          {GpioBoundary::clock, &pins},
                                          5U,
                                          16U};
    ASSERT_EQ(nx_software_i2c_init(&owner, &config, 1000U), NX_SUCCESS);
    unsigned scl_reads = 0U;
    pins.after_read = [&](unsigned pin) {
        if (pin == 0U && ++scl_reads == 2U) {
            pins.now = GetParam().callback_time;
        }
    };
    EXPECT_EQ(nx_software_i2c_recover(&owner, 1000U), GetParam().expected);
    EXPECT_EQ(scl_reads, 2U);
    EXPECT_EQ(nx_software_i2c_cleanup_result(&owner), GetParam().expected);
    EXPECT_TRUE(nx_software_i2c_faulted(&owner));
    EXPECT_EQ(owner.config, &config);
    EXPECT_TRUE(pins.pins[0].high);
    EXPECT_TRUE(pins.pins[1].high);
}

INSTANTIATE_TEST_SUITE_P(
    CallbackCompletion, I2cFinalObservation,
    testing::Values(FinalObservation{1000U, NX_ERROR_TIMEOUT},
                    FinalObservation{0U, NX_ERROR_IO},
                    FinalObservation{NX_DEADLINE_NEVER, NX_ERROR_IO}));

TEST(I2cStopProof, ClockFaultDuringNewStopCannotReusePreviousIdleProof) {
    GpioBoundary pins;
    nx_software_i2c_t owner{};
    const nx_software_i2c_config_t config{{&pins.faces[0], 1U},
                                          {&pins.faces[1], 1U},
                                          {GpioBoundary::clock, &pins},
                                          5U,
                                          16U};
    ASSERT_EQ(nx_software_i2c_init(&owner, &config, 1000U), NX_SUCCESS);
    unsigned scl_reads = 0U;
    pins.after_read = [&](unsigned pin) {
        if (pin == 0U && ++scl_reads == 2U) {
            pins.now = 0U;
        }
    };
    ASSERT_EQ(nx_software_i2c_recover(&owner, 1000U), NX_ERROR_IO);
    pins.after_read = {};
    EXPECT_EQ(nx_software_i2c_stop(&owner, 2000U), NX_ERROR_IO);
    EXPECT_EQ(owner.config, &config);
    EXPECT_TRUE(nx_software_i2c_faulted(&owner));
}
} /* namespace */
