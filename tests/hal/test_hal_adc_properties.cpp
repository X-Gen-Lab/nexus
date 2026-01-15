/**
 * \file            test_hal_adc_properties.cpp
 * \brief           HAL ADC Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for ADC module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <cstdint>
#include <gtest/gtest.h>
#include <random>

extern "C" {
#include "hal/hal_adc.h"
#include "native_platform.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           ADC Property Test Fixture
 */
class HalAdcPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        native_adc_reset_all();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        native_adc_reset_all();
    }

    hal_adc_instance_t randomInstance() {
        std::uniform_int_distribution<int> dist(0, HAL_ADC_MAX - 1);
        return static_cast<hal_adc_instance_t>(dist(rng));
    }

    hal_adc_resolution_t randomResolution() {
        std::uniform_int_distribution<int> dist(0, 3);
        return static_cast<hal_adc_resolution_t>(dist(rng));
    }

    uint8_t randomChannel() {
        std::uniform_int_distribution<int> dist(0, 15);
        return static_cast<uint8_t>(dist(rng));
    }

    uint16_t randomRawValue(hal_adc_resolution_t resolution) {
        uint16_t max_val;
        switch (resolution) {
            case HAL_ADC_RES_6BIT:
                max_val = 63;
                break;
            case HAL_ADC_RES_8BIT:
                max_val = 255;
                break;
            case HAL_ADC_RES_10BIT:
                max_val = 1023;
                break;
            case HAL_ADC_RES_12BIT:
                max_val = 4095;
                break;
            default:
                max_val = 4095;
                break;
        }
        std::uniform_int_distribution<uint16_t> dist(0, max_val);
        return dist(rng);
    }

    uint32_t randomVref() {
        /* Common reference voltages: 1210mV (internal), 3300mV (VDD), 2500mV,
         * 5000mV */
        std::uniform_int_distribution<uint32_t> dist(1000, 5000);
        return dist(rng);
    }

    uint16_t getMaxValueForResolution(hal_adc_resolution_t resolution) {
        switch (resolution) {
            case HAL_ADC_RES_6BIT:
                return 63;
            case HAL_ADC_RES_8BIT:
                return 255;
            case HAL_ADC_RES_10BIT:
                return 1023;
            case HAL_ADC_RES_12BIT:
                return 4095;
            default:
                return 4095;
        }
    }

    hal_adc_config_t makeConfig(hal_adc_resolution_t resolution) {
        return hal_adc_config_t{.resolution = resolution,
                                .reference = HAL_ADC_REF_VDD,
                                .sample_time = HAL_ADC_SAMPLE_15CYCLES};
    }
};

/**
 * Feature: phase2-core-platform, Property 13: ADC Voltage Conversion
 *
 * *For any* raw ADC value, the millivolt conversion SHALL follow:
 * mv = raw * vref_mv / max_value, where max_value depends on resolution.
 *
 * **Validates: Requirements 6.4**
 */
TEST_F(HalAdcPropertyTest, Property13_VoltageConversion) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomInstance();
        auto resolution = randomResolution();
        auto raw_value = randomRawValue(resolution);
        auto vref_mv = randomVref();

        hal_adc_config_t config = makeConfig(resolution);

        ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance;

        /* Calculate expected millivolts using the formula */
        uint16_t max_value = getMaxValueForResolution(resolution);
        uint32_t expected_mv = (uint32_t)raw_value * vref_mv / max_value;

        /* Get actual millivolts from the API */
        uint32_t actual_mv =
            hal_adc_to_millivolts(instance, raw_value, vref_mv);

        EXPECT_EQ(expected_mv, actual_mv)
            << "Iteration " << i << ": voltage conversion mismatch. "
            << "raw=" << raw_value << " vref=" << vref_mv
            << " resolution=" << resolution << " max_value=" << max_value
            << " expected=" << expected_mv << " actual=" << actual_mv;

        hal_adc_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 13 (Extended): ADC Voltage Boundary
 * Values
 *
 * *For any* ADC resolution, the conversion at max raw value SHALL equal
 * vref_mv, and the conversion at raw value 0 SHALL equal 0.
 *
 * **Validates: Requirements 6.4**
 */
TEST_F(HalAdcPropertyTest, Property13_VoltageBoundaryValues) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomInstance();
        auto resolution = randomResolution();
        auto vref_mv = randomVref();

        hal_adc_config_t config = makeConfig(resolution);

        ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config))
            << "Iteration " << i << ": init failed";

        uint16_t max_value = getMaxValueForResolution(resolution);

        /* At raw value 0, millivolts should be 0 */
        uint32_t mv_at_zero = hal_adc_to_millivolts(instance, 0, vref_mv);
        EXPECT_EQ(0u, mv_at_zero)
            << "Iteration " << i << ": conversion at 0 should be 0";

        /* At max raw value, millivolts should be vref_mv */
        uint32_t mv_at_max =
            hal_adc_to_millivolts(instance, max_value, vref_mv);
        EXPECT_EQ(vref_mv, mv_at_max)
            << "Iteration " << i << ": conversion at max should equal vref. "
            << "resolution=" << resolution << " max_value=" << max_value
            << " vref=" << vref_mv << " actual=" << mv_at_max;

        hal_adc_deinit(instance);
    }
}

/**
 * Feature: phase2-core-platform, Property 13 (Extended): ADC Voltage
 * Monotonicity
 *
 * *For any* two raw ADC values where raw1 < raw2, the converted millivolts
 * SHALL satisfy mv1 <= mv2 (monotonically increasing).
 *
 * **Validates: Requirements 6.4**
 */
TEST_F(HalAdcPropertyTest, Property13_VoltageMonotonicity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomInstance();
        auto resolution = randomResolution();
        auto vref_mv = randomVref();

        hal_adc_config_t config = makeConfig(resolution);

        ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config))
            << "Iteration " << i << ": init failed";

        /* Generate two random raw values and ensure raw1 < raw2 */
        uint16_t raw1 = randomRawValue(resolution);
        uint16_t raw2 = randomRawValue(resolution);
        if (raw1 > raw2) {
            std::swap(raw1, raw2);
        }

        uint32_t mv1 = hal_adc_to_millivolts(instance, raw1, vref_mv);
        uint32_t mv2 = hal_adc_to_millivolts(instance, raw2, vref_mv);

        EXPECT_LE(mv1, mv2)
            << "Iteration " << i << ": voltage conversion not monotonic. "
            << "raw1=" << raw1 << " raw2=" << raw2 << " mv1=" << mv1
            << " mv2=" << mv2;

        hal_adc_deinit(instance);
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 15: ADC Voltage Conversion Accuracy
 *
 * *For any* raw ADC value V, resolution R, and reference voltage Vref,
 * `hal_adc_to_millivolts(V, Vref)` SHALL return `(V * Vref) / max_value`
 * where max_value = 2^R - 1.
 *
 * **Validates: Requirements 8.4**
 */
TEST_F(HalAdcPropertyTest, Property15_ADCVoltageConversionAccuracy) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomInstance();
        auto resolution = randomResolution();
        auto raw_value = randomRawValue(resolution);
        auto vref_mv = randomVref();

        hal_adc_config_t config = makeConfig(resolution);

        ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config))
            << "Iteration " << i << ": init failed for instance=" << instance;

        /* Calculate expected millivolts using the exact formula from design:
         * mv = (V * Vref) / max_value where max_value = 2^R - 1 */
        uint16_t max_value = getMaxValueForResolution(resolution);
        uint32_t expected_mv = ((uint32_t)raw_value * vref_mv) / max_value;

        /* Get actual millivolts from the API */
        uint32_t actual_mv =
            hal_adc_to_millivolts(instance, raw_value, vref_mv);

        EXPECT_EQ(expected_mv, actual_mv)
            << "Iteration " << i << ": voltage conversion accuracy mismatch. "
            << "raw=" << raw_value << " vref=" << vref_mv
            << " resolution=" << resolution << " max_value=" << max_value
            << " expected=" << expected_mv << " actual=" << actual_mv;

        hal_adc_deinit(instance);
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 16: ADC Resolution Configuration
 *
 * *For any* ADC resolution (6/8/10/12 bit), the ADC CR1.RES bits SHALL be
 * configured correctly: 12-bit=00, 10-bit=01, 8-bit=10, 6-bit=11.
 *
 * **Validates: Requirements 8.1, 8.8**
 */
TEST_F(HalAdcPropertyTest, Property16_ADCResolutionConfiguration) {
    /* Test all resolution configurations */
    hal_adc_resolution_t resolutions[] = {HAL_ADC_RES_6BIT, HAL_ADC_RES_8BIT,
                                          HAL_ADC_RES_10BIT, HAL_ADC_RES_12BIT};

    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomInstance();

        /* Test each resolution */
        for (auto resolution : resolutions) {
            native_adc_reset_all();

            hal_adc_config_t config = makeConfig(resolution);

            ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config))
                << "Iteration " << i
                << ": init failed for resolution=" << resolution;

            /* Verify the resolution was configured correctly by checking
             * that the max value for reads matches the expected resolution */
            hal_adc_resolution_t configured_res =
                native_adc_get_resolution(instance);

            EXPECT_EQ(resolution, configured_res)
                << "Iteration " << i << ": resolution mismatch. "
                << "expected=" << resolution << " actual=" << configured_res;

            hal_adc_deinit(instance);
        }
    }
}

/**
 * Feature: stm32f4-hal-adapter, Property 15 (Extended): ADC Voltage Conversion
 * Linearity
 *
 * *For any* ADC resolution and reference voltage, the voltage conversion
 * SHALL be linear: doubling the raw value SHALL approximately double the
 * millivolt output (within integer rounding).
 *
 * **Validates: Requirements 8.4**
 */
TEST_F(HalAdcPropertyTest, Property15_VoltageConversionLinearity) {
    for (int i = 0; i < PROPERTY_TEST_ITERATIONS; ++i) {
        native_adc_reset_all();

        auto instance = randomInstance();
        auto resolution = randomResolution();
        auto vref_mv = randomVref();

        hal_adc_config_t config = makeConfig(resolution);

        ASSERT_EQ(HAL_OK, hal_adc_init(instance, &config))
            << "Iteration " << i << ": init failed";

        uint16_t max_value = getMaxValueForResolution(resolution);

        /* Generate a raw value that can be doubled without overflow */
        std::uniform_int_distribution<uint16_t> dist(1, max_value / 2);
        uint16_t raw1 = dist(rng);
        uint16_t raw2 = raw1 * 2;

        uint32_t mv1 = hal_adc_to_millivolts(instance, raw1, vref_mv);
        uint32_t mv2 = hal_adc_to_millivolts(instance, raw2, vref_mv);

        /* mv2 should be approximately 2 * mv1 (within integer rounding) */
        uint32_t expected_mv2 = mv1 * 2;

        /* Allow for integer rounding error of 1 */
        EXPECT_LE(std::abs((int32_t)mv2 - (int32_t)expected_mv2), 1)
            << "Iteration " << i << ": linearity check failed. "
            << "raw1=" << raw1 << " raw2=" << raw2 << " mv1=" << mv1
            << " mv2=" << mv2 << " expected_mv2=" << expected_mv2;

        hal_adc_deinit(instance);
    }
}
