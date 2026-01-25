/**
 * \file            test_nx_adc_properties.cpp
 * \brief           ADC Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Property-based tests for ADC peripheral implementation.
 *                  Each property is tested with 100+ random inputs.
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_adc.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_adc_helpers.h"
}

/**
 * \brief           ADC Property Test Fixture
 */
class AdcPropertyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Initialize random number generator */
        rng.seed(std::random_device{}());

        /* Reset all ADC instances */
        native_adc_reset_all();

        /* Get ADC instance */
        adc = nx_factory_adc(0);
        ASSERT_NE(nullptr, adc);

        /* Get lifecycle interface */
        lifecycle = adc->get_lifecycle(adc);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize ADC */
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize ADC */
        if (adc != nullptr && lifecycle != nullptr) {
            lifecycle->deinit(lifecycle);
        }

        /* Reset all instances */
        native_adc_reset_all();
    }

    /* Random number generator */
    std::mt19937 rng;

    /* ADC interfaces */
    nx_adc_t* adc = nullptr;
    nx_lifecycle_t* lifecycle = nullptr;

    /* Helper functions */
    uint16_t randomAdcValue() {
        /* Generate random 12-bit ADC value */
        std::uniform_int_distribution<uint16_t> dist(0, 4095);
        return dist(rng);
    }

    int randomChannel() {
        /* Generate random channel index (0-7) */
        std::uniform_int_distribution<int> dist(0, 7);
        return dist(rng);
    }

    size_t randomConversionCount() {
        /* Generate random conversion count */
        std::uniform_int_distribution<size_t> dist(1, 50);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotence                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotence
 *
 * *For any* ADC instance and configuration, multiple initializations with
 * the same configuration should produce the same result state.
 *
 * **Validates: Requirements 6.1**
 */
TEST_F(AdcPropertyTest, Property1_InitializationIdempotence) {
    for (int i = 0; i < 100; ++i) {
        /* Reset ADC */
        native_adc_reset_all();

        /* Get fresh instance */
        adc = nx_factory_adc(0);
        ASSERT_NE(nullptr, adc);
        lifecycle = adc->get_lifecycle(adc);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize once */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Get state after first init */
        native_adc_state_t state1;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state1));

        /* Initialize again */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Get state after second init */
        native_adc_state_t state2;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state2));

        /* States should be identical */
        EXPECT_EQ(state1.initialized, state2.initialized);
        EXPECT_EQ(state1.suspended, state2.suspended);
        EXPECT_EQ(state1.clock_enabled, state2.clock_enabled);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 2: Lifecycle Round Trip                                          */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 2: Lifecycle Round Trip
 *
 * *For any* ADC instance, initializing then immediately deinitializing
 * should restore the ADC to uninitialized state.
 *
 * **Validates: Requirements 6.9**
 */
TEST_F(AdcPropertyTest, Property2_LifecycleRoundTrip) {
    for (int i = 0; i < 100; ++i) {
        /* Reset ADC */
        native_adc_reset_all();

        /* Get fresh instance */
        adc = nx_factory_adc(0);
        ASSERT_NE(nullptr, adc);
        lifecycle = adc->get_lifecycle(adc);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_adc_state_t state;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
        EXPECT_TRUE(state.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify uninitialized */
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
        EXPECT_FALSE(state.initialized);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round Trip                                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round Trip
 *
 * *For any* ADC instance and state, entering low-power mode then waking up
 * should restore the original state.
 *
 * **Validates: Requirements 6.8, 6.9**
 */
TEST_F(AdcPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int i = 0; i < 100; ++i) {
        /* Get power interface */
        nx_power_t* power = adc->get_power(adc);
        ASSERT_NE(nullptr, power);

        /* Set random analog value */
        int channel = randomChannel();
        uint16_t value = randomAdcValue();
        EXPECT_EQ(NX_OK, native_adc_set_analog_value(
                             0, static_cast<uint8_t>(channel), value));

        /* Trigger conversion */
        adc->trigger(adc);

        /* Get state before suspend */
        native_adc_state_t state_before;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state_before));

        /* Suspend */
        EXPECT_EQ(NX_OK, power->disable(power));

        /* Verify suspended */
        native_adc_state_t state_suspended;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state_suspended));
        EXPECT_TRUE(state_suspended.suspended);

        /* Resume */
        EXPECT_EQ(NX_OK, power->enable(power));

        /* Get state after resume */
        native_adc_state_t state_after;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state_after));

        /* State should be restored (except suspended flag) */
        EXPECT_FALSE(state_after.suspended);
        EXPECT_EQ(state_before.initialized, state_after.initialized);
        EXPECT_EQ(state_before.clock_enabled, state_after.clock_enabled);

        /* ADC should still work */
        adc->trigger(adc);
        nx_adc_channel_t* ch =
            adc->get_channel(adc, static_cast<uint8_t>(channel));
        if (ch != nullptr) {
            EXPECT_EQ(value, ch->get_value(ch));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Property 15: ADC Sampling Value Range                                     */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 15: ADC Sampling Value Range
 *
 * *For any* ADC configuration and input value, sampling results should be
 * within the valid range (0 to 2^resolution-1).
 *
 * **Validates: Requirements 6.2**
 */
TEST_F(AdcPropertyTest, Property15_AdcSamplingValueRange) {
    for (int i = 0; i < 100; ++i) {
        /* Generate random channel and value */
        int channel = randomChannel();
        uint16_t input_value = randomAdcValue();

        /* Set analog value */
        EXPECT_EQ(NX_OK, native_adc_set_analog_value(
                             0, static_cast<uint8_t>(channel), input_value));

        /* Trigger conversion */
        adc->trigger(adc);

        /* Get channel and read value */
        nx_adc_channel_t* ch =
            adc->get_channel(adc, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);

        uint32_t sampled_value = ch->get_value(ch);

        /* Verify value is within valid range (12-bit ADC: 0-4095) */
        EXPECT_LE(sampled_value, 4095U);
        EXPECT_GE(sampled_value, 0U);

        /* Verify value matches input */
        EXPECT_EQ(input_value, sampled_value);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 16: ADC Continuous Sampling Count                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 16: ADC Continuous Sampling Count
 *
 * *For any* ADC configuration, continuous sampling N times should produce
 * N sample values.
 *
 * **Validates: Requirements 6.3**
 */
TEST_F(AdcPropertyTest, Property16_AdcContinuousSamplingCount) {
    for (int i = 0; i < 100; ++i) {
        /* Reset ADC to clear conversion count */
        native_adc_reset(0);
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Generate random conversion count */
        size_t num_conversions = randomConversionCount();

        /* Set random analog value */
        int channel = randomChannel();
        uint16_t value = randomAdcValue();
        EXPECT_EQ(NX_OK, native_adc_set_analog_value(
                             0, static_cast<uint8_t>(channel), value));

        /* Perform N conversions */
        for (size_t j = 0; j < num_conversions; ++j) {
            adc->trigger(adc);
        }

        /* Verify conversion count */
        native_adc_state_t state;
        EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
        EXPECT_EQ(num_conversions, state.conversion_count);

        /* Verify we can still read the value */
        nx_adc_channel_t* ch =
            adc->get_channel(adc, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);
        EXPECT_EQ(value, ch->get_value(ch));
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Multi-Channel Sampling Consistency                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Multi-Channel Sampling Consistency
 *
 * *For any* set of channels with different analog values, a single trigger
 * should sample all channels correctly.
 *
 * **Validates: Requirements 6.2**
 */
TEST_F(AdcPropertyTest, PropertyMultiChannelSamplingConsistency) {
    for (int i = 0; i < 100; ++i) {
        /* Generate random values for multiple channels */
        std::vector<uint16_t> values;
        size_t num_channels = 4;

        for (size_t ch = 0; ch < num_channels; ++ch) {
            uint16_t value = randomAdcValue();
            values.push_back(value);
            EXPECT_EQ(NX_OK, native_adc_set_analog_value(
                                 0, static_cast<uint8_t>(ch), value));
        }

        /* Trigger single conversion */
        adc->trigger(adc);

        /* Verify all channels have correct values */
        for (size_t ch = 0; ch < num_channels; ++ch) {
            nx_adc_channel_t* channel =
                adc->get_channel(adc, static_cast<uint8_t>(ch));
            ASSERT_NE(nullptr, channel);
            EXPECT_EQ(values[ch], channel->get_value(channel));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Value Persistence Between Triggers                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Value Persistence Between Triggers
 *
 * *For any* ADC channel, if the analog value doesn't change, multiple
 * triggers should return the same sampled value.
 *
 * **Validates: Requirements 6.2**
 */
TEST_F(AdcPropertyTest, PropertyValuePersistenceBetweenTriggers) {
    for (int i = 0; i < 100; ++i) {
        /* Set random analog value */
        int channel = randomChannel();
        uint16_t value = randomAdcValue();
        EXPECT_EQ(NX_OK, native_adc_set_analog_value(
                             0, static_cast<uint8_t>(channel), value));

        /* Trigger multiple times */
        size_t num_triggers = randomConversionCount();
        std::vector<uint32_t> sampled_values;

        for (size_t j = 0; j < num_triggers; ++j) {
            adc->trigger(adc);
            nx_adc_channel_t* ch =
                adc->get_channel(adc, static_cast<uint8_t>(channel));
            ASSERT_NE(nullptr, ch);
            sampled_values.push_back(ch->get_value(ch));
        }

        /* All sampled values should be identical */
        for (size_t j = 0; j < num_triggers; ++j) {
            EXPECT_EQ(value, sampled_values[j]);
        }
    }
}
