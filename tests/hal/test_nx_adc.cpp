/**
 * \file            test_nx_adc.cpp
 * \brief           Nexus ADC Interface Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for the new nx_adc_t interface.
 * Tests ADC initialization, single/multi-channel reads, and lifecycle.
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/interface/nx_adc.h"
#include "hal/nx_status.h"

/* Native platform factory functions */
nx_adc_t* nx_adc_native_get(uint8_t adc_index);
nx_adc_t* nx_adc_native_get_with_config(uint8_t adc_index,
                                        const nx_adc_config_t* cfg);
void nx_adc_native_set_simulated_value(uint8_t adc_index, uint8_t channel,
                                       uint16_t value);
}

/**
 * \brief           ADC Test Fixture
 */
class NxAdcTest : public ::testing::Test {
  protected:
    nx_adc_t* adc;

    void SetUp() override {
        adc = nullptr;
    }

    void TearDown() override {
        if (adc) {
            nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
            if (lifecycle) {
                lifecycle->deinit(lifecycle);
            }
        }
    }
};

/**
 * \brief           Test ADC instance creation
 */
TEST_F(NxAdcTest, GetAdcInstance) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);
    EXPECT_NE(nullptr, adc->read);
    EXPECT_NE(nullptr, adc->read_voltage);
    EXPECT_NE(nullptr, adc->get_lifecycle);
}

/**
 * \brief           Test ADC lifecycle initialization
 */
TEST_F(NxAdcTest, LifecycleInit) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_NE(nullptr, lifecycle);

    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_INITIALIZED, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test ADC double initialization
 */
TEST_F(NxAdcTest, DoubleInit) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_NE(nullptr, lifecycle);

    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));
}

/**
 * \brief           Test ADC single channel read
 */
TEST_F(NxAdcTest, ReadSingleChannel) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set simulated value */
    nx_adc_native_set_simulated_value(0, 0, 2048);

    /* Read channel */
    uint16_t value = 0;
    EXPECT_EQ(NX_OK, adc->read(adc, 0, &value));
    EXPECT_EQ(2048, value);
}

/**
 * \brief           Test ADC read on uninitialized instance
 */
TEST_F(NxAdcTest, ReadOnUninitializedInstance) {
    adc = nx_adc_native_get(1);
    ASSERT_NE(nullptr, adc);

    uint16_t value = 0;
    EXPECT_EQ(NX_ERR_NOT_INIT, adc->read(adc, 0, &value));
}

/**
 * \brief           Test ADC read with null pointer
 */
TEST_F(NxAdcTest, ReadNullPointer) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    EXPECT_EQ(NX_ERR_NULL_PTR, adc->read(adc, 0, nullptr));
}

/**
 * \brief           Test ADC read with invalid channel
 */
TEST_F(NxAdcTest, ReadInvalidChannel) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    uint16_t value = 0;
    EXPECT_EQ(NX_ERR_INVALID_PARAM, adc->read(adc, 16, &value));
}

/**
 * \brief           Test ADC voltage reading
 */
TEST_F(NxAdcTest, ReadVoltage) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set reference voltage to 3.3V */
    EXPECT_EQ(NX_OK, adc->set_reference_voltage(adc, 3300));

    /* Set simulated value to half scale (12-bit: 2048 out of 4096) */
    nx_adc_native_set_simulated_value(0, 0, 2048);

    /* Read voltage (should be approximately 1650 mV) */
    uint32_t voltage_mv = 0;
    EXPECT_EQ(NX_OK, adc->read_voltage(adc, 0, &voltage_mv));
    EXPECT_NEAR(1650, voltage_mv, 10); /* Allow 10mV tolerance */
}

/**
 * \brief           Test ADC multi-channel read
 */
TEST_F(NxAdcTest, ReadMultipleChannels) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set simulated values */
    nx_adc_native_set_simulated_value(0, 0, 1000);
    nx_adc_native_set_simulated_value(0, 1, 2000);
    nx_adc_native_set_simulated_value(0, 2, 3000);

    /* Read multiple channels */
    uint8_t channels[] = {0, 1, 2};
    uint16_t values[3] = {0};
    EXPECT_EQ(NX_OK, adc->read_multi(adc, channels, 3, values));
    EXPECT_EQ(1000, values[0]);
    EXPECT_EQ(2000, values[1]);
    EXPECT_EQ(3000, values[2]);
}

/**
 * \brief           Test ADC multi-channel read with null pointer
 */
TEST_F(NxAdcTest, ReadMultiNullPointer) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    uint8_t channels[] = {0, 1, 2};
    EXPECT_EQ(NX_ERR_NULL_PTR, adc->read_multi(adc, channels, 3, nullptr));
    EXPECT_EQ(NX_ERR_NULL_PTR, adc->read_multi(adc, nullptr, 3, nullptr));
}

/**
 * \brief           Test ADC multi-channel read with invalid channel
 */
TEST_F(NxAdcTest, ReadMultiInvalidChannel) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    uint8_t channels[] = {0, 16, 2}; /* Channel 16 is invalid */
    uint16_t values[3] = {0};
    EXPECT_EQ(NX_ERR_INVALID_PARAM, adc->read_multi(adc, channels, 3, values));
}

/**
 * \brief           Test ADC continuous mode
 */
TEST_F(NxAdcTest, ContinuousMode) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Start continuous mode */
    EXPECT_EQ(NX_OK, adc->start_continuous(adc));

    /* Check state */
    nx_adc_stats_t stats = {};
    EXPECT_EQ(NX_OK, adc->get_stats(adc, &stats));
    EXPECT_TRUE(stats.busy);

    /* Stop continuous mode */
    EXPECT_EQ(NX_OK, adc->stop_continuous(adc));
    EXPECT_EQ(NX_OK, adc->get_stats(adc, &stats));
    EXPECT_FALSE(stats.busy);
}

/**
 * \brief           Test ADC continuous mode when already busy
 */
TEST_F(NxAdcTest, ContinuousModeWhenBusy) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Start continuous mode */
    EXPECT_EQ(NX_OK, adc->start_continuous(adc));

    /* Try to start again (should fail) */
    EXPECT_EQ(NX_ERR_BUSY, adc->start_continuous(adc));

    /* Stop */
    EXPECT_EQ(NX_OK, adc->stop_continuous(adc));
}

/**
 * \brief           Test ADC DMA buffer access
 */
TEST_F(NxAdcTest, GetBuffer) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get buffer */
    uint16_t buffer[256] = {0};
    size_t count = 256;
    EXPECT_EQ(NX_OK, adc->get_buffer(adc, buffer, &count));
    EXPECT_LE(count, 256u);
}

/**
 * \brief           Test ADC callback registration
 */
static int callback_count = 0;
static uint8_t callback_channel = 0;
static uint16_t callback_value = 0;

static void test_adc_callback(void* context, uint8_t channel, uint16_t value) {
    (void)context;
    callback_count++;
    callback_channel = channel;
    callback_value = value;
}

TEST_F(NxAdcTest, CallbackRegistration) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    callback_count = 0;

    /* Set callback */
    EXPECT_EQ(NX_OK, adc->set_callback(adc, test_adc_callback, nullptr));

    /* Clear callback */
    EXPECT_EQ(NX_OK, adc->clear_callback(adc));
}

/**
 * \brief           Test ADC calibration
 */
TEST_F(NxAdcTest, Calibration) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Calibrate */
    EXPECT_EQ(NX_OK, adc->calibrate(adc));
}

/**
 * \brief           Test ADC reference voltage setting
 */
TEST_F(NxAdcTest, ReferenceVoltage) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set reference voltage */
    EXPECT_EQ(NX_OK, adc->set_reference_voltage(adc, 5000)); /* 5V */
}

/**
 * \brief           Test ADC resolution setting
 */
TEST_F(NxAdcTest, ResolutionSetting) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set resolution to 8-bit */
    EXPECT_EQ(NX_OK, adc->set_resolution(adc, NX_ADC_RESOLUTION_8BIT));

    /* Set resolution to 12-bit */
    EXPECT_EQ(NX_OK, adc->set_resolution(adc, NX_ADC_RESOLUTION_12BIT));

    /* Set resolution to 16-bit */
    EXPECT_EQ(NX_OK, adc->set_resolution(adc, NX_ADC_RESOLUTION_16BIT));
}

/**
 * \brief           Test ADC sampling time setting
 */
TEST_F(NxAdcTest, SamplingTimeSetting) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set sampling time */
    EXPECT_EQ(NX_OK, adc->set_sampling_time(adc, NX_ADC_SAMPLING_56_CYCLES));
    EXPECT_EQ(NX_OK, adc->set_sampling_time(adc, NX_ADC_SAMPLING_480_CYCLES));
}

/**
 * \brief           Test ADC configuration get/set
 */
TEST_F(NxAdcTest, ConfigurationGetSet) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set configuration */
    nx_adc_config_t config = {.resolution = NX_ADC_RESOLUTION_10BIT,
                              .sampling_time = NX_ADC_SAMPLING_84_CYCLES,
                              .trigger = NX_ADC_TRIGGER_TIMER,
                              .continuous_mode = true,
                              .dma_enable = true,
                              .channel_count = 3,
                              .channels = nullptr};
    EXPECT_EQ(NX_OK, adc->set_config(adc, &config));

    /* Get configuration */
    nx_adc_config_t read_config = {};
    EXPECT_EQ(NX_OK, adc->get_config(adc, &read_config));
    EXPECT_EQ(NX_ADC_RESOLUTION_10BIT, read_config.resolution);
    EXPECT_EQ(NX_ADC_SAMPLING_84_CYCLES, read_config.sampling_time);
    EXPECT_EQ(NX_ADC_TRIGGER_TIMER, read_config.trigger);
    EXPECT_TRUE(read_config.continuous_mode);
    EXPECT_TRUE(read_config.dma_enable);
    EXPECT_EQ(3u, read_config.channel_count);
}

/**
 * \brief           Test ADC power management
 */
TEST_F(NxAdcTest, PowerManagement) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    nx_power_t* power = adc->get_power(adc);
    ASSERT_NE(nullptr, power);

    /* Power should be enabled after init */
    EXPECT_TRUE(power->is_enabled(power));

    /* Disable power */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));
}

/**
 * \brief           Test ADC suspend and resume
 */
TEST_F(NxAdcTest, SuspendResume) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    nx_power_t* power = adc->get_power(adc);
    ASSERT_NE(nullptr, power);

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_FALSE(power->is_enabled(power));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_TRUE(power->is_enabled(power));
}

/**
 * \brief           Test ADC statistics
 */
TEST_F(NxAdcTest, Statistics) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get initial stats */
    nx_adc_stats_t stats = {};
    EXPECT_EQ(NX_OK, adc->get_stats(adc, &stats));
    EXPECT_FALSE(stats.busy);
    EXPECT_EQ(0u, stats.conversion_count);
    EXPECT_EQ(0u, stats.overrun_count);
    EXPECT_EQ(0u, stats.dma_error_count);

    /* Perform a read to increment conversion count */
    uint16_t value = 0;
    EXPECT_EQ(NX_OK, adc->read(adc, 0, &value));
    EXPECT_EQ(NX_OK, adc->get_stats(adc, &stats));
    EXPECT_EQ(1u, stats.conversion_count);

    /* Clear stats */
    EXPECT_EQ(NX_OK, adc->clear_stats(adc));
    EXPECT_EQ(NX_OK, adc->get_stats(adc, &stats));
    EXPECT_EQ(0u, stats.conversion_count);
}

/**
 * \brief           Test ADC diagnostic interface
 */
TEST_F(NxAdcTest, DiagnosticInterface) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    nx_diagnostic_t* diagnostic = adc->get_diagnostic(adc);
    ASSERT_NE(nullptr, diagnostic);

    /* Get status */
    nx_adc_stats_t stats = {};
    EXPECT_EQ(NX_OK, diagnostic->get_status(diagnostic, &stats, sizeof(stats)));

    /* Get statistics */
    EXPECT_EQ(NX_OK,
              diagnostic->get_statistics(diagnostic, &stats, sizeof(stats)));

    /* Clear statistics */
    EXPECT_EQ(NX_OK, diagnostic->clear_statistics(diagnostic));
}

/**
 * \brief           Test ADC deinitialization
 */
TEST_F(NxAdcTest, Deinit) {
    adc = nx_adc_native_get(0);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_INITIALIZED, lifecycle->get_state(lifecycle));

    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));
}

/**
 * \brief           Test ADC with initial configuration
 */
TEST_F(NxAdcTest, GetWithConfig) {
    nx_adc_config_t config = {.resolution = NX_ADC_RESOLUTION_14BIT,
                              .sampling_time = NX_ADC_SAMPLING_144_CYCLES,
                              .trigger = NX_ADC_TRIGGER_EXTERNAL,
                              .continuous_mode = false,
                              .dma_enable = false,
                              .channel_count = 1,
                              .channels = nullptr};

    adc = nx_adc_native_get_with_config(0, &config);
    ASSERT_NE(nullptr, adc);

    nx_lifecycle_t* lifecycle = adc->get_lifecycle(adc);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify configuration was applied */
    nx_adc_config_t read_config = {};
    EXPECT_EQ(NX_OK, adc->get_config(adc, &read_config));
    EXPECT_EQ(NX_ADC_RESOLUTION_14BIT, read_config.resolution);
    EXPECT_EQ(NX_ADC_SAMPLING_144_CYCLES, read_config.sampling_time);
    EXPECT_EQ(NX_ADC_TRIGGER_EXTERNAL, read_config.trigger);
    EXPECT_FALSE(read_config.continuous_mode);
    EXPECT_FALSE(read_config.dma_enable);
    EXPECT_EQ(1u, read_config.channel_count);
}
