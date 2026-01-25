/**
 * \file            test_nx_dac.cpp
 * \brief           DAC Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for DAC peripheral implementation.
 *                  Requirements: 7.1-7.7, 21.1-21.3
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_dac.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_dac_helpers.h"
}

/**
 * \brief           DAC Test Fixture
 */
class DacTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all DAC instances before each test */
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

    nx_dac_t* dac = nullptr;
    nx_lifecycle_t* lifecycle = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 7.1, 7.2                         */
/*---------------------------------------------------------------------------*/

TEST_F(DacTest, InitializeDac) {
    /* Already initialized in SetUp, check state */
    native_dac_state_t state;
    EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(DacTest, SetOutputValue) {
    /* Get channel interface */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);

    /* Set output value */
    channel->set_value(channel, 2048);

    /* Verify value was set */
    uint32_t value = native_dac_get_output_value(0, 0);
    EXPECT_EQ(2048U, value);
}

TEST_F(DacTest, SetOutputVoltage) {
    /* Get channel interface */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);

    /* Set output voltage (1650mV = half of 3300mV reference) */
    channel->set_voltage_mv(channel, 1650);

    /* Verify value was set (should be approximately half of max value) */
    uint32_t value = native_dac_get_output_value(0, 0);
    /* For 12-bit DAC: 1650/3300 * 4095 = 2047.5 */
    EXPECT_NEAR(2047U, value, 1U);
}

TEST_F(DacTest, MultipleChannels) {
    /* Set values for multiple channels */
    nx_dac_channel_t* ch0 = dac->get_channel(dac, 0);
    nx_dac_channel_t* ch1 = dac->get_channel(dac, 1);

    ASSERT_NE(nullptr, ch0);
    ASSERT_NE(nullptr, ch1);

    ch0->set_value(ch0, 1000);
    ch1->set_value(ch1, 2000);

    /* Verify values */
    EXPECT_EQ(1000U, native_dac_get_output_value(0, 0));
    EXPECT_EQ(2000U, native_dac_get_output_value(0, 1));
}

TEST_F(DacTest, GetInvalidChannel) {
    /* Try to get invalid channel */
    nx_dac_channel_t* channel = dac->get_channel(dac, 255);
    EXPECT_EQ(nullptr, channel);
}

TEST_F(DacTest, TriggerOutput) {
    /* Set output value */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_value(channel, 3000);

    /* Trigger output update */
    dac->trigger(dac);

    /* Verify value is still set */
    EXPECT_EQ(3000U, native_dac_get_output_value(0, 0));
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 7.1, 7.7                                   */
/*---------------------------------------------------------------------------*/

TEST_F(DacTest, Deinitialize) {
    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Verify state */
    native_dac_state_t state;
    EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
    EXPECT_FALSE(state.initialized);
}

TEST_F(DacTest, ReinitializeAfterDeinit) {
    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Reinitialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify state */
    native_dac_state_t state;
    EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 7.5, 7.6                            */
/*---------------------------------------------------------------------------*/

TEST_F(DacTest, SuspendAndResume) {
    /* Get power interface */
    nx_power_t* power = dac->get_power(dac);
    ASSERT_NE(nullptr, power);

    /* Set output value */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_value(channel, 1500);

    /* Disable power */
    EXPECT_EQ(NX_OK, power->disable(power));

    /* Verify suspended state */
    native_dac_state_t state;
    EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
    EXPECT_TRUE(state.suspended);

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));

    /* Verify resumed state */
    EXPECT_EQ(NX_OK, native_dac_get_state(0, &state));
    EXPECT_FALSE(state.suspended);

    /* DAC should still work after resume */
    EXPECT_EQ(1500U, native_dac_get_output_value(0, 0));
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 21.1, 21.2, 21.3                      */
/*---------------------------------------------------------------------------*/

TEST_F(DacTest, NullPointerHandling) {
    /* Test null pointer in get_channel */
    nx_dac_channel_t* channel = dac->get_channel(nullptr, 0);
    EXPECT_EQ(nullptr, channel);
}

TEST_F(DacTest, InvalidInstanceHandling) {
    /* Try to get state of invalid instance */
    native_dac_state_t state;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, native_dac_get_state(255, &state));
}

TEST_F(DacTest, OperationOnUninitializedDac) {
    /* Deinitialize DAC */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Try to set value (should handle gracefully) */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    if (channel != nullptr) {
        channel->set_value(channel, 1000);
        /* Operation should not crash */
    }
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(DacTest, ZeroValue) {
    /* Set zero output value */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_value(channel, 0);

    /* Verify value */
    EXPECT_EQ(0U, native_dac_get_output_value(0, 0));
}

TEST_F(DacTest, MaxValue) {
    /* Set maximum output value (12-bit DAC) */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_value(channel, 4095);

    /* Verify value */
    EXPECT_EQ(4095U, native_dac_get_output_value(0, 0));
}

TEST_F(DacTest, ValueClamping) {
    /* Try to set value beyond maximum */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_value(channel, 10000);

    /* Value should be clamped to maximum */
    uint32_t value = native_dac_get_output_value(0, 0);
    EXPECT_LE(value, 4095U);
}

TEST_F(DacTest, VoltageClamping) {
    /* Try to set voltage beyond reference */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_voltage_mv(channel, 5000);

    /* Value should be clamped to maximum */
    uint32_t value = native_dac_get_output_value(0, 0);
    EXPECT_LE(value, 4095U);
}

TEST_F(DacTest, MultipleUpdatesOnSameChannel) {
    /* Set different values multiple times */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);

    channel->set_value(channel, 1000);
    EXPECT_EQ(1000U, native_dac_get_output_value(0, 0));

    channel->set_value(channel, 2000);
    EXPECT_EQ(2000U, native_dac_get_output_value(0, 0));

    channel->set_value(channel, 3000);
    EXPECT_EQ(3000U, native_dac_get_output_value(0, 0));
}

TEST_F(DacTest, ValuePersistence) {
    /* Set value */
    nx_dac_channel_t* channel = dac->get_channel(dac, 0);
    ASSERT_NE(nullptr, channel);
    channel->set_value(channel, 2500);

    /* Trigger multiple times */
    dac->trigger(dac);
    dac->trigger(dac);
    dac->trigger(dac);

    /* Value should persist */
    EXPECT_EQ(2500U, native_dac_get_output_value(0, 0));
}
