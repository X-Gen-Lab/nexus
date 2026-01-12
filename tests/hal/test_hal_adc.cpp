/**
 * \file            test_hal_adc.cpp
 * \brief           HAL ADC Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/hal_adc.h"
#include "native_platform.h"
}

/**
 * \brief           ADC Test Fixture
 */
class HalAdcTest : public ::testing::Test {
  protected:
    void SetUp() override {
        native_adc_reset_all();
    }

    void TearDown() override {
        native_adc_reset_all();
    }
};

/**
 * \brief           Test ADC initialization
 * \details         Requirements 6.1 - ADC init with valid config should succeed
 */
TEST_F(HalAdcTest, InitWithValidConfig) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    EXPECT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_TRUE(native_adc_is_initialized(HAL_ADC_0));
    EXPECT_EQ(HAL_ADC_RES_12BIT, native_adc_get_resolution(HAL_ADC_0));
}

/**
 * \brief           Test ADC initialization with different resolutions
 * \details         Requirements 6.1 - ADC init should support all resolutions
 */
TEST_F(HalAdcTest, InitWithDifferentResolutions) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_6BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    EXPECT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_ADC_RES_6BIT, native_adc_get_resolution(HAL_ADC_0));
    hal_adc_deinit(HAL_ADC_0);

    config.resolution = HAL_ADC_RES_8BIT;
    EXPECT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_ADC_RES_8BIT, native_adc_get_resolution(HAL_ADC_0));
    hal_adc_deinit(HAL_ADC_0);

    config.resolution = HAL_ADC_RES_10BIT;
    EXPECT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_ADC_RES_10BIT, native_adc_get_resolution(HAL_ADC_0));
}

/**
 * \brief           Test ADC invalid parameters
 * \details         Requirements 6.1 - ADC init with invalid params should fail
 */
TEST_F(HalAdcTest, InitInvalidParams) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    /* Invalid instance */
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_adc_init(HAL_ADC_MAX, &config));

    /* Null config */
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_adc_init(HAL_ADC_0, nullptr));
}

/**
 * \brief           Test ADC double initialization
 * \details         Requirements 6.1 - Double init should fail
 */
TEST_F(HalAdcTest, DoubleInit) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    EXPECT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_ERROR_ALREADY_INIT, hal_adc_init(HAL_ADC_0, &config));
}

/**
 * \brief           Test ADC deinit
 * \details         Requirements 6.1 - ADC deinit should succeed
 */
TEST_F(HalAdcTest, Deinit) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    EXPECT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_OK, hal_adc_deinit(HAL_ADC_0));
    EXPECT_FALSE(native_adc_is_initialized(HAL_ADC_0));
}

/**
 * \brief           Test ADC read single channel
 * \details         Requirements 6.2 - ADC read should return value
 */
TEST_F(HalAdcTest, ReadSingleChannel) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));

    /* Set a known simulated value */
    native_adc_set_simulated_value(HAL_ADC_0, 0, 2048);

    uint16_t value = 0;
    EXPECT_EQ(HAL_OK, hal_adc_read(HAL_ADC_0, 0, &value, 100));
    EXPECT_EQ(2048, value);
}

/**
 * \brief           Test ADC read multiple channels
 * \details         Requirements 6.3 - ADC read_multi should read multiple
 * channels
 */
TEST_F(HalAdcTest, ReadMultipleChannels) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));

    /* Set known simulated values */
    native_adc_set_simulated_value(HAL_ADC_0, 0, 1000);
    native_adc_set_simulated_value(HAL_ADC_0, 1, 2000);
    native_adc_set_simulated_value(HAL_ADC_0, 2, 3000);

    uint8_t channels[] = {0, 1, 2};
    uint16_t values[3] = {0};

    EXPECT_EQ(HAL_OK, hal_adc_read_multi(HAL_ADC_0, channels, values, 3, 100));
    EXPECT_EQ(1000, values[0]);
    EXPECT_EQ(2000, values[1]);
    EXPECT_EQ(3000, values[2]);
}

/**
 * \brief           Test ADC read on uninitialized instance
 * \details         Requirements 6.2 - Read on uninit should fail
 */
TEST_F(HalAdcTest, ReadOnUninitializedInstance) {
    uint16_t value = 0;
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_adc_read(HAL_ADC_0, 0, &value, 100));
}

/**
 * \brief           Test ADC read with null pointer
 * \details         Requirements 6.2 - Read with null pointer should fail
 */
TEST_F(HalAdcTest, ReadNullPointer) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_adc_read(HAL_ADC_0, 0, nullptr, 100));
}

/**
 * \brief           Test ADC read with invalid channel
 * \details         Requirements 6.2 - Read with invalid channel should fail
 */
TEST_F(HalAdcTest, ReadInvalidChannel) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));

    uint16_t value = 0;
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_adc_read(HAL_ADC_0, 16, &value, 100));
}

/**
 * \brief           Test ADC read temperature
 * \details         Requirements 6.5 - Read temperature should return value
 */
TEST_F(HalAdcTest, ReadTemperature) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));

    int16_t temp = 0;
    EXPECT_EQ(HAL_OK, hal_adc_read_temperature(HAL_ADC_0, &temp));
    EXPECT_EQ(25, temp); /* Simulated room temperature */
}

/**
 * \brief           Test ADC read vref
 * \details         Requirements 6.6 - Read vref should return value
 */
TEST_F(HalAdcTest, ReadVref) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));

    uint16_t vref = 0;
    EXPECT_EQ(HAL_OK, hal_adc_read_vref(HAL_ADC_0, &vref));
    EXPECT_EQ(1210, vref); /* Simulated internal reference */
}

/**
 * \brief           Test ADC channel configuration
 * \details         Requirements 6.1 - Channel config should succeed
 */
TEST_F(HalAdcTest, ConfigChannel) {
    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));

    hal_adc_channel_config_t ch_config = {
        .channel = 0, .sample_time = HAL_ADC_SAMPLE_56CYCLES};

    EXPECT_EQ(HAL_OK, hal_adc_config_channel(HAL_ADC_0, &ch_config));
}

/**
 * \brief           Test ADC callback registration
 * \details         Requirements 6.1 - Callback registration should succeed
 */
static bool callback_invoked = false;
static uint16_t callback_value = 0;

static void test_adc_callback(hal_adc_instance_t instance, uint16_t value,
                              void* context) {
    (void)instance;
    (void)context;
    callback_invoked = true;
    callback_value = value;
}

TEST_F(HalAdcTest, CallbackRegistration) {
    callback_invoked = false;
    callback_value = 0;

    hal_adc_config_t config = {.resolution = HAL_ADC_RES_12BIT,
                               .reference = HAL_ADC_REF_VDD,
                               .sample_time = HAL_ADC_SAMPLE_15CYCLES};

    ASSERT_EQ(HAL_OK, hal_adc_init(HAL_ADC_0, &config));
    EXPECT_EQ(HAL_OK,
              hal_adc_set_callback(HAL_ADC_0, test_adc_callback, nullptr));

    /* Set a known simulated value */
    native_adc_set_simulated_value(HAL_ADC_0, 0, 1234);

    uint16_t value = 0;
    EXPECT_EQ(HAL_OK, hal_adc_read(HAL_ADC_0, 0, &value, 100));

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(1234, callback_value);
}
