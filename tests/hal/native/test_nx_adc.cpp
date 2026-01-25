/**
 * \file            test_nx_adc.cpp
 * \brief           ADC Unit Tests for Native Platform
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for ADC peripheral implementation.
 *                  Requirements: 6.1-6.9, 21.1-21.3
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_adc.h"
#include "hal/nx_factory.h"
#include "tests/hal/native/devices/native_adc_helpers.h"
}

/**
 * \brief           ADC Test Fixture
 */
class AdcTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Reset all ADC instances before each test */
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

    nx_adc_t* adc = nullptr;
    nx_lifecycle_t* lifecycle = nullptr;
};

/*---------------------------------------------------------------------------*/
/* Basic Functionality Tests - Requirements 6.1, 6.2                         */
/*---------------------------------------------------------------------------*/

TEST_F(AdcTest, InitializeAdc) {
    /* Already initialized in SetUp, check state */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

TEST_F(AdcTest, TriggerConversion) {
    /* Set analog value for channel 0 */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 2048));

    /* Trigger conversion */
    adc->trigger(adc);

    /* Get channel interface */
    nx_adc_channel_t* channel = adc->get_channel(adc, 0);
    ASSERT_NE(nullptr, channel);

    /* Read converted value */
    uint32_t value = channel->get_value(channel);
    EXPECT_EQ(2048U, value);
}

TEST_F(AdcTest, MultipleChannels) {
    /* Set analog values for multiple channels */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 1000));
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 1, 2000));
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 2, 3000));

    /* Trigger conversion */
    adc->trigger(adc);

    /* Read all channels */
    nx_adc_channel_t* ch0 = adc->get_channel(adc, 0);
    nx_adc_channel_t* ch1 = adc->get_channel(adc, 1);
    nx_adc_channel_t* ch2 = adc->get_channel(adc, 2);

    ASSERT_NE(nullptr, ch0);
    ASSERT_NE(nullptr, ch1);
    ASSERT_NE(nullptr, ch2);

    EXPECT_EQ(1000U, ch0->get_value(ch0));
    EXPECT_EQ(2000U, ch1->get_value(ch1));
    EXPECT_EQ(3000U, ch2->get_value(ch2));
}

TEST_F(AdcTest, GetInvalidChannel) {
    /* Try to get invalid channel */
    nx_adc_channel_t* channel = adc->get_channel(adc, 255);
    EXPECT_EQ(nullptr, channel);
}

/*---------------------------------------------------------------------------*/
/* Diagnostic Tests - Requirement 6.6                                        */
/*---------------------------------------------------------------------------*/

TEST_F(AdcTest, DiagnosticInterface) {
    /* Get diagnostic interface */
    nx_diagnostic_t* diag = adc->get_diagnostic(adc);
    ASSERT_NE(nullptr, diag);

    /* Trigger some conversions */
    adc->trigger(adc);
    adc->trigger(adc);
    adc->trigger(adc);

    /* Check conversion count */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_EQ(3U, state.conversion_count);
}

/*---------------------------------------------------------------------------*/
/* Lifecycle Tests - Requirements 6.1, 6.9                                   */
/*---------------------------------------------------------------------------*/

TEST_F(AdcTest, Deinitialize) {
    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Verify state */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_FALSE(state.initialized);
}

TEST_F(AdcTest, ReinitializeAfterDeinit) {
    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Reinitialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify state */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_TRUE(state.initialized);
}

/*---------------------------------------------------------------------------*/
/* Power Management Tests - Requirements 6.8, 6.9                            */
/*---------------------------------------------------------------------------*/

TEST_F(AdcTest, SuspendAndResume) {
    /* Get power interface */
    nx_power_t* power = adc->get_power(adc);
    ASSERT_NE(nullptr, power);

    /* Set analog value and trigger */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 1500));
    adc->trigger(adc);

    /* Disable power */
    EXPECT_EQ(NX_OK, power->disable(power));

    /* Verify suspended state */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_TRUE(state.suspended);

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));

    /* Verify resumed state */
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_FALSE(state.suspended);

    /* ADC should still work after resume */
    adc->trigger(adc);
    nx_adc_channel_t* channel = adc->get_channel(adc, 0);
    ASSERT_NE(nullptr, channel);
    EXPECT_EQ(1500U, channel->get_value(channel));
}

/*---------------------------------------------------------------------------*/
/* Error Handling Tests - Requirements 21.1, 21.2, 21.3                      */
/*---------------------------------------------------------------------------*/

TEST_F(AdcTest, NullPointerHandling) {
    /* Test null pointer in get_channel */
    nx_adc_channel_t* channel = adc->get_channel(nullptr, 0);
    EXPECT_EQ(nullptr, channel);
}

TEST_F(AdcTest, InvalidInstanceHandling) {
    /* Try to get state of invalid instance */
    native_adc_state_t state;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, native_adc_get_state(255, &state));
}

TEST_F(AdcTest, OperationOnUninitializedAdc) {
    /* Deinitialize ADC */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));

    /* Try to trigger conversion (should handle gracefully) */
    adc->trigger(adc);

    /* Verify no conversions were counted */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_EQ(0U, state.conversion_count);
}

/*---------------------------------------------------------------------------*/
/* Boundary Condition Tests                                                  */
/*---------------------------------------------------------------------------*/

TEST_F(AdcTest, ZeroValue) {
    /* Set zero analog value */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 0));

    /* Trigger and read */
    adc->trigger(adc);
    nx_adc_channel_t* channel = adc->get_channel(adc, 0);
    ASSERT_NE(nullptr, channel);
    EXPECT_EQ(0U, channel->get_value(channel));
}

TEST_F(AdcTest, MaxValue) {
    /* Set maximum analog value (12-bit ADC) */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 4095));

    /* Trigger and read */
    adc->trigger(adc);
    nx_adc_channel_t* channel = adc->get_channel(adc, 0);
    ASSERT_NE(nullptr, channel);
    EXPECT_EQ(4095U, channel->get_value(channel));
}

TEST_F(AdcTest, MultipleTriggersOnSameChannel) {
    /* Set different values and trigger multiple times */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 1000));
    adc->trigger(adc);

    nx_adc_channel_t* channel = adc->get_channel(adc, 0);
    ASSERT_NE(nullptr, channel);
    EXPECT_EQ(1000U, channel->get_value(channel));

    /* Change value and trigger again */
    EXPECT_EQ(NX_OK, native_adc_set_analog_value(0, 0, 2000));
    adc->trigger(adc);
    EXPECT_EQ(2000U, channel->get_value(channel));
}

TEST_F(AdcTest, ConversionCountIncreases) {
    /* Trigger multiple conversions */
    for (int i = 0; i < 10; ++i) {
        adc->trigger(adc);
    }

    /* Check conversion count */
    native_adc_state_t state;
    EXPECT_EQ(NX_OK, native_adc_get_state(0, &state));
    EXPECT_EQ(10U, state.conversion_count);
}
