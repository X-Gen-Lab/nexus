/**
 * \file            test_nx_dac_properties.cpp
 * \brief           DAC Property-Based Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Property-based tests for DAC peripheral implementation.
 *                  Each property is tested with 100+ random inputs.
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "hal/interface/nx_dac.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_dac_helpers.h"
}

/**
 * \brief           DAC Property Test Fixture
 */
class DacPropertyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Initialize random number generator */
        rng.seed(std::random_device{}());

        /* Reset all DAC instances */
        native_dac_reset_all();

        /* Get DAC instance */
        dac = nx_factory_dac(0);
        ASSERT_NE(nullptr, dac);

        /* Get lifecycle interface */
        lifecycle = dac->get_lifecycle(dac);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize DAC */
        ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    }

    void TearDown() override {
        /* Deinitialize DAC */
        if (dac != nullptr && lifecycle != nullptr) {
            lifecycle->deinit(lifecycle);
        }

        /* Reset all instances */
        native_dac_reset_all();
    }

    /* Random number generator */
    std::mt19937 rng;

    /* DAC interfaces */
    nx_dac_t* dac = nullptr;
    nx_lifecycle_t* lifecycle = nullptr;

    /* Helper functions */
    uint16_t randomDacValue() {
        /* Generate random 12-bit DAC value */
        std::uniform_int_distribution<uint16_t> dist(0, 4095);
        return dist(rng);
    }

    int randomChannel() {
        /* Generate random channel index (0-3) */
        std::uniform_int_distribution<int> dist(0, 3);
        return dist(rng);
    }

    uint32_t randomVoltage() {
        /* Generate random voltage in mV (0-3300) */
        std::uniform_int_distribution<uint32_t> dist(0, 3300);
        return dist(rng);
    }
};

/*---------------------------------------------------------------------------*/
/* Property 1: Initialization Idempotence                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 1: Initialization Idempotence
 *
 * *For any* DAC instance and configuration, multiple initializations with
 * the same configuration should produce the same result state.
 *
 * **Validates: Requirements 7.1**
 */
TEST_F(DacPropertyTest, Property1_InitializationIdempotence) {
    for (int i = 0; i < 100; ++i) {
        /* Reset DAC */
        native_dac_reset_all();

        /* Get fresh instance */
        dac = nx_factory_dac(0);
        ASSERT_NE(nullptr, dac);
        lifecycle = dac->get_lifecycle(dac);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize once */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Get state after first init */
        native_dac_state_t state1;
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state1));

        /* Initialize again */
        nx_status_t status = lifecycle->init(lifecycle);
        /* May return OK or ALREADY_INIT, both are acceptable */
        EXPECT_TRUE(status == NX_OK || status == NX_ERR_ALREADY_INIT);

        /* Get state after second init */
        native_dac_state_t state2;
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state2));

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
 * *For any* DAC instance, initializing then immediately deinitializing
 * should restore the DAC to uninitialized state.
 *
 * **Validates: Requirements 7.7**
 */
TEST_F(DacPropertyTest, Property2_LifecycleRoundTrip) {
    for (int i = 0; i < 100; ++i) {
        /* Reset DAC */
        native_dac_reset_all();

        /* Get fresh instance */
        dac = nx_factory_dac(0);
        ASSERT_NE(nullptr, dac);
        lifecycle = dac->get_lifecycle(dac);
        ASSERT_NE(nullptr, lifecycle);

        /* Initialize */
        EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

        /* Verify initialized */
        native_dac_state_t state;
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
        EXPECT_TRUE(state.initialized);

        /* Deinitialize */
        EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

        /* Verify uninitialized */
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
        EXPECT_FALSE(state.initialized);
    }
}

/*---------------------------------------------------------------------------*/
/* Property 3: Power Management Round Trip                                   */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 3: Power Management Round Trip
 *
 * *For any* DAC instance and state, entering low-power mode then waking up
 * should restore the original state.
 *
 * **Validates: Requirements 7.6, 7.7**
 */
TEST_F(DacPropertyTest, Property3_PowerManagementRoundTrip) {
    for (int i = 0; i < 100; ++i) {
        /* Get power interface */
        nx_power_t* power = dac->get_power(dac);
        ASSERT_NE(nullptr, power);

        /* Set random output value */
        int channel = randomChannel();
        uint16_t value = randomDacValue();
        nx_dac_channel_t* ch =
            dac->get_channel(dac, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);
        ch->set_value(ch, value);

        /* Get state before suspend */
        native_dac_state_t state_before;
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state_before));

        /* Suspend */
        EXPECT_EQ(NX_OK, power->disable(power));

        /* Verify suspended */
        native_dac_state_t state_suspended;
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state_suspended));
        EXPECT_TRUE(state_suspended.suspended);

        /* Resume */
        EXPECT_EQ(NX_OK, power->enable(power));

        /* Get state after resume */
        native_dac_state_t state_after;
        EXPECT_EQ(NX_OK, native_dac_get_state(0, &state_after));

        /* State should be restored (except suspended flag) */
        EXPECT_FALSE(state_after.suspended);
        EXPECT_EQ(state_before.initialized, state_after.initialized);
        EXPECT_EQ(state_before.clock_enabled, state_after.clock_enabled);

        /* DAC value should persist */
        EXPECT_EQ(value, native_dac_get_output_value(
                             0, static_cast<uint8_t>(channel)));
    }
}

/*---------------------------------------------------------------------------*/
/* Property 17: DAC Output Value Consistency                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 17: DAC Output Value Consistency
 *
 * *For any* DAC channel and output value, setting the value then querying
 * should return the same value.
 *
 * **Validates: Requirements 7.2**
 */
TEST_F(DacPropertyTest, Property17_DacOutputValueConsistency) {
    for (int i = 0; i < 100; ++i) {
        /* Generate random channel and value */
        int channel = randomChannel();
        uint16_t value = randomDacValue();

        /* Get channel interface */
        nx_dac_channel_t* ch =
            dac->get_channel(dac, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);

        /* Set value */
        ch->set_value(ch, value);

        /* Query value */
        uint32_t queried_value =
            native_dac_get_output_value(0, static_cast<uint8_t>(channel));

        /* Values should match */
        EXPECT_EQ(value, queried_value);
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Voltage to Value Conversion Consistency              */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Voltage to Value Conversion
 *
 * *For any* DAC channel and voltage, setting voltage then querying the raw
 * value should produce a value consistent with the voltage-to-value formula.
 *
 * **Validates: Requirements 7.2**
 */
TEST_F(DacPropertyTest, PropertyVoltageToValueConversion) {
    for (int i = 0; i < 100; ++i) {
        /* Generate random channel and voltage */
        int channel = randomChannel();
        uint32_t voltage_mv = randomVoltage();

        /* Get channel interface */
        nx_dac_channel_t* ch =
            dac->get_channel(dac, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);

        /* Set voltage */
        ch->set_voltage_mv(ch, voltage_mv);

        /* Query raw value */
        uint32_t raw_value =
            native_dac_get_output_value(0, static_cast<uint8_t>(channel));

        /* Calculate expected value: voltage / vref * max_value */
        /* For 12-bit DAC with 3300mV reference */
        uint32_t expected_value = (voltage_mv * 4095U) / 3300U;

        /* Allow small rounding error */
        EXPECT_NEAR(expected_value, raw_value, 1U);
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Value Clamping                                       */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Value Clamping
 *
 * *For any* DAC channel and value (including out-of-range), the stored
 * value should never exceed the maximum resolution.
 *
 * **Validates: Requirements 7.2**
 */
TEST_F(DacPropertyTest, PropertyValueClamping) {
    for (int i = 0; i < 100; ++i) {
        /* Generate random channel and potentially out-of-range value */
        int channel = randomChannel();
        std::uniform_int_distribution<uint32_t> dist(0, 10000);
        uint32_t value = dist(rng);

        /* Get channel interface */
        nx_dac_channel_t* ch =
            dac->get_channel(dac, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);

        /* Set value */
        ch->set_value(ch, value);

        /* Query value */
        uint32_t queried_value =
            native_dac_get_output_value(0, static_cast<uint8_t>(channel));

        /* Value should be clamped to maximum (12-bit: 4095) */
        EXPECT_LE(queried_value, 4095U);

        /* If input was within range, should match exactly */
        if (value <= 4095U) {
            EXPECT_EQ(value, queried_value);
        } else {
            /* If input was out of range, should be clamped to max */
            EXPECT_EQ(4095U, queried_value);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Multi-Channel Independence                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Multi-Channel Independence
 *
 * *For any* set of channels with different values, setting one channel
 * should not affect the values of other channels.
 *
 * **Validates: Requirements 7.2**
 */
TEST_F(DacPropertyTest, PropertyMultiChannelIndependence) {
    for (int i = 0; i < 100; ++i) {
        /* Set random values for all channels */
        std::vector<uint16_t> values;
        for (int ch = 0; ch < 4; ++ch) {
            uint16_t value = randomDacValue();
            values.push_back(value);

            nx_dac_channel_t* channel =
                dac->get_channel(dac, static_cast<uint8_t>(ch));
            ASSERT_NE(nullptr, channel);
            channel->set_value(channel, value);
        }

        /* Verify all channels have correct values */
        for (int ch = 0; ch < 4; ++ch) {
            uint32_t queried_value =
                native_dac_get_output_value(0, static_cast<uint8_t>(ch));
            EXPECT_EQ(values[ch], queried_value);
        }

        /* Modify one channel */
        int modified_channel = randomChannel();
        uint16_t new_value = randomDacValue();
        nx_dac_channel_t* channel =
            dac->get_channel(dac, static_cast<uint8_t>(modified_channel));
        ASSERT_NE(nullptr, channel);
        channel->set_value(channel, new_value);

        /* Verify modified channel has new value */
        EXPECT_EQ(new_value, native_dac_get_output_value(
                                 0, static_cast<uint8_t>(modified_channel)));

        /* Verify other channels are unchanged */
        for (int ch = 0; ch < 4; ++ch) {
            if (ch != modified_channel) {
                uint32_t queried_value =
                    native_dac_get_output_value(0, static_cast<uint8_t>(ch));
                EXPECT_EQ(values[ch], queried_value);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: Value Persistence After Trigger                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: Value Persistence After Trigger
 *
 * *For any* DAC channel and value, triggering output should not change
 * the stored value.
 *
 * **Validates: Requirements 7.2, 7.3**
 */
TEST_F(DacPropertyTest, PropertyValuePersistenceAfterTrigger) {
    for (int i = 0; i < 100; ++i) {
        /* Set random value */
        int channel = randomChannel();
        uint16_t value = randomDacValue();

        nx_dac_channel_t* ch =
            dac->get_channel(dac, static_cast<uint8_t>(channel));
        ASSERT_NE(nullptr, ch);
        ch->set_value(ch, value);

        /* Trigger multiple times */
        std::uniform_int_distribution<int> trigger_dist(1, 10);
        int num_triggers = trigger_dist(rng);

        for (int j = 0; j < num_triggers; ++j) {
            dac->trigger(dac);

            /* Verify value persists */
            uint32_t queried_value =
                native_dac_get_output_value(0, static_cast<uint8_t>(channel));
            EXPECT_EQ(value, queried_value);
        }
    }
}
