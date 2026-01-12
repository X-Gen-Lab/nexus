/**
 * \file            test_hal_timer.cpp
 * \brief           HAL Timer Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Timer module.
 * Tests timer initialization, start/stop, and PWM configuration.
 * Requirements: 5.1, 5.2, 5.3, 5.6
 */

#include <gtest/gtest.h>

extern "C" {
#include "hal/hal_timer.h"
#include "native_platform.h"
}

/**
 * \brief           Timer Test Fixture
 */
class HalTimerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        native_timer_reset_all();
    }

    void TearDown() override {
        native_timer_reset_all();
    }
};

/**
 * \brief           Test timer initialization
 * \details         Requirements 5.1 - Timer init with valid config
 */
TEST_F(HalTimerTest, InitWithValidConfig) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    EXPECT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));
    EXPECT_TRUE(native_timer_is_initialized(HAL_TIMER_0));
    EXPECT_EQ(1000u, native_timer_get_period_us(HAL_TIMER_0));
    EXPECT_EQ(HAL_TIMER_MODE_PERIODIC, native_timer_get_mode(HAL_TIMER_0));
}

/**
 * \brief           Test timer initialization with oneshot mode
 */
TEST_F(HalTimerTest, InitOneshotMode) {
    hal_timer_config_t config = {.period_us = 5000,
                                 .mode = HAL_TIMER_MODE_ONESHOT,
                                 .direction = HAL_TIMER_DIR_UP};

    EXPECT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_1, &config));
    EXPECT_TRUE(native_timer_is_initialized(HAL_TIMER_1));
    EXPECT_EQ(HAL_TIMER_MODE_ONESHOT, native_timer_get_mode(HAL_TIMER_1));
}

/**
 * \brief           Test timer invalid parameters
 */
TEST_F(HalTimerTest, InitInvalidParams) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    /* Invalid instance */
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM, hal_timer_init(HAL_TIMER_MAX, &config));

    /* Null config */
    EXPECT_EQ(HAL_ERROR_NULL_POINTER, hal_timer_init(HAL_TIMER_0, nullptr));

    /* Zero period */
    hal_timer_config_t zero_config = {.period_us = 0,
                                      .mode = HAL_TIMER_MODE_PERIODIC,
                                      .direction = HAL_TIMER_DIR_UP};
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_timer_init(HAL_TIMER_0, &zero_config));
}

/**
 * \brief           Test timer start
 * \details         Requirements 5.2 - Timer start
 */
TEST_F(HalTimerTest, Start) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));
    EXPECT_FALSE(native_timer_is_running(HAL_TIMER_0));

    EXPECT_EQ(HAL_OK, hal_timer_start(HAL_TIMER_0));
    EXPECT_TRUE(native_timer_is_running(HAL_TIMER_0));
}

/**
 * \brief           Test timer stop
 * \details         Requirements 5.3 - Timer stop preserves count
 */
TEST_F(HalTimerTest, Stop) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));
    ASSERT_EQ(HAL_OK, hal_timer_start(HAL_TIMER_0));
    EXPECT_TRUE(native_timer_is_running(HAL_TIMER_0));

    /* Set a count value */
    ASSERT_EQ(HAL_OK, hal_timer_set_count(HAL_TIMER_0, 500));

    EXPECT_EQ(HAL_OK, hal_timer_stop(HAL_TIMER_0));
    EXPECT_FALSE(native_timer_is_running(HAL_TIMER_0));

    /* Verify count is preserved */
    uint32_t count;
    EXPECT_EQ(HAL_OK, hal_timer_get_count(HAL_TIMER_0, &count));
    EXPECT_EQ(500u, count);
}

/**
 * \brief           Test timer deinit
 */
TEST_F(HalTimerTest, Deinit) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));
    EXPECT_TRUE(native_timer_is_initialized(HAL_TIMER_0));

    EXPECT_EQ(HAL_OK, hal_timer_deinit(HAL_TIMER_0));
    EXPECT_FALSE(native_timer_is_initialized(HAL_TIMER_0));
}

/**
 * \brief           Test operations on uninitialized timer
 */
TEST_F(HalTimerTest, OperationsOnUninitializedTimer) {
    uint32_t count;

    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_start(HAL_TIMER_0));
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_stop(HAL_TIMER_0));
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_get_count(HAL_TIMER_0, &count));
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_set_count(HAL_TIMER_0, 100));
    EXPECT_EQ(HAL_ERROR_NOT_INIT,
              hal_timer_set_callback(HAL_TIMER_0, nullptr, nullptr));
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_timer_deinit(HAL_TIMER_0));
}

/**
 * \brief           Test timer get/set count
 */
TEST_F(HalTimerTest, GetSetCount) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));

    /* Set count */
    EXPECT_EQ(HAL_OK, hal_timer_set_count(HAL_TIMER_0, 12345));

    /* Get count */
    uint32_t count;
    EXPECT_EQ(HAL_OK, hal_timer_get_count(HAL_TIMER_0, &count));
    EXPECT_EQ(12345u, count);
}

/**
 * \brief           Test timer get count with null pointer
 */
TEST_F(HalTimerTest, GetCountNullPointer) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));
    EXPECT_EQ(HAL_ERROR_NULL_POINTER,
              hal_timer_get_count(HAL_TIMER_0, nullptr));
}

/**
 * \brief           Test timer callback registration
 */
static int callback_counter = 0;
static void test_timer_callback(hal_timer_instance_t instance, void* context) {
    (void)instance;
    int* counter = static_cast<int*>(context);
    if (counter) {
        (*counter)++;
    }
    callback_counter++;
}

TEST_F(HalTimerTest, SetCallback) {
    hal_timer_config_t config = {.period_us = 1000,
                                 .mode = HAL_TIMER_MODE_PERIODIC,
                                 .direction = HAL_TIMER_DIR_UP};

    callback_counter = 0;
    int local_counter = 0;

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &config));
    EXPECT_EQ(HAL_OK, hal_timer_set_callback(HAL_TIMER_0, test_timer_callback,
                                             &local_counter));
    EXPECT_EQ(HAL_OK, hal_timer_start(HAL_TIMER_0));

    /* Simulate period elapsed */
    EXPECT_TRUE(native_timer_simulate_period_elapsed(HAL_TIMER_0));
    EXPECT_EQ(1, callback_counter);
    EXPECT_EQ(1, local_counter);
    EXPECT_EQ(1u, native_timer_get_callback_count(HAL_TIMER_0));
}

/**
 * \brief           Test PWM initialization
 * \details         Requirements 5.6 - PWM init with valid config
 */
TEST_F(HalTimerTest, PwmInit) {
    hal_pwm_config_t config = {.frequency = 1000, .duty_cycle = 5000};

    EXPECT_EQ(HAL_OK, hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, &config));
    EXPECT_TRUE(native_pwm_is_initialized(HAL_TIMER_0, HAL_TIMER_CH_1));
    EXPECT_EQ(1000u, native_pwm_get_frequency(HAL_TIMER_0, HAL_TIMER_CH_1));
    EXPECT_EQ(5000u, native_pwm_get_duty_cycle(HAL_TIMER_0, HAL_TIMER_CH_1));
}

/**
 * \brief           Test PWM invalid parameters
 */
TEST_F(HalTimerTest, PwmInitInvalidParams) {
    hal_pwm_config_t config = {.frequency = 1000, .duty_cycle = 5000};

    /* Invalid instance */
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_pwm_init(HAL_TIMER_MAX, HAL_TIMER_CH_1, &config));

    /* Invalid channel */
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_MAX, &config));

    /* Null config */
    EXPECT_EQ(HAL_ERROR_NULL_POINTER,
              hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, nullptr));

    /* Zero frequency */
    hal_pwm_config_t zero_freq = {.frequency = 0, .duty_cycle = 5000};
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, &zero_freq));

    /* Invalid duty cycle (> 10000) */
    hal_pwm_config_t invalid_duty = {.frequency = 1000, .duty_cycle = 10001};
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, &invalid_duty));
}

/**
 * \brief           Test PWM start/stop
 */
TEST_F(HalTimerTest, PwmStartStop) {
    hal_pwm_config_t config = {.frequency = 1000, .duty_cycle = 5000};

    ASSERT_EQ(HAL_OK, hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, &config));
    EXPECT_FALSE(native_pwm_is_running(HAL_TIMER_0, HAL_TIMER_CH_1));

    EXPECT_EQ(HAL_OK, hal_pwm_start(HAL_TIMER_0, HAL_TIMER_CH_1));
    EXPECT_TRUE(native_pwm_is_running(HAL_TIMER_0, HAL_TIMER_CH_1));

    EXPECT_EQ(HAL_OK, hal_pwm_stop(HAL_TIMER_0, HAL_TIMER_CH_1));
    EXPECT_FALSE(native_pwm_is_running(HAL_TIMER_0, HAL_TIMER_CH_1));
}

/**
 * \brief           Test PWM set duty cycle
 */
TEST_F(HalTimerTest, PwmSetDuty) {
    hal_pwm_config_t config = {.frequency = 1000, .duty_cycle = 5000};

    ASSERT_EQ(HAL_OK, hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, &config));

    /* Set duty to 0% */
    EXPECT_EQ(HAL_OK, hal_pwm_set_duty(HAL_TIMER_0, HAL_TIMER_CH_1, 0));
    EXPECT_EQ(0u, native_pwm_get_duty_cycle(HAL_TIMER_0, HAL_TIMER_CH_1));

    /* Set duty to 100% */
    EXPECT_EQ(HAL_OK, hal_pwm_set_duty(HAL_TIMER_0, HAL_TIMER_CH_1, 10000));
    EXPECT_EQ(10000u, native_pwm_get_duty_cycle(HAL_TIMER_0, HAL_TIMER_CH_1));

    /* Set duty to 75% */
    EXPECT_EQ(HAL_OK, hal_pwm_set_duty(HAL_TIMER_0, HAL_TIMER_CH_1, 7500));
    EXPECT_EQ(7500u, native_pwm_get_duty_cycle(HAL_TIMER_0, HAL_TIMER_CH_1));
}

/**
 * \brief           Test PWM set duty invalid value
 */
TEST_F(HalTimerTest, PwmSetDutyInvalid) {
    hal_pwm_config_t config = {.frequency = 1000, .duty_cycle = 5000};

    ASSERT_EQ(HAL_OK, hal_pwm_init(HAL_TIMER_0, HAL_TIMER_CH_1, &config));

    /* Invalid duty cycle (> 10000) */
    EXPECT_EQ(HAL_ERROR_INVALID_PARAM,
              hal_pwm_set_duty(HAL_TIMER_0, HAL_TIMER_CH_1, 10001));
}

/**
 * \brief           Test PWM operations on uninitialized channel
 */
TEST_F(HalTimerTest, PwmOperationsOnUninitializedChannel) {
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_pwm_start(HAL_TIMER_0, HAL_TIMER_CH_1));
    EXPECT_EQ(HAL_ERROR_NOT_INIT, hal_pwm_stop(HAL_TIMER_0, HAL_TIMER_CH_1));
    EXPECT_EQ(HAL_ERROR_NOT_INIT,
              hal_pwm_set_duty(HAL_TIMER_0, HAL_TIMER_CH_1, 5000));
}
